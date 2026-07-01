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
//  一个 Exe 配置 Profile
// ────────────────────────────────────────────────────────────
struct ExeProfile {
    QString     name;             // 配置名
    QString     testBinary;       // exe 路径
    QStringList dependencies;     // 依赖路径（DLL 目录或文件）
    QString     workingDir;       // 工作目录
    QStringList extraArgs;        // 额外参数
    QVector<TestCategory> categories;

    bool isValid() const { return !testBinary.isEmpty(); }
};

// ────────────────────────────────────────────────────────────
//  配置管理器（JSON → 内存结构）
// ────────────────────────────────────────────────────────────
class ConfigManager {
public:
    ConfigManager();

    // 加载/重载配置文件
    bool load(const QString& configPath);
    bool load();

    // 保存当前配置
    bool save() const;

    // ── Profile 管理 ──
    const QVector<ExeProfile>& profiles() const { return m_profiles; }
    int activeProfile() const { return m_activeProfile; }
    const ExeProfile& currentProfile() const;
    ExeProfile& currentProfile();

    void setProfiles(const QVector<ExeProfile>& profiles);
    void setActiveProfile(int idx);
    void addProfile(const ExeProfile& p);
    void removeProfile(int idx);
    void updateProfile(int idx, const ExeProfile& p);

    // ── 旧接口兼容 ──
    QString testBinary() const;
    QString workingDir() const;
    QStringList extraArgs() const;
    QVector<TestCategory> categories() const;
    void setTestBinary(const QString& v);
    void setWorkingDir(const QString& v);
    void setExtraArgs(const QStringList& v);
    void addCategory(const TestCategory& c);
    void removeCategory(int idx);
    void setCategories(const QVector<TestCategory>& cats);

    // ── UI 状态 ──
    struct UIState {
        int  windowX = -1, windowY = -1, windowW = 1280, windowH = 800;
        bool maximized = false;
        int  themeIndex = 0;       // 0=light 1=dark 2=high contrast
        int  splitterLeftW = 250;   // 左面板宽度(像素)
        int  splitterRightW = 350;  // 右面板宽度(像素)
        int  splitterVPos = 200;    // 中栏垂直分割位置(像素, 上:进度 下:结果)
        int  splitterVPos2 = 400;   // 中栏下半部分内部分割(结果树 与 属性树)
        bool modelInfoCollapsed = false; // ModelInfo 面板是否收起
        bool leftPanelVisible = true;
        bool rightPanelVisible = true;
    };
    UIState uiState;

    // ── 默认路径 ──
    QString configPath() const { return m_configPath; }
    static QString defaultConfigPath();

    // ── 错误 ──
    QString lastError() const { return m_lastError; }

private:
    void fromJson(const QJsonObject& obj);
    QJsonObject toJson() const;
    ExeProfile profileFromJson(const QJsonObject& obj) const;
    QJsonObject profileToJson(const ExeProfile& p) const;

    QString             m_configPath;
    QVector<ExeProfile> m_profiles;
    int                 m_activeProfile = 0;
    mutable QString     m_lastError;
};