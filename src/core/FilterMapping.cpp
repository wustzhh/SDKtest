#include "FilterMapping.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace FilterMapping {

QString filterSetsSignature(const QVector<FilterSet>& sets) {
    QJsonArray arr;
    for (const auto& fs : sets) {
        QJsonObject o;
        o["name"] = fs.name;
        o["mode"] = fs.mode;
        QJsonArray conds;
        for (const auto& c : fs.conditions) {
            QJsonObject co;
            co["key"] = c.key;
            co["op"] = c.op;
            co["value"] = c.value;
            conds.append(co);
        }
        o["conditions"] = conds;
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

static QString valueOf(const TestRunResult& r, const QString& key) {
    if (key == QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81")) return r.status;
    if (key == QString::fromUtf8("\xe5\xa5\x97\xe4\xbb\xb6")) return r.testCase.suiteName;
    if (key == QString::fromUtf8("\xe7\x94\xa8\xe4\xbe\x8b")) return r.testCase.caseName;
    if (key == QString::fromUtf8("\xe8\x80\x97\xe6\x97\xb6(ms)")) return QString::number(r.durationMs, 'f', 0);
    if (r.properties.contains(key)) return r.properties.value(key);
    return QString();
}

static QString statusCodeFromDisplay(const QString& disp) {
    const QString d = disp.trimmed();
    // 已直接是内部状态码则原样返回
    if (d == QLatin1String("PASSED") || d == QLatin1String("FAILED") ||
        d == QLatin1String("SKIPPED") || d == QLatin1String("DISABLED") ||
        d == QLatin1String("CRASHED"))
        return d;
    // 筛选对话框里“状态”的值是显示串（✅ 通过 等），按结尾中文转成内部状态码
    if (d.endsWith("通过")) return QLatin1String("PASSED");
    if (d.endsWith("跳过")) return QLatin1String("SKIPPED");
    if (d.endsWith("禁用")) return QLatin1String("DISABLED");
    if (d.endsWith("失败")) return QLatin1String("FAILED");
    return d;
}

static bool matchesCondition(const TestRunResult& r, const FilterCondition& c) {
    QString v = valueOf(r, c.key);
    QString cond = c.value.trimmed();
    // 状态列的值统一按内部状态码比较
    if (c.key == "状态")
        cond = statusCodeFromDisplay(cond);
    if (cond.isEmpty()) return c.op == QLatin1String("ne");
    if (c.op == QLatin1String("eq")) return QString::compare(v, cond, Qt::CaseInsensitive) == 0;
    if (c.op == QLatin1String("ne")) return QString::compare(v, cond, Qt::CaseInsensitive) != 0;
    if (c.op == QLatin1String("notin")) return !v.contains(cond, Qt::CaseInsensitive);
    return v.contains(cond, Qt::CaseInsensitive); // 默认 in
}

bool matches(const FilterSet& fs, const TestRunResult& r) {
    if (fs.conditions.isEmpty()) return true;
    if (fs.mode == QLatin1String("or")) {
        for (const auto& c : fs.conditions)
            if (matchesCondition(r, c)) return true;
        return false;
    }
    for (const auto& c : fs.conditions)
        if (!matchesCondition(r, c)) return false;
    return true;
}

QMap<QString, QStringList> computeMappings(const QVector<FilterSet>& sets,
                                           const QVector<TestRunResult>& results) {
    QMap<QString, QStringList> m;
    for (const auto& fs : sets) {
        if (fs.name.trimmed().isEmpty()) continue;
        QStringList names;
        for (const auto& r : results)
            if (matches(fs, r)) names << r.testCase.fullName();
        m[fs.name] = names;
    }
    return m;
}

} // namespace FilterMapping
