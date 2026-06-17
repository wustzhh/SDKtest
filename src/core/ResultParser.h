#pragma once

#include "models/TestResult.h"
#include <QString>

// ────────────────────────────────────────────────────────────
//  解析 gtest stdout → 结构化模型树
//  ResultNode 树中的 "模型" 数据
// ────────────────────────────────────────────────────────────
class ResultParser {
public:
    static TestRunResult parse(const TestCase& tc,
                               const QString& rawStdout,
                               const QString& rawStderr,
                               double durationMs,
                               const QString& status);

private:
    // 从原始输出中提取结构化数据
    static ResultNode buildModelTree(const QString& output);

    // 解析特定模式的行
    static void parseTopoInfo(ResultNode& parent, const QString& line);
    static void parseDefeatureInfo(ResultNode& parent, const QString& line);
    static void parseTimingInfo(ResultNode& parent, const QString& line);
};
