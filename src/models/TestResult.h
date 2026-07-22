#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariant>
#include <QMap>
#include <QDateTime>

// ────────────────────────────────────────────────────────────
//  筛选条件 / 筛选组
// ────────────────────────────────────────────────────────────
struct FilterCondition {
    QString key;   // 属性名  e.g. "interface"
    QString op;    // 匹配方式 e.g. "eq" "ne" "in" "notin"
    QString value;
};

struct FilterSet {
    QString name;
    QVector<FilterCondition> conditions;
};

// ────────────────────────────────────────────────────────────
//  测试用例的基本描述
// ────────────────────────────────────────────────────────────
struct TestCase {
    QString suiteName;   // 测试套间名  e.g. "FixtureTestRecordData"
    QString caseName;    // 测试用例名  e.g. "VerifyDefeature_Zhh"
    QString fullName() const { return suiteName + "." + caseName; }
};

// ────────────────────────────────────────────────────────────
//  一个"模型数据节点"——渲染树的最小单元
// ────────────────────────────────────────────────────────────
enum class NodeType {
    Root,         // 根
    Suite,        // 测试套间
    Case,         // 测试用例
    Scalar,       // 标量值  e.g. nbBodies: 3
    Status,       // 状态    e.g. PASSED / FAILED / SKIP
    KeyValue,     // 键值对  e.g. tolerance: 0.01
    Array,        // 数组    e.g. [102, 105, 108]
    Table,        // 表格数据
    Section       // 分组标题 e.g. "拓扑统计 →"
};

struct ResultNode {
    QString     name;        // 显示名
    QString     value;       // 显示值（可选）
    NodeType    type = NodeType::Scalar;
    QVariant    rawData;     // 原始数据（JSON object/array/…）
    QVector<ResultNode> children;

    // 运行态
    bool        expanded = true;   // 默认展开
    int         matchScore = 0;    // 搜索匹配度（0=不匹配，>0 高亮）

    static ResultNode makeScalar(const QString& name, const QString& val) {
        return { name, val, NodeType::Scalar };
    }
    static ResultNode makeKeyValue(const QString& key, const QString& val) {
        return { key, val, NodeType::KeyValue };
    }
    static ResultNode makeStatus(const QString& name, const QString& status) {
        return { name, status, NodeType::Status };
    }
    static ResultNode makeArray(const QString& name, const QStringList& items) {
        ResultNode n{ name, {}, NodeType::Array };
        for (const auto& s : items)
            n.children.push_back(makeScalar(s, ""));
        return n;
    }
    static ResultNode makeSection(const QString& title) {
        return { title, {}, NodeType::Section };
    }
};

// ────────────────────────────────────────────────────────────
//  一个测试用例的完整运行结果
// ────────────────────────────────────────────────────────────
struct TestRunResult {
    TestCase    testCase;
    QString     status;         // PASSED / FAILED / SKIPPED / ERROR
    double      durationMs = 0;

    QString     rawStdout;      // 原始 stdout
    QString     rawStderr;      // 原始 stderr

    // gtest RecordProperty 自定义属性
    QMap<QString, QString> properties;  // e.g. interface, model, resultModel

    // 解析后的结构化模型树
    ResultNode  modelTree;

    bool passed() const { return status == "PASSED"; }
};

// ────────────────────────────────────────────────────────────
//  一次运行的完整报告
// ────────────────────────────────────────────────────────────
struct TestReport {
    QDateTime           startTime;
    QDateTime           endTime;
    QString             testBinary;
    QString             filterPattern;

    QVector<TestRunResult> results;
    QVector<FilterSet>   savedFilters;

    int total()     const { return results.size(); }
    int passed()    const { int c=0; for(auto& r:results) if(r.passed()) ++c; return c; }
    int skipped()   const { int c=0; for(auto& r:results) if(r.status=="SKIPPED") ++c; return c; }
    int disabled()  const { int c=0; for(auto& r:results) if(r.status=="DISABLED") ++c; return c; }
    int failed()    const { return total() - passed() - skipped() - disabled(); }
    double totalDurationMs() const {
        double s=0; for(auto& r:results) s+=r.durationMs; return s;
    }
};
