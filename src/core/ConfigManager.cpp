#include "ConfigManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>

ConfigManager::ConfigManager() {
    m_configPath = defaultConfigPath();
}

QString ConfigManager::defaultConfigPath() {
    // 优先 D 盘，没有则 C 盘
    QString base = "C:";
    for (const auto& d : QDir::drives()) {
        QString name = d.absolutePath().toUpper();
        if (name.startsWith("D")) { base = "D:"; break; }
    }
    return base + "/.SDKtest/config.json";
}

const ExeProfile& ConfigManager::currentProfile() const {
    static ExeProfile empty;
    if (m_activeProfile < 0 || m_activeProfile >= m_profiles.size())
        return empty;
    return m_profiles[m_activeProfile];
}

ExeProfile& ConfigManager::currentProfile() {
    static ExeProfile empty;
    if (m_activeProfile < 0 || m_activeProfile >= m_profiles.size())
        return empty;
    return m_profiles[m_activeProfile];
}

// ── 旧接口兼容 ──
QString ConfigManager::testBinary() const { return currentProfile().testBinary; }
QString ConfigManager::workingDir() const { return currentProfile().workingDir; }
QStringList ConfigManager::extraArgs() const { return currentProfile().extraArgs; }
QVector<TestCategory> ConfigManager::categories() const { return currentProfile().categories; }

// ── Profile 管理 ──
void ConfigManager::setProfiles(const QVector<ExeProfile>& profiles) { m_profiles = profiles; }
void ConfigManager::setActiveProfile(int idx) { if (idx >= 0 && idx < m_profiles.size()) m_activeProfile = idx; }
void ConfigManager::addProfile(const ExeProfile& p) { m_profiles.append(p); }
void ConfigManager::removeProfile(int idx) { if (idx >= 0 && idx < m_profiles.size()) m_profiles.remove(idx); }
void ConfigManager::updateProfile(int idx, const ExeProfile& p) { if (idx >= 0 && idx < m_profiles.size()) m_profiles[idx] = p; }

// ── 加载 ──
bool ConfigManager::load(const QString& configPath) {
    m_configPath = configPath;
    return load();
}

bool ConfigManager::load() {
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        // 文件不存在时创建默认配置
        if (m_profiles.isEmpty()) {
            ExeProfile def;
            def.name = "默认";
            def.testBinary = "";
            def.workingDir = ".";
            TestCategory c1, c2;
            c1.name = "test_p*";
            c1.prefixes << "test_p";
            c2.name = "其他";
            def.categories << c1 << c2;
            def.envVars["MODEL_DIR"] = "";
            m_profiles.append(def);
            m_activeProfile = 0;
            save();
        }
        return true;
    }
    QByteArray data = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        m_lastError = QString("JSON parse error: %1").arg(err.errorString());
        return false;
    }
    fromJson(doc.object());
    return true;
}

bool ConfigManager::save() const {
    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QString("Cannot write config: %1").arg(m_configPath);
        return false;
    }
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

// ── JSON 序列化 ──
void ConfigManager::fromJson(const QJsonObject& obj) {
    m_profiles.clear();
    for (const auto& v : obj["profiles"].toArray())
        m_profiles.append(profileFromJson(v.toObject()));
    m_activeProfile = obj["active_profile"].toInt(0);
    if (m_profiles.isEmpty()) {
        ExeProfile def;
        def.name = "默认";
        def.workingDir = ".";
        TestCategory c1, c2;
        c1.name = "test_p*";
        c1.prefixes << "test_p";
        c2.name = "其他";
        def.categories << c1 << c2;
        def.envVars["MODEL_DIR"] = "";
        m_profiles.append(def);
    }
    if (m_activeProfile >= m_profiles.size()) m_activeProfile = 0;

    // UI 状态
    auto ui = obj["ui"].toObject();
    uiState.windowX = ui["window_x"].toInt(-1);
    uiState.windowY = ui["window_y"].toInt(-1);
    uiState.windowW = ui["window_w"].toInt(1280);
    uiState.windowH = ui["window_h"].toInt(800);
    uiState.maximized = ui["maximized"].toBool(false);
    uiState.themeIndex = ui["theme"].toInt(0);
    uiState.splitterLeftPct = ui["splitter_left_pct"].toInt(20);
    uiState.splitterRightPct = ui["splitter_right_pct"].toInt(30);
    uiState.splitterVPct = ui["splitter_v_pct"].toInt(25);
    uiState.splitterV2Pct = ui["splitter_v2_pct"].toInt(60);
    uiState.cfgDialogW = ui["cfg_dialog_w"].toInt(580);
    uiState.cfgDialogH = ui["cfg_dialog_h"].toInt(500);
    uiState.modelInfoCollapsed = ui["model_info_collapsed"].toBool(false);
    uiState.leftPanelVisible = ui["left_panel"].toBool(true);
    uiState.rightPanelVisible = ui["right_panel"].toBool(true);
}

QJsonObject ConfigManager::toJson() const {
    QJsonObject obj;
    obj["config_path"] = m_configPath;
    obj["active_profile"] = m_activeProfile;
    QJsonArray arr;
    for (const auto& p : m_profiles) arr.append(profileToJson(p));
    obj["profiles"] = arr;

    QJsonObject ui;
    ui["window_x"] = uiState.windowX;
    ui["window_y"] = uiState.windowY;
    ui["window_w"] = uiState.windowW;
    ui["window_h"] = uiState.windowH;
    ui["maximized"] = uiState.maximized;
    ui["theme"] = uiState.themeIndex;
    ui["splitter_left_pct"] = uiState.splitterLeftPct;
    ui["splitter_right_pct"] = uiState.splitterRightPct;
    ui["splitter_v_pct"] = uiState.splitterVPct;
    ui["splitter_v2_pct"] = uiState.splitterV2Pct;
    ui["cfg_dialog_w"] = uiState.cfgDialogW;
    ui["cfg_dialog_h"] = uiState.cfgDialogH;
    ui["model_info_collapsed"] = uiState.modelInfoCollapsed;
    ui["left_panel"] = uiState.leftPanelVisible;
    ui["right_panel"] = uiState.rightPanelVisible;
    obj["ui"] = ui;

    return obj;
}

ExeProfile ConfigManager::profileFromJson(const QJsonObject& obj) const {
    ExeProfile p;
    p.name = obj["name"].toString();
    p.testBinary = obj["test_binary"].toString();
    p.workingDir = obj["working_dir"].toString();
    for (const auto& d : obj["dependencies"].toArray())
        p.dependencies << d.toString();
    for (const auto& a : obj["extra_args"].toArray())
        p.extraArgs << a.toString();
    for (const auto& c : obj["categories"].toArray()) {
        QJsonObject co = c.toObject();
        TestCategory cat;
        cat.name = co["name"].toString();
        for (const auto& pr : co["prefixes"].toArray())
            cat.prefixes << pr.toString();
        p.categories.push_back(cat);
    }
    for (const auto& sv : obj["scenarios"].toArray()) {
        QJsonObject so = sv.toObject();
        TestScenario s; s.name = so["name"].toString();
        for (const auto& t : so["selectedTests"].toArray())
            s.selectedTests << t.toString();
        s.singleTest = so["single_test"].toBool(false);
        p.scenarios.push_back(s);
    }
    auto envObj = obj["env_vars"].toObject();
    for (auto it = envObj.begin(); it != envObj.end(); ++it)
        p.envVars[it.key()] = it.value().toString();

    return p;
}

QJsonObject ConfigManager::profileToJson(const ExeProfile& p) const {
    QJsonObject obj;
    obj["name"] = p.name;
    obj["test_binary"] = p.testBinary;
    obj["working_dir"] = p.workingDir;
    QJsonArray deps;
    for (const auto& d : p.dependencies) deps.append(d);
    obj["dependencies"] = deps;
    QJsonArray args;
    for (const auto& a : p.extraArgs) args.append(a);
    obj["extra_args"] = args;
    QJsonArray cats;
    for (const auto& c : p.categories) {
        QJsonObject co; co["name"] = c.name;
        QJsonArray cp; for (const auto& pr : c.prefixes) cp.append(pr);
        co["prefixes"] = cp; cats.append(co);
    }
    obj["categories"] = cats;
    QJsonArray scs;
    for (const auto& s : p.scenarios) {
        QJsonObject so; so["name"] = s.name;
        QJsonArray st; for (const auto& t : s.selectedTests) st.append(t);
        so["selectedTests"] = st;
        so["single_test"] = s.singleTest;
        scs.append(so);
    }
    obj["scenarios"] = scs;
    QJsonObject envObj;
    for (auto it = p.envVars.begin(); it != p.envVars.end(); ++it)
        envObj[it.key()] = it.value();
    obj["env_vars"] = envObj;

    return obj;
}

void ConfigManager::setTestBinary(const QString& v) { currentProfile().testBinary = v; }
void ConfigManager::setWorkingDir(const QString& v) { currentProfile().workingDir = v; }
void ConfigManager::setExtraArgs(const QStringList& v) { currentProfile().extraArgs = v; }
void ConfigManager::addCategory(const TestCategory& c) { currentProfile().categories.push_back(c); }
void ConfigManager::removeCategory(int idx) { if (idx >= 0 && idx < currentProfile().categories.size()) currentProfile().categories.remove(idx); }
void ConfigManager::setCategories(const QVector<TestCategory>& cats) { currentProfile().categories = cats; }

void ConfigManager::addScenario(const TestScenario& s) { currentProfile().scenarios.push_back(s); }
void ConfigManager::removeScenario(int idx) { if (idx >= 0 && idx < currentProfile().scenarios.size()) currentProfile().scenarios.remove(idx); }
void ConfigManager::updateScenario(int idx, const TestScenario& s) { if (idx >= 0 && idx < currentProfile().scenarios.size()) currentProfile().scenarios[idx] = s; }