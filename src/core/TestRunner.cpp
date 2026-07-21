#include "TestRunner.h"

#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include "Logger.h"

TestRunner::TestRunner(QObject* parent)
    : QObject(parent)
{
}

void TestRunner::run(const QString& binaryPath,
                     const QVector<TestCase>& cases,
                     const QStringList& extraArgs,
                     const QString& workingDir,
                     const QStringList& dependencies,
                     const QMap<QString, QString>& envVars,
                     int actualTotal,
                     const QVector<TestCase>& expectedTests,
                     bool singleTest)
{
    if (isRunning()) {
        emit errorOccurred("Already running.");
        return;
    }

    m_binaryPath = binaryPath;
    m_workingDir = workingDir;
    m_envVars = envVars;
    m_extraArgs = extraArgs;
    m_dependencies = dependencies;
    m_expectedTests = expectedTests.isEmpty() ? cases : expectedTests;
    m_singleTest = singleTest;
    m_totalCount = qMax(actualTotal, m_expectedTests.size());
    m_doneCount = 0;
    m_nextBatchIdx = 0;
    m_batchesFinished = 0;
    m_activeCount = 0;
    m_cancelled = false;
    m_batches.clear();
    m_seen.clear();
    m_anyCrashed = false;
    m_lastRunCount = 0;
    m_lastDoneCount = 0;
    m_lastEmittedProgress = 0;

    if (m_totalCount == 0) { emit allFinished(); return; }

    // 按 2000 字符拆批
    auto makeSeg = [](const TestCase& tc) -> QString {
        if (tc.caseName == "*") return tc.suiteName + ".*";
        return tc.fullName();
    };

    QVector<QVector<TestCase>> rawBatches;
    QVector<TestCase> batch;
    int batchLen = 0;
    auto flush = [&]() {
        if (!batch.isEmpty()) { rawBatches.append(batch); batch.clear(); batchLen = 0; }
    };
    for (const auto& tc : cases) {
        QString seg = makeSeg(tc);
        int add = seg.length() + (batchLen > 0 ? 1 : 0);
        int limit = m_singleTest ? 0 : MAX_FILTER_LEN;  // 逐个模式：1用例1批
        if (batchLen > 0 && batchLen + add > limit) flush();
        batch.append(tc);
        batchLen += add;
    }
    flush();

    LOG("RUN", QString("Split %1 tests into %2 batches").arg(cases.size()).arg(rawBatches.size()));

    // 构造 BatchState
    for (int i = 0; i < rawBatches.size(); i++) {
        BatchState bs;
        bs.cases = rawBatches[i];
        bs.xmlPath = QDir::toNativeSeparators(QDir::tempPath() + QString("/gtest_batch_%1.xml").arg(i));
        m_batches.append(bs);
    }

    // 启动最多 MAX_CONCURRENT 批
    for (int i = 0; i < qMin(MAX_CONCURRENT, m_batches.size()); i++)
        startNextBatch();
}

void TestRunner::startNextBatch() {
    if (m_cancelled || m_nextBatchIdx >= m_batches.size()) return;

    BatchState* batch = &m_batches[m_nextBatchIdx];
    int batchIdx = m_nextBatchIdx;
    m_nextBatchIdx++;
    m_activeCount++;

    // 构造 filter
    QStringList filters;
    for (const auto& tc : batch->cases) {
        if (tc.suiteName == "*") { filters = {"*"}; break; }
        if (tc.caseName == "*") filters << tc.suiteName + ".*";
        else filters << tc.fullName();
    }
    QString filter = filters.join(":");

    batch->accumulatedStdout.clear();
    batch->process = new QProcess(this);

    QStringList args;
    args << "--gtest_filter=" + filter
         << "--gtest_output=xml:" + batch->xmlPath
         << m_extraArgs;

    LOG("RUN", QString("Start batch %1/%2: %3 tests, filter=%4 chars")
        .arg(batchIdx+1).arg(m_batches.size()).arg(batch->cases.size()).arg(filter.length()));

    // stdout
    connect(batch->process, &QProcess::readyReadStandardOutput, this, [this, batch]() {
        QString text = QString::fromLocal8Bit(batch->process->readAllStandardOutput());
        batch->accumulatedStdout += text;
        emit rawOutput(text);
        // 实时进度：遍历所有运行中批次的 stdout 统计已完成数
        // 只统计单独的用例结果行 (Suite.Test)，排除 "N tests." 等汇总行
        static QRegularExpression doneRe(R"(\[       OK \] [A-Za-z_]|\[  FAILED  \] [A-Za-z_]|\[  SKIPPED \] [A-Za-z_])");
        int total = 0;
        for (const auto& b : m_batches) {
            auto it = doneRe.globalMatch(b.accumulatedStdout);
            while (it.hasNext()) { it.next(); total++; }
        }
        if (total != m_lastDoneCount) {
            m_lastDoneCount = total;
            safeProgress(total);
        }
    });
    // stderr
    connect(batch->process, &QProcess::readyReadStandardError, this, [this, batch]() {
        emit rawOutput("[STDERR] " + QString::fromLocal8Bit(batch->process->readAllStandardError()));
    });
    // finished
    connect(batch->process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, batch]() { onBatchFinished(batch); });

    // 环境变量
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList depDirs;
    if (m_dependencies.isEmpty()) {
        LOG("RUN", "  no deps configured");
    }
    for (const auto& d : m_dependencies) {
        if (QFileInfo::exists(d)) {
            depDirs << QDir::toNativeSeparators(QDir(d).absolutePath());
            LOG("RUN", "  dep added: " + d);
        } else {
            LOG("RUN", "  dep NOT FOUND: " + d);
        }
    }
    if (!depDirs.isEmpty()) {
        QString newPath = depDirs.join(";") + ";" + env.value("PATH");
        env.insert("PATH", newPath);
        LOG("RUN", "  PATH updated with " + QString::number(depDirs.size()) + " dep dir(s)");
    }
    // 自定义环境变量
    for (auto it = m_envVars.begin(); it != m_envVars.end(); ++it) {
        env.insert(it.key(), it.value());
        LOG("RUN", "  env: " + it.key() + "=" + it.value());
    }
    batch->process->setProcessEnvironment(env);
    QFileInfo binInfo(m_binaryPath);
    batch->process->setWorkingDirectory(m_workingDir.isEmpty() ? binInfo.absolutePath() : m_workingDir);

    LOG("RUN", QString("Batch %1 started, XML: %2").arg(batchIdx+1).arg(batch->xmlPath));
    m_elapsed.start();
    batch->process->start(m_binaryPath, args);
    if (!batch->process->waitForStarted(10000)) {
        LOG("RUN", QString("Batch %1 FAILED to start: %2").arg(batchIdx+1).arg(batch->process->errorString()));
        emit errorOccurred(QString("Batch %1 cannot start: %2").arg(batchIdx+1).arg(batch->process->errorString()));
        m_anyCrashed = true;
        onBatchFinished(batch);
    }
}

void TestRunner::onBatchFinished(BatchState* batch) {
    if (m_cancelled) return;

    LOG("RUN", "Batch done, stdout: " + QString::number(batch->accumulatedStdout.size()) + " bytes");
    auto exitStatus = batch->process ? batch->process->exitStatus() : QProcess::NormalExit;
    auto exitCode   = batch->process ? batch->process->exitCode() : -1;
    LOG("RUN", QString("Batch exit: code=%1  status=%2")
        .arg(exitCode)
        .arg(exitStatus == QProcess::CrashExit ? "CRASHED" : "normal"));
    if (exitStatus == QProcess::CrashExit || exitCode < 0)
        m_anyCrashed = true;
    bool batchFailed = (exitStatus == QProcess::CrashExit) || (exitCode < 0);

    // 解析 XML
    QMap<QString, QMap<QString, QString>> allProps;
    {
        QFile f(batch->xmlPath);
        if (f.open(QIODevice::ReadOnly)) {
            QString xml = QString::fromUtf8(f.readAll());
            f.close(); f.remove();
            LOG("RUN", "XML size: " + QString::number(xml.size()) + " bytes");
            xml.replace(QRegularExpression("<testcase[^>]*/>"), "");
            QRegularExpression tcRe1(
                R"tc(<testcase\s+name="([^"]+)"[^>]*classname="([^"]+)"[^>]*>(.*?)</testcase>)tc",
                QRegularExpression::DotMatchesEverythingOption);
            QRegularExpression tcRe2(
                R"tc(<testcase\s+classname="([^"]+)"[^>]*name="([^"]+)"[^>]*>(.*?)</testcase>)tc",
                QRegularExpression::DotMatchesEverythingOption);
            QRegularExpression propRe(
                R"pr(<property name="([^"]+)" value="([^"]+)"/?>)pr",
                QRegularExpression::DotMatchesEverythingOption);
            auto parseXmlTc = [&](const QRegularExpression& re) {
                auto it = re.globalMatch(xml);
                while (it.hasNext()) {
                    auto m = it.next();
                    QString full = m.captured(2) + "." + m.captured(1);
                    auto pIt = propRe.globalMatch(m.captured(3));
                    while (pIt.hasNext()) {
                        auto pm = pIt.next();
                        allProps[full][pm.captured(1)] = pm.captured(2);
                    }
                }
            };
            parseXmlTc(tcRe1);
            parseXmlTc(tcRe2);
        } else {
            LOG("RUN", "XML file not found or unreadable: " + batch->xmlPath);
        }
    }
    if (!allProps.isEmpty())
        LOG("RUN", "XML properties found for " + QString::number(allProps.size()) + " testcases");
    else
        LOG("RUN", "XML contains no properties");

    // 解析 stdout
    auto blocks = parseCombinedOutput(batch->accumulatedStdout);

    // 崩溃定位：打印最后一个输出块的用例
    if (batchFailed && !blocks.isEmpty()) {
        const auto& last = blocks.last();
        LOG("RUN", QString("CRASH: last seen test = %1.%2  [%3]")
            .arg(last.suite).arg(last.name).arg(last.status));
    }

    // 发射结果（使用成员 m_seen 避免跨批次重复）
    for (const auto& b : blocks) {
        TestRunResult res;
        res.testCase.suiteName = b.suite;
        res.testCase.caseName  = b.name;
        res.status    = b.status;
        res.durationMs = b.durationMs;
        res.rawStdout = b.output;
        res.properties = allProps.value(res.testCase.fullName());
        m_seen.insert(res.testCase.fullName());
        m_doneCount++;
        emit testFinished(res);
    }
    for (auto it = allProps.begin(); it != allProps.end(); ++it) {
        if (m_seen.contains(it.key()) || it.value().isEmpty()) continue;
        int dot = it.key().lastIndexOf('.');
        if (dot < 0) continue;
        TestRunResult res;
        res.testCase.suiteName = it.key().left(dot);
        res.testCase.caseName  = it.key().mid(dot + 1);
        res.status = "SKIPPED";
        res.properties = it.value();
        m_doneCount++;
        m_seen.insert(it.key());
        emit testFinished(res);
    }

    // 进度
    safeProgress(m_doneCount);

    // 清理
    batch->process->deleteLater();
    batch->process = nullptr;
    m_activeCount--;
    m_batchesFinished++;

    // 启动下一批
    startNextBatch();

    // 全部完成时才处理未出现的用例（避免跨批次重复计数）
    if (m_batchesFinished >= m_batches.size()) {
        for (const auto& tc : m_expectedTests) {
            QString full = tc.fullName();
            if (tc.caseName == "*" || m_seen.contains(full)) continue;
            TestRunResult res;
            res.testCase = tc;
            res.status = m_anyCrashed ? "CRASHED" : "SKIPPED";
            m_doneCount++;
            m_seen.insert(full);
            emit testFinished(res);
        }
        safeProgress(m_totalCount);
        emit allFinished();
    }
}

void TestRunner::safeProgress(int done) {
    done = qMin(done, m_totalCount);
    if (done > m_lastEmittedProgress) {
        m_lastEmittedProgress = done;
        emit progressUpdated(done, m_totalCount);
    }
}

void TestRunner::cancel() {
    m_cancelled = true;
    for (auto& b : m_batches) {
        if (b.process) {
            if (b.process->state() != QProcess::NotRunning)
                b.process->kill();
            b.process->deleteLater();
            b.process = nullptr;
        }
    }
    // 重置全部计数器，让 isRunning() 返回 false，UI 恢复可用
    m_activeCount = 0;
    m_nextBatchIdx = m_batches.size();
    m_batchesFinished = m_batches.size();
    m_doneCount = 0;
    m_lastRunCount = 0;
    m_lastDoneCount = 0;
    m_lastEmittedProgress = 0;

    emit allFinished();
}

bool TestRunner::isRunning() const {
    return m_activeCount > 0 || m_nextBatchIdx < m_batches.size();
}

QVector<TestRunner::ParsedBlock> TestRunner::parseCombinedOutput(const QString& allOutput) {
    QVector<ParsedBlock> blocks;
    static QRegularExpression splitRe(R"(\[ RUN      \] ([^ \r\n]+)\.([^ \r\n]+))");
    static QRegularExpression okRe(R"(\[       OK \] [^ ]+ \((\d+) ms\))");
    static QRegularExpression failRe(R"(\[  FAILED  \] [^ ]+ \((\d+) ms\))");
    static QRegularExpression skipRe(R"(\[  SKIPPED \] [^ ]+ \((\d+) ms\))");

    auto it = splitRe.globalMatch(allOutput);
    int lastPos = 0;
    QString lastSuite, lastName;
    while (it.hasNext()) {
        auto m = it.next();
        int pos = m.capturedStart();
        if (!lastSuite.isEmpty()) {
            ParsedBlock block;
            block.suite = lastSuite;
            block.name  = lastName;
            block.output = allOutput.mid(lastPos, pos - lastPos);
            auto okM = okRe.match(block.output);
            auto fM  = failRe.match(block.output);
            auto sM  = skipRe.match(block.output);
            if (okM.hasMatch())      { block.status = "PASSED"; block.durationMs = okM.captured(1).toDouble(); }
            else if (fM.hasMatch())  { block.status = "FAILED"; block.durationMs = fM.captured(1).toDouble(); }
            else if (sM.hasMatch())  { block.status = "SKIPPED"; block.durationMs = sM.captured(1).toDouble(); }
            else                     { block.status = "ERROR"; }
            blocks.append(block);
        }
        lastSuite = m.captured(1).remove('\r').remove('\n').trimmed();
        lastName  = m.captured(2).remove('\r').remove('\n').trimmed();
        lastPos   = pos;
    }
    if (!lastSuite.isEmpty()) {
        ParsedBlock block;
        block.suite = lastSuite;
        block.name  = lastName;
        block.output = allOutput.mid(lastPos);
        auto okM = okRe.match(block.output);
        auto fM  = failRe.match(block.output);
        auto sM  = skipRe.match(block.output);
        if (okM.hasMatch())      { block.status = "PASSED"; block.durationMs = okM.captured(1).toDouble(); }
        else if (fM.hasMatch())  { block.status = "FAILED"; block.durationMs = fM.captured(1).toDouble(); }
        else if (sM.hasMatch())  { block.status = "SKIPPED"; block.durationMs = sM.captured(1).toDouble(); }
        else                     { block.status = "ERROR"; }
        blocks.append(block);
    }
    return blocks;
}
