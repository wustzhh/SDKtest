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
                     const QString& workingDir)
{
    if (isRunning()) {
        emit errorOccurred("A test run is already in progress.");
        return;
    }

    m_binaryPath  = binaryPath;
    m_extraArgs   = extraArgs;
    m_workingDir  = workingDir;
    m_pendingCases = cases;
    m_doneCount   = 0;
    m_totalCount  = cases.size();
    m_inTest      = false;

    if (m_totalCount == 0) {
        emit allFinished();
        return;
    }

    m_elapsed.start();
    parseNextTest();
}

void TestRunner::cancel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    m_pendingCases.clear();
}

void TestRunner::parseNextTest() {
    if (m_pendingCases.isEmpty()) {
        emit allFinished();
        return;
    }

    TestCase tc = m_pendingCases.takeFirst();

    // 构建 filter
    // 如果有多个 pending，分批运行——这里一次只跑一个用例
    // 但为了效率可以把相邻的同 suite 合并，先跑单个
    QString filter = tc.fullName();

    // 重置当前累积
    m_currentResult = TestRunResult{};
    m_currentResult.testCase = tc;
    m_currentResult.status = "RUNNING";
    m_accumulatedStdout.clear();
    m_inTest = true;

    // gtest XML output for RecordProperty
    m_gtestXmlPath = QDir::tempPath() + "/gtest_out_" + QString::number(m_doneCount) + ".xml";

    QStringList args;
    args << "--gtest_filter=" + filter
         << "--gtest_also_run_disabled_tests"
         << "--gtest_output=xml:" + m_gtestXmlPath
         << m_extraArgs;

    emit rawOutput(QString("▶ [%1/%2] %3\n")
                       .arg(m_doneCount + 1)
                       .arg(m_totalCount)
                       .arg(tc.fullName()));

    LOG("RUN", "Start: " + m_binaryPath + "  filter=" + filter);
    LOG("RUN", "XmlOut: " + m_gtestXmlPath);
    LOG("RUN", "WorkDir: " + m_workingDir);

    if (!m_workingDir.isEmpty())
        m_process->setWorkingDirectory(m_workingDir);
    m_process->start(m_binaryPath, args);
    if (!m_process->waitForStarted(5000)) {
        QString err = m_process->errorString();
        LOG("RUN", "START FAILED: " + err);
        m_currentResult.status = "ERROR";
        m_currentResult.rawStderr = err;
        m_inTest = false;
        m_doneCount++;
        emit testFinished(m_currentResult);
        emit progressUpdated(m_doneCount, m_totalCount);
        parseNextTest();
    } else {
        LOG("RUN", "Started OK");
    }
}

void TestRunner::onReadyReadStdout() {
    QByteArray data = m_process->readAllStandardOutput();
    QString text = QString::fromLocal8Bit(data);
    m_accumulatedStdout += text;
    emit rawOutput(text);
}

void TestRunner::onReadyReadStderr() {
    QString text = QString::fromLocal8Bit(m_process->readAllStandardError());
    m_currentResult.rawStderr += text;
    emit rawOutput("[STDERR] " + text);
}

void TestRunner::onProcessFinished(int exitCode) {
    LOG("RUN", "Exit code: " + QString::number(exitCode));

    // 解析当前用例的 gtest 输出
    static QRegularExpression okRe(R"(\[       OK \] (.+?) \((\d+) ms\))");
    static QRegularExpression failRe(R"(\[  FAILED  \] (.+?)(?: \((\d+) ms\))?)");

    m_currentResult.rawStdout = m_accumulatedStdout;

    auto matchOK   = okRe.match(m_accumulatedStdout);
    auto matchFail = failRe.match(m_accumulatedStdout);

    if (matchOK.hasMatch()) {
        m_currentResult.status = "PASSED";
        m_currentResult.durationMs = matchOK.captured(2).toDouble();
    } else if (matchFail.hasMatch()) {
        m_currentResult.status = "FAILED";
        m_currentResult.durationMs = matchFail.captured(2).toDouble();
    } else if (m_inTest) {
        // 进程异常退出，未产生有效 gtest 结果
        m_currentResult.status = "ERROR";
        m_currentResult.durationMs = m_elapsed.elapsed();
    }

    // 从 gtest XML 提取 RecordProperty
    parseGtestXmlProperties(m_currentResult.properties);

    m_inTest = false;
    m_doneCount++;

    emit testFinished(m_currentResult);
    emit progressUpdated(m_doneCount, m_totalCount);

    // 继续下一个
    parseNextTest();
}

void TestRunner::parseGtestXmlProperties(QMap<QString, QString>& props) {
    QFile file(m_gtestXmlPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG("XML", "No XML file: " + m_gtestXmlPath);
        return;
    }
    LOG("XML", "Parsing: " + m_gtestXmlPath);

    QString xml = QString::fromUtf8(file.readAll());
    file.close();
    file.remove();
    LOG("XML", "File size: " + QString::number(xml.size()) + " bytes");
    LOG("XML", "Content: " + xml.left(1000));

    // 解析 <property name="..." value="..."/>
    // gtest XML 格式: <property name="interface" value="SearchBoss"/>
    static QRegularExpression propRe(
        R"xml(<property name="([^"]+)" value="([^"]+)"/?>)xml");
    auto it = propRe.globalMatch(xml);
    while (it.hasNext()) {
        auto m = it.next();
        props[m.captured(1)] = m.captured(2);
    }
    LOG("XML", "Found " + QString::number(props.size()) + " properties");
    for (auto i = props.begin(); i != props.end(); ++i)
        LOG("XML", "  " + i.key() + " = " + i.value());
}
