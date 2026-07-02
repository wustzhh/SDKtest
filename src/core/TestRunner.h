#pragma once

#include "models/TestResult.h"

#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QStringList>
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>

class TestRunner : public QObject {
    Q_OBJECT
public:
    explicit TestRunner(QObject* parent = nullptr);

    // 一次跑完所有选中用例（合并 --gtest_filter）
    void run(const QString& binaryPath,
             const QVector<TestCase>& cases,
             const QStringList& extraArgs = {},
             const QString& workingDir = {},
             const QStringList& dependencies = {},
             const QMap<QString, QString>& envVars = {});

    void cancel();
    bool isRunning() const;

signals:
    void testFinished(const TestRunResult& result);
    void progressUpdated(int done, int total);
    void allFinished();
    void errorOccurred(const QString& message);
    void rawOutput(const QString& line);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    // 从合并的 stdout 中切分每个用例的输出
    struct ParsedBlock {
        QString suite;
        QString name;
        QString output;
        double  durationMs = 0;
        QString status;
    };
    QVector<ParsedBlock> parseCombinedOutput(const QString& allOutput);

    // 解析 gtest XML (RecordProperty)
    void parseXmlProperties(const QString& xmlPath, QMap<QString, QString>& props);

    QProcess*       m_process = nullptr;
    QElapsedTimer   m_elapsed;

    QString         m_binaryPath;
    QString         m_workingDir;
    int             m_totalCount = 0;
    int             m_doneCount  = 0;

    QString         m_accumulatedStdout;
    QString         m_gtestXmlPath;
    QMap<QString, QString> m_envVars;
};
