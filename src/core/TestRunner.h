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

    void run(const QString& binaryPath,
             const QVector<TestCase>& cases,
             const QStringList& extraArgs = {},
             const QString& workingDir = {},
             const QStringList& dependencies = {},
             const QMap<QString, QString>& envVars = {},
             int actualTotal = 0,
             const QVector<TestCase>& expectedTests = {},
             bool singleTest = false);

    void cancel();
    bool isRunning() const;

signals:
    void testFinished(const TestRunResult& result);
    void progressUpdated(int done, int total);
    void allFinished();
    void errorOccurred(const QString& message);
    void rawOutput(const QString& line);

private:
    struct ParsedBlock {
        QString suite;
        QString name;
        QString output;
        double  durationMs = 0;
        QString status;
    };
    struct BatchState {
        QProcess*   process = nullptr;
        QString     accumulatedStdout;
        QString     xmlPath;
        QVector<TestCase> cases;
    };
    QVector<ParsedBlock> parseCombinedOutput(const QString& allOutput);
    void startNextBatch();
    void onBatchFinished(BatchState* batch);
    void safeProgress(int done);

    QVector<BatchState> m_batches;
    int m_nextBatchIdx = 0;
    int m_batchesFinished = 0;
    int m_activeCount = 0;
    static const int MAX_CONCURRENT = 10;
    static const int MAX_FILTER_LEN = 2000;
    bool m_singleTest = false;

    QString         m_binaryPath;
    QString         m_workingDir;
    int             m_totalCount = 0;
    int             m_doneCount  = 0;
    QVector<TestCase> m_expectedTests;
    QSet<QString>    m_seen;
    bool             m_anyCrashed = false;
    QStringList     m_extraArgs;
    QStringList     m_dependencies;
    QMap<QString, QString> m_envVars;
    QElapsedTimer   m_elapsed;
    bool            m_cancelled = false;
    int             m_lastRunCount = 0;
    int             m_lastDoneCount = 0;
    int             m_lastEmittedProgress = 0;
};
