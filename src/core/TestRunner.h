#pragma once

#include "models/TestResult.h"

#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QStringList>

// ────────────────────────────────────────────────────────────
//  运行测试用例，实时发射信号
// ────────────────────────────────────────────────────────────
class TestRunner : public QObject {
    Q_OBJECT
public:
    explicit TestRunner(QObject* parent = nullptr);

    // 开始运行指定用例
    void run(const QString& binaryPath,
             const QVector<TestCase>& cases,
             const QStringList& extraArgs = {},
             const QString& workingDir = {});

    // 取消当前运行
    void cancel();

    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }

signals:
    // 每跑完一个用例
    void testFinished(const TestRunResult& result);

    // 整体进度
    void progressUpdated(int done, int total);

    // 全部完成
    void allFinished();

    // 错误
    void errorOccurred(const QString& message);

    // 原始输出行（用于实时日志）
    void rawOutput(const QString& line);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode);

private:
    void parseNextTest();

    QProcess*       m_process = nullptr;
    QElapsedTimer   m_elapsed;

    QString         m_binaryPath;
    QString         m_workingDir;
    QStringList     m_extraArgs;
    QVector<TestCase> m_pendingCases;
    int             m_doneCount = 0;
    int             m_totalCount = 0;

    // 当前正在累积输出的用例
    TestRunResult   m_currentResult;
    bool            m_inTest = false;
    QString         m_accumulatedStdout;
    QString         m_gtestXmlPath;

    // 解析 gtest XML 中的 RecordProperty
    void parseGtestXmlProperties(QMap<QString, QString>& props);
};
