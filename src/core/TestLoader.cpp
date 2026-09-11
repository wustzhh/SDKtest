#include "TestLoader.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTextStream>

#include "Logger.h"

TestLoader::TestLoader() {}

namespace {

bool isValidGTestName(const QString& name) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_/-]*$"));
    return pattern.match(name).hasMatch();
}

QString withoutComment(const QString& line) {
    const int hash = line.indexOf('#');
    return (hash >= 0 ? line.left(hash) : line).trimmed();
}

}

QVector<TestCase> TestLoader::parseGTestListTests(const QString& output) {
    QVector<TestCase> cases;
    QString currentSuite;
    QString text = output;
    QTextStream stream(&text);
    QString line;
    while (stream.readLineInto(&line)) {
        if (line.trimmed().isEmpty()) continue;

        const bool indented = line.startsWith(' ') || line.startsWith('\t');
        const QString content = withoutComment(line.trimmed());
        if (!indented) {
            if (content.endsWith('.')) {
                const QString suite = content.left(content.size() - 1);
                currentSuite = isValidGTestName(suite) ? suite : QString();
            } else {
                // Non-indented text that is not a suite header is output from
                // the test binary, not a test case.
                currentSuite.clear();
            }
            continue;
        }

        if (currentSuite.isEmpty() || !isValidGTestName(content)) continue;

        TestCase tc;
        tc.suiteName = currentSuite;
        tc.caseName = content;
        cases.append(tc);
    }
    return cases;
}

bool TestLoader::load(const QString& binaryPath, const QStringList& extraArgs,
                       const QString& workingDir, const QStringList& dependencies,
                       const QMap<QString, QString>& envVars) {
    m_cases.clear();

    QProcess proc;
    QFileInfo binInfo(binaryPath);
    QString workDir = workingDir.isEmpty() ? binInfo.absolutePath() : workingDir;
    if (!workDir.isEmpty())
        proc.setWorkingDirectory(workDir);

    if (!dependencies.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QStringList depDirs;
        for (const auto& d : dependencies) {
            if (QFileInfo::exists(d)) {
                depDirs << QDir::toNativeSeparators(QDir(d).absolutePath());
                LOG("LDR", "  dep added: " + d);
            } else {
                LOG("LDR", "  dep NOT FOUND: " + d);
            }
        }
        if (!depDirs.isEmpty()) {
            env.insert("PATH", depDirs.join(';') + ";" + env.value("PATH"));
            proc.setProcessEnvironment(env);
        }
    } else {
        LOG("LDR", "  no deps configured");
    }

    for (auto it = envVars.begin(); it != envVars.end(); ++it) {
        QProcessEnvironment env = proc.processEnvironment();
        if (env.isEmpty()) env = QProcessEnvironment::systemEnvironment();
        env.insert(it.key(), it.value());
        proc.setProcessEnvironment(env);
        LOG("LDR", "  env: " + it.key() + "=" + it.value());
    }

    QStringList args;
    args << "--gtest_list_tests" << extraArgs;

    LOG("LDR", "Starting", binaryPath);
    LOG("LDR", "Args", args.join(" "));

    proc.start(binaryPath, args);
    if (!proc.waitForStarted(5000)) {
        const QString err = proc.errorString();
        LOG("LDR", "START FAILED", err);
        m_lastError = QString("Cannot start '%1': %2").arg(binaryPath, err);
        return false;
    }
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        m_lastError = QString("Timeout listing tests from '%1'").arg(binaryPath);
        return false;
    }

    const QString errOut = QString::fromLocal8Bit(proc.readAllStandardError());
    const QString output = QString::fromLocal8Bit(proc.readAllStandardOutput());

    LOG("LDR", "Exit code: " + QString::number(proc.exitCode()));
    LOG("LDR", "Stdout size: " + QString::number(output.size()) + " bytes");
    if (!errOut.isEmpty())
        LOG("LDR", "Stderr: " + errOut.left(500));
    LOG("LDR", "Stdout preview: " + output.left(300));

    m_cases = parseGTestListTests(output);
    if (m_cases.isEmpty()) {
        m_lastError = "No test cases found. Output:\n" + output;
        return false;
    }
    return true;
}

QMap<QString, QVector<TestCase>> TestLoader::groupedBySuite() const {
    QMap<QString, QVector<TestCase>> groups;
    for (const auto& tc : m_cases)
        groups[tc.suiteName].append(tc);
    return groups;
}
