#pragma once

#include "models/TestResult.h"

#include <QString>
#include <QVector>

// ────────────────────────────────────────────────────────────
//  通过 --gtest_list_tests 发现所有测试用例
// ────────────────────────────────────────────────────────────
class TestLoader {
public:
    TestLoader();

    // 执行 list_tests，返回 true 表示成功
    bool load(const QString& binaryPath, const QStringList& extraArgs = {},
              const QString& workingDir = {}, const QStringList& dependencies = {},
              const QMap<QString, QString>& envVars = {});

    // 获取发现的用例
    QVector<TestCase> testCases() const { return m_cases; }

    // 按 suite 名称分组
    QMap<QString, QVector<TestCase>> groupedBySuite() const;

    QString lastError() const { return m_lastError; }

private:
    QVector<TestCase> m_cases;
    QString m_lastError;
};
