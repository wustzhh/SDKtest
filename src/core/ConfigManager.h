#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

// ────────────────────────────────────────────────────────────
//  测试分类定义（用于用例树分组）
// ────────────────────────────────────────────────────────────
struct TestCategory {
    QString     name;            // 显示名  e.g. "几何算法测试"
    QStringList prefixes;        // Suite 名前缀  e.g. ["FixtureTest", "ut_geometry"]
};

// ────────────────────────────────────────────────────────────
//  配置管理器（JSON → 内存结构）
// ────────────────────────────────────────────────────────────
class ConfigManager {
public:
    ConfigManager();

    // 加载/重载配置文件
    bool load(const QString& configPath);
    bool load();  // 上次路径

    // 保存当前配置
    bool save() const;

    // ── 访问器 ──
    QString testBinary()        const { return m_testBinary; }
    QString workingDir()        const { return m_workingDir; }
    QStringList extraArgs()      const { return m_extraArgs; }
    QVector<TestCategory> categories() const { return m_categories; }
    QString configPath()        const { return m_configPath; }

    // ── 修改器（UI 编辑用）──
    void setTestBinary(const QString& v);
    void setWorkingDir(const QString& v);
    void setExtraArgs(const QStringList& v);
    void addCategory(const TestCategory& c);
    void removeCategory(int idx);
    void setCategories(const QVector<TestCategory>& cats);

    // ── 错误 ──
    QString lastError() const { return m_lastError; }

private:
    void fromJson(const QJsonObject& obj);
    QJsonObject toJson() const;

    QString             m_configPath;
    QString             m_testBinary;
    QString             m_workingDir;
    QStringList         m_extraArgs;
    QVector<TestCategory> m_categories;
    mutable QString     m_lastError;
};
