#include "TestLoader.h"

#include <QProcess>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include "Logger.h"

TestLoader::TestLoader() {}

bool TestLoader::load(const QString& binaryPath, const QStringList& extraArgs,
                       const QString& workingDir, const QStringList& dependencies,
                       const QMap<QString, QString>& envVars) {
    m_cases.clear();

    QProcess proc;
    QFileInfo binInfo(binaryPath);
    QString workDir = workingDir.isEmpty() ? binInfo.absolutePath() : workingDir;
    if (!workDir.isEmpty())
        proc.setWorkingDirectory(workDir);

    // 设置进程环境：PATH 优先包含依赖目录
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
            QString newPath = depDirs.join(";") + ";" + env.value("PATH");
            env.insert("PATH", newPath);
            proc.setProcessEnvironment(env);
        }
    } else {
        LOG("LDR", "  no deps configured");
    }
    // 自定义环境变量
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
        QString err = proc.errorString();
        LOG("LDR", "START FAILED", err);
        m_lastError = QString("Cannot start '%1': %2").arg(binaryPath, err);
        return false;
    }
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        m_lastError = QString("Timeout listing tests from '%1'").arg(binaryPath);
        return false;
    }

    QString errOut = QString::fromLocal8Bit(proc.readAllStandardError());
    QString output = QString::fromLocal8Bit(proc.readAllStandardOutput());

    LOG("LDR", "Exit code: " + QString::number(proc.exitCode()));
    LOG("LDR", "Stdout size: " + QString::number(output.size()) + " bytes");
    if (!errOut.isEmpty())
        LOG("LDR", "Stderr: " + errOut.left(500));
    LOG("LDR", "Stdout preview: " + output.left(300));

    // ── 解析 gtest_list_tests 输出 ──
    //    TestSuite.
    //      TestCase1
    //      TestCase2
    //    TestSuite2.
    //      TestCase1  # comment
    QString currentSuite;
    QTextStream stream(&output);
    QString line;
    while (stream.readLineInto(&line)) {
        // 跳过空行和全局行
        if (line.trimmed().isEmpty()) continue;

        // 套间行：以 "." 结尾且不缩进
        if (!line.startsWith(' ') && !line.startsWith('\t') && line.endsWith('.')) {
            currentSuite = line.trimmed();
            if (currentSuite.endsWith('.'))
                currentSuite.chop(1);
        }
        // 用例行：缩进
        else if (!currentSuite.isEmpty()) {
            QString name = line.trimmed();
            // 去掉行尾注释
            int hash = name.indexOf('#');
            if (hash >= 0) name = name.left(hash).trimmed();
            if (name.isEmpty()) continue;

            TestCase tc;
            tc.suiteName = currentSuite;
            tc.caseName  = name;
            m_cases.append(tc);
        }
    }

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
