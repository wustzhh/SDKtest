#include "ConfigManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QCoreApplication>

ConfigManager::ConfigManager() {
    // 默认路径：exe 同级 config/
    QString exeDir = QCoreApplication::applicationDirPath();
    m_configPath = exeDir + "/config/test_config.json";
}

bool ConfigManager::load(const QString& configPath) {
    m_configPath = configPath;
    return load();
}

bool ConfigManager::load() {
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QString("Cannot open config: %1").arg(m_configPath);
        return false;
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

void ConfigManager::fromJson(const QJsonObject& obj) {
    m_testBinary = obj["test_binary"].toString();
    m_workingDir = obj["working_dir"].toString();
    m_extraArgs.clear();
    for (const auto& a : obj["extra_args"].toArray())
        m_extraArgs << a.toString();

    m_categories.clear();
    for (const auto& c : obj["categories"].toArray()) {
        QJsonObject co = c.toObject();
        TestCategory cat;
        cat.name = co["name"].toString();
        for (const auto& p : co["prefixes"].toArray())
            cat.prefixes << p.toString();
        m_categories.push_back(cat);
    }
}

QJsonObject ConfigManager::toJson() const {
    QJsonObject obj;
    obj["test_binary"] = m_testBinary;
    if (!m_workingDir.isEmpty())
        obj["working_dir"] = m_workingDir;

    QJsonArray args;
    for (const auto& a : m_extraArgs) args.append(a);
    obj["extra_args"] = args;

    QJsonArray cats;
    for (const auto& c : m_categories) {
        QJsonObject co;
        co["name"] = c.name;
        QJsonArray prefs;
        for (const auto& p : c.prefixes) prefs.append(p);
        co["prefixes"] = prefs;
        cats.append(co);
    }
    obj["categories"] = cats;
    return obj;
}

void ConfigManager::setTestBinary(const QString& v) { m_testBinary = v; }
void ConfigManager::setWorkingDir(const QString& v) { m_workingDir = v; }
void ConfigManager::setExtraArgs(const QStringList& v) { m_extraArgs = v; }
void ConfigManager::addCategory(const TestCategory& c) { m_categories.push_back(c); }
void ConfigManager::removeCategory(int idx) { if (idx >= 0 && idx < m_categories.size()) m_categories.remove(idx); }
void ConfigManager::setCategories(const QVector<TestCategory>& cats) { m_categories = cats; }
