#include "TestRunner.h"

#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include "Logger.h"

TestRunner::TestRunner(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &TestRunner::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &TestRunner::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TestRunner::onProcessFinished);
}

void TestRunner::run(const QString& binaryPath,
                     const QVector<TestCase>& cases,
                     const QStringList& extraArgs,
                     const QString& workingDir,
                     const QStringList& dependencies,
                     const QMap<QString, QString>& envVars)
{
    if (isRunning()) {
        emit errorOccurred("Already running.");
        return;
    }

    m_binaryPath = binaryPath;
    m_workingDir = workingDir;
    m_envVars = envVars;
    m_totalCount = cases.size();
    m_doneCount  = 0;

    if (m_totalCount == 0) { emit allFinished(); return; }

    // 合并 filter: Suite1.Case1:Suite1.Case2:Suite2.Case3
    QStringList filters;
    for (const auto& tc : cases)
        filters << tc.fullName();
    QString filter = filters.join(":");

    m_gtestXmlPath = QDir::tempPath() + "/gtest_batch.xml";
    m_accumulatedStdout.clear();

    QStringList args;
    args << "--gtest_filter=" + filter
         << "--gtest_also_run_disabled_tests"
         << "--gtest_output=xml:" + m_gtestXmlPath
         << extraArgs;

    LOG("RUN", "Batch: " + filter);
    LOG("RUN", "XmlOut: " + m_gtestXmlPath);
    emit rawOutput(QString("▶ Running %1 tests in one batch...\n").arg(m_totalCount));

    // 设置进程环境：PATH 优先包含依赖目录
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList depDirs;
    for (const auto& d : dependencies) {
        if (QFileInfo::exists(d)) {
            depDirs << QDir::toNativeSeparators(QDir(d).absolutePath());
            LOG("RUN", "  dep added: " + d);
        } else {
            LOG("RUN", "  dep NOT FOUND: " + d);
        }
    }
    // 把 exe 目录和依赖目录加到 PATH 前面
    if (!depDirs.isEmpty()) {
        QString newPath = depDirs.join(";") + ";" + env.value("PATH");
        env.insert("PATH", newPath);
        m_process->setProcessEnvironment(env);
    }
    // 自定义环境变量
    for (auto it = m_envVars.begin(); it != m_envVars.end(); ++it) {
        QProcessEnvironment env = m_process->processEnvironment();
        if (env.isEmpty()) env = QProcessEnvironment::systemEnvironment();
        env.insert(it.key(), it.value());
        m_process->setProcessEnvironment(env);
    }
    // 工作目录设到 exe 所在目录
    QFileInfo binInfo(m_binaryPath);
    QString workDir = m_workingDir.isEmpty() ? binInfo.absolutePath() : m_workingDir;
    m_process->setWorkingDirectory(workDir);

    m_elapsed.start();
    m_process->start(m_binaryPath, args);
    if (!m_process->waitForStarted(10000)) {
        LOG("RUN", "Start FAILED: " + m_process->errorString());
        emit errorOccurred("Cannot start: " + m_process->errorString());
        emit allFinished();
    }
}

void TestRunner::cancel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool TestRunner::isRunning() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

void TestRunner::onReadyReadStdout() {
    QString text = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    m_accumulatedStdout += text;
    emit rawOutput(text);
}

void TestRunner::onReadyReadStderr() {
    QString text = QString::fromLocal8Bit(m_process->readAllStandardError());
    emit rawOutput("[STDERR] " + text);
}

void TestRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode);
    Q_UNUSED(status);

    LOG("RUN", "Process exit, stdout size: " + QString::number(m_accumulatedStdout.size()));

    // 1. 解析 XML 获取 RecordProperty（整体）
    QMap<QString, QMap<QString, QString>> allProps;  // fullName → {key→val}
    {
        QFile f(m_gtestXmlPath);
        if (f.open(QIODevice::ReadOnly)) {
            QString xml = QString::fromUtf8(f.readAll());
            f.close(); f.remove();
            LOG("XML", "XML size: " + QString::number(xml.size()) + " bytes");
            LOG("XML", "XML preview: " + xml.left(300));
            // 每个 testcase 的 properties
            // <testcase name="Case1" classname="Suite1"> ... <property name="k" value="v"/> ...
            QRegularExpression tcRe(
                R"tc(<testcase\s+name="([^"]+)"[^>]*classname="([^"]+)"[^>]*>)tc"
                R"tc((.*?)</testcase>)tc",
                QRegularExpression::DotMatchesEverythingOption);
            QRegularExpression propRe(
                R"pr(<property name="([^"]+)" value="([^"]+)"/?>)pr",
                QRegularExpression::DotMatchesEverythingOption);
            auto tcIt = tcRe.globalMatch(xml);
            while (tcIt.hasNext()) {
                auto m = tcIt.next();
                QString caseName = m.captured(1);
                QString suiteName = m.captured(2);
                QString body = m.captured(3);
                QString full = suiteName + "." + caseName;
                auto pIt = propRe.globalMatch(body);
                while (pIt.hasNext()) {
                    auto pm = pIt.next();
                    QString key = pm.captured(1);
                    QString val = pm.captured(2);
                    allProps[full][key] = val;
                    LOG("PROP", full, key + " = " + val);
                }
            }
            LOG("XML", "Parsed " + QString::number(allProps.size()) + " testcases with properties");
        } else {
            LOG("XML", "No XML: " + m_gtestXmlPath);
        }
    }

    // 2. 按 [RUN] 切分 stdout 得到每个用例的输出
    auto blocks = parseCombinedOutput(m_accumulatedStdout);
    LOG("RUN", "Parsed " + QString::number(blocks.size()) + " test blocks from stdout");

    // 3. 逐个发射结果
    for (const auto& b : blocks) {
        TestRunResult res;
        res.testCase.suiteName = b.suite;
        res.testCase.caseName  = b.name;
        res.status    = b.status;
        res.durationMs = b.durationMs;
        res.rawStdout = b.output;
        res.properties = allProps.value(res.testCase.fullName());

        m_doneCount++;
        emit testFinished(res);
        emit progressUpdated(m_doneCount, m_totalCount);
    }

    emit allFinished();
}

QVector<TestRunner::ParsedBlock> TestRunner::parseCombinedOutput(const QString& allOutput) {
    QVector<ParsedBlock> blocks;

    // 按 [ RUN      ] Suite.Case 切分
    // [^ \r\n]+ = 匹配除空格/换行/回车外的任何字符（停在行尾）
    static QRegularExpression splitRe(R"(\[ RUN      \] ([^ \r\n]+)\.([^ \r\n]+))");
    // 状态行
    static QRegularExpression okRe(R"(\[       OK \] [^ ]+ \((\d+) ms\))");
    static QRegularExpression failRe(R"(\[  FAILED  \] [^ ]+ \((\d+) ms\))");

    // 找到所有 [RUN] 位置
    auto it = splitRe.globalMatch(allOutput);
    int lastPos = 0;
    QString lastSuite, lastName;

    while (it.hasNext()) {
        auto m = it.next();
        int pos = m.capturedStart();

        // 如果有上一个用例，结算它
        if (!lastSuite.isEmpty()) {
            ParsedBlock block;
            block.suite = lastSuite;
            block.name  = lastName;
            block.output = allOutput.mid(lastPos, pos - lastPos);
            // 解析状态
            auto okM = okRe.match(block.output);
            auto fM  = failRe.match(block.output);
            if (okM.hasMatch()) {
                block.status = "PASSED";
                block.durationMs = okM.captured(1).toDouble();
            } else if (fM.hasMatch()) {
                block.status = "FAILED";
                block.durationMs = fM.captured(1).toDouble();
            } else {
                block.status = "ERROR";
            }
            blocks.append(block);
        }

        lastSuite = m.captured(1).remove('\r').remove('\n').trimmed();
        lastName  = m.captured(2).remove('\r').remove('\n').trimmed();
        lastPos   = pos;
    }

    // 最后一个用例
    if (!lastSuite.isEmpty()) {
        ParsedBlock block;
        block.suite = lastSuite;
        block.name  = lastName;
        block.output = allOutput.mid(lastPos);
        auto okM = okRe.match(block.output);
        auto fM  = failRe.match(block.output);
        if (okM.hasMatch()) {
            block.status = "PASSED";
            block.durationMs = okM.captured(1).toDouble();
        } else if (fM.hasMatch()) {
            block.status = "FAILED";
            block.durationMs = fM.captured(1).toDouble();
        } else {
            block.status = "ERROR";
        }
        blocks.append(block);
    }

    return blocks;
}
