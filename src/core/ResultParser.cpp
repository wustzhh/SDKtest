#include "ResultParser.h"

#include <QTextStream>
#include <QRegularExpression>

TestRunResult ResultParser::parse(const TestCase& tc,
                                  const QString& rawOutput,
                                  const QString& rawStderr,
                                  double durationMs,
                                  const QString& status)
{
    TestRunResult result;
    result.testCase   = tc;
    result.status     = status;
    result.durationMs = durationMs;
    result.rawStdout  = rawOutput;
    result.rawStderr  = rawStderr;

    // 构建模型树
    result.modelTree = ResultNode::makeSection(tc.fullName());

    // 提取测试输出本体（去掉 gtest 框架行）
    QString body = rawOutput;
    // 去掉 [==========] / [----------] / [ RUN      ] / [       OK ] / [  FAILED  ] 行
    static QRegularExpression gtestLine(R"(\[[^\]]+\].*)");
    body.replace(gtestLine, "");

    // 解析结构化数据
    auto& tree = result.modelTree;
    QTextStream stream(&body);
    QString line;

    // ── 状态节点 ──
    tree.children.push_back(ResultNode::makeStatus("状态", status));

    // ── 耗时 ──
    tree.children.push_back(
        ResultNode::makeKeyValue("耗时", QString::number(durationMs, 'f', 1) + " ms"));

    // ── 属性节点（提前建好，避免空节点头）──
    bool hasProps = false;

    // ── 逐行解析 ──
    bool inTopoSection = false;
    bool inDefeatureSection = false;
    bool inTimingSection = false;

    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty()) continue;

        // 拓扑信息
        if (line.contains("nbBodies") || line.contains("nbFaces") ||
            line.contains("nbEdges") || line.contains("nbVertices")) {
            if (!inTopoSection) {
                tree.children.push_back(ResultNode::makeSection("📊 拓扑统计"));
                inTopoSection = true;
            }
            parseTopoInfo(tree.children.last(), line);
            continue;
        }

        // 特征信息
        if (line.contains("shapeIds") || line.contains("searched") ||
            line.contains("removed") || line.contains("features")) {
            if (!inDefeatureSection) {
                tree.children.push_back(ResultNode::makeSection("🔍 特征识别"));
                inDefeatureSection = true;
            }
            parseDefeatureInfo(tree.children.last(), line);
            continue;
        }

        // ── 自定义属性 [PROP] key=value ──
        if (line.startsWith("[PROP]") || line.startsWith("prop:")) {
            static QRegularExpression propRe(R"((?:\[PROP\]|prop:)\s*(\w+)\s*=\s*(.+))");
            auto m = propRe.match(line);
            if (m.hasMatch()) {
                QString key = m.captured(1);
                QString val = m.captured(2).trimmed();
                result.properties[key] = val;
                if (!hasProps) {
                    tree.children.push_back(ResultNode::makeSection("📋 模型属性"));
                    hasProps = true;
                }
                tree.children.last().children.push_back(
                    ResultNode::makeKeyValue(key, val));
            }
            continue;
        }

        // 计时信息
        if (line.contains("ms") && (line.startsWith("import") ||
            line.startsWith("export") || line.contains("用时"))) {
            if (!inTimingSection) {
                tree.children.push_back(ResultNode::makeSection("⏱ 计时"));
                inTimingSection = true;
            }
            parseTimingInfo(tree.children.last(), line);
            continue;
        }

        // EXPECT_EQ 失败信息
        if (line.contains("error") || line.contains("Error") ||
            line.contains("Failed") || line.contains("failure") ||
            line.contains("exception") || line.contains("Exception")) {
            tree.children.push_back(
                ResultNode::makeStatus("⚠ " + line, "FAIL"));
            continue;
        }
    }

    // ── 原始输出 ──
    tree.children.push_back(ResultNode::makeSection("📄 原始输出"));
    tree.children.push_back(ResultNode::makeScalar("stdout",
        result.rawStdout.left(2000)));  // 截断，完整在详情面板
    if (!rawStderr.isEmpty()) {
        tree.children.push_back(ResultNode::makeScalar("stderr", rawStderr));
    }

    return result;
}

void ResultParser::parseTopoInfo(ResultNode& parent, const QString& line) {
    // nbBodies: 3
    static QRegularExpression re(R"((\w+):\s*(\d+))");
    auto match = re.match(line);
    if (match.hasMatch()) {
        parent.children.push_back(
            ResultNode::makeKeyValue(match.captured(1), match.captured(2)));
    }
}

void ResultParser::parseDefeatureInfo(ResultNode& parent, const QString& line) {
    // searched: [8] features
    // removed: [6] features
    static QRegularExpression re(R"((\w+):\s*\[?(\d+)\]?\s*(\w*))");
    auto match = re.match(line);
    if (match.hasMatch()) {
        parent.children.push_back(
            ResultNode::makeKeyValue(match.captured(1),
                                     match.captured(2) + " " + match.captured(3)));
    } else {
        parent.children.push_back(ResultNode::makeScalar(line, ""));
    }
}

void ResultParser::parseTimingInfo(ResultNode& parent, const QString& line) {
    // import: 23ms
    static QRegularExpression re(R"((\w+).*?(\d+\.?\d*)\s*ms)");
    auto match = re.match(line);
    if (match.hasMatch()) {
        parent.children.push_back(
            ResultNode::makeKeyValue(match.captured(1),
                                     match.captured(2) + " ms"));
    } else {
        parent.children.push_back(ResultNode::makeScalar(line, ""));
    }
}
