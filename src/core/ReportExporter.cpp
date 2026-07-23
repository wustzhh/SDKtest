#include "ReportExporter.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QCoreApplication>

// ═══════════════════════════════════════════════════════════
//  JSON 序列化
// ═══════════════════════════════════════════════════════════
static QJsonObject resultToJson(const TestRunResult& r) {
    QJsonObject jr;
    jr["s"] = r.testCase.suiteName;
    jr["c"] = r.testCase.caseName;
    jr["st"] = r.status;
    jr["d"] = r.durationMs;
    if (!r.rawStderr.isEmpty()) jr["err"] = r.rawStderr;
    QJsonObject jp;
    for (auto it = r.properties.begin(); it != r.properties.end(); ++it)
        jp[it.key()] = it.value();
    jr["p"] = jp;
    // 截图路径
    if (!r.properties.value("_screenshot_import").isEmpty())
        jr["si"] = r.properties["_screenshot_import"];
    if (!r.properties.value("_screenshot_export").isEmpty())
        jr["se"] = r.properties["_screenshot_export"];
    return jr;
}

static QJsonObject runToJson(const TestReport& report, const QString& runName) {
    QJsonObject entry;
    entry["id"] = report.startTime.toString("yyyyMMdd_HHmmss_zzz");
    entry["name"] = runName.isEmpty() ? report.startTime.toString("HH:mm:ss") : runName;
    entry["time"] = report.startTime.toString("yyyy-MM-dd HH:mm:ss");
    entry["binary"] = report.testBinary;
    entry["filter"] = report.filterPattern;
    entry["total"] = report.total();
    entry["passed"] = report.passed();
    entry["failed"] = report.failed();
    entry["skipped"] = report.skipped();
    entry["disabled"] = report.disabled();
    entry["durationMs"] = (int)report.totalDurationMs();
    QJsonArray filtersArr;
    for (const auto& fs : report.savedFilters) {
        QJsonObject fo; fo["name"] = fs.name; fo["mode"] = fs.mode;
        QJsonArray conds;
        for (const auto& c : fs.conditions) {
            QJsonObject co; co["c"] = c.key; co["o"] = c.op; co["v"] = c.value;
            conds.append(co);
        }
        fo["conds"] = conds;
        filtersArr.append(fo);
    }
    entry["savedFilters"] = filtersArr;
    QJsonArray results;
    for (const auto& r : report.results)
        results.append(resultToJson(r));
    entry["results"] = results;
    return entry;
}

// ═══════════════════════════════════════════════════════════
//  仅保存 JSON 数据文件（不操作 HTML）
// ═══════════════════════════════════════════════════════════
bool ReportExporter::saveJson(const TestReport& report,
                               const QString& htmlDir,
                               const QString& runName,
                               QString* errorMsg)
{
    QDir dir(htmlDir);
    dir.mkpath("data");

    QString jsonName = QString("report_%1.json").arg(report.startTime.toString("yyyyMMdd_HHmmss"));
    QFile f(dir.filePath("data/" + jsonName));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = "Cannot write data file";
        return false;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << QJsonDocument(runToJson(report, runName)).toJson(QJsonDocument::Indented);
    f.close();
    return true;
}

// ═══════════════════════════════════════════════════════════
//  加载 data 目录下所有 JSON，按 binary 去重
// ═══════════════════════════════════════════════════════════
static TestReport jsonToReport(const QJsonObject& entry, QString& runName) {
    TestReport r;
    r.startTime = QDateTime::fromString(entry["id"].toString(), "yyyyMMdd_HHmmss_zzz");
    r.endTime = r.startTime;
    runName = entry["name"].toString();
    r.testBinary = entry["binary"].toString();
    r.filterPattern = entry["filter"].toString();
    // 解析 savedFilters
    for (const auto& fv : entry["savedFilters"].toArray()) {
        QJsonObject fo = fv.toObject();
        FilterSet fs; fs.name = fo["name"].toString(); fs.mode = fo["mode"].toString("and");
        for (const auto& cv : fo["conds"].toArray()) {
            QJsonObject co = cv.toObject();
            FilterCondition c;
            QJsonValue cc = co["c"];
            if (cc.isString()) c.key = cc.toString();
            else c.key = QString::number(cc.toInt());
            c.op  = co["o"].toString();
            c.value = co["v"].toString();
            fs.conditions.append(c);
        }
        r.savedFilters.append(fs);
    }
    // 解析 results
    for (const auto& rv : entry["results"].toArray()) {
        QJsonObject ro = rv.toObject();
        TestRunResult tr;
        tr.testCase.suiteName = ro["s"].toString();
        tr.testCase.caseName  = ro["c"].toString();
        tr.status   = ro["st"].toString();
        tr.durationMs = ro["d"].toDouble();
        tr.rawStderr = ro["err"].toString();
        auto jp = ro["p"].toObject();
        for (auto it = jp.begin(); it != jp.end(); ++it)
            tr.properties[it.key()] = it.value().toString();
        if (!ro["si"].toString().isEmpty())
            tr.properties["_screenshot_import"] = ro["si"].toString();
        if (!ro["se"].toString().isEmpty())
            tr.properties["_screenshot_export"] = ro["se"].toString();
        r.results.append(tr);
    }
    return r;
}

QVector<QPair<TestReport, QString>> ReportExporter::loadAllData(const QString& dataDir,
                                                                   QString* errorMsg)
{
    QVector<QPair<TestReport, QString>> entries;
    QDir dir(dataDir + "/data");
    if (!dir.exists()) return entries;

    struct FileInfo { QString path; QString binary; QDateTime time; };
    QVector<FileInfo> files;
    for (const auto& fi : dir.entryInfoList({"report_*.json"}, QDir::Files, QDir::Name)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) continue;
        auto obj = doc.object();
        FileInfo info;
        info.path = fi.absoluteFilePath();
        info.binary = obj["binary"].toString();
        info.time = QDateTime::fromString(obj["id"].toString(), "yyyyMMdd_HHmmss_zzz");
        if (!info.time.isValid()) info.time = fi.lastModified();
        files.append(info);
    }

    std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
        return a.time > b.time;
    });

    QSet<QString> seenBinaries;
    QVector<QString> keepPaths;
    for (const auto& fi : files) {
        QString key = fi.binary.isEmpty() ? "_unknown_" : fi.binary;
        if (seenBinaries.contains(key)) {
            QFile::remove(fi.path);
        } else {
            seenBinaries.insert(key);
            keepPaths.append(fi.path);
        }
    }

    for (const auto& path : keepPaths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) continue;
        QString runName;
        TestReport report = jsonToReport(doc.object(), runName);
        entries.append({report, runName});
    }

    return entries;
}

// ═══════════════════════════════════════════════════════════
//  从多条数据记录重建完整 HTML
// ═══════════════════════════════════════════════════════════
bool ReportExporter::rebuildHtml(const QVector<QPair<TestReport, QString>>& entries,
                                  const QString& htmlDir,
                                  QString* errorMsg)
{
    if (entries.isEmpty()) {
        if (errorMsg) *errorMsg = "No entries to build report";
        return false;
    }

    QDir dir(htmlDir);
    QString htmlPath = dir.filePath("test_report.html");

    // 读取模板
    QString tplPath = QCoreApplication::applicationDirPath() + "/template_report.html";
    QString html;
    QFile tpl(tplPath);
    if (tpl.open(QIODevice::ReadOnly | QIODevice::Text)) {
        html = QString::fromUtf8(tpl.readAll());
        tpl.close();
    } else {
        // 后备：从 exe 同目录读取
        QFile tpl2(tplPath);
        if (tpl2.open(QIODevice::ReadOnly | QIODevice::Text)) {
            html = QString::fromUtf8(tpl2.readAll());
            tpl2.close();
        } else {
            if (errorMsg) *errorMsg = "Template not found";
            return false;
        }
    }

    // 构建完整 JSON 数据数组
    QJsonArray arr;
    for (const auto& entry : entries) {
        arr.append(runToJson(entry.first, entry.second));
    }

    QString dataStr = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    html.replace("__DATA__", dataStr);

    QFile f(htmlPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = "Cannot write: " + htmlPath;
        return false;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << html;
    f.close();

    return true;
}

// ═══════════════════════════════════════════════════════════
//  导出单条（兼容旧调用，追加到现有 HTML）
// ═══════════════════════════════════════════════════════════
bool ReportExporter::exportRun(const TestReport& report,
                                const QString& htmlDir,
                                const QString& runName,
                                QString* errorMsg)
{
    QDir dir(htmlDir);
    dir.mkpath("data");

    QString htmlPath = dir.filePath("test_report.html");

    // 生成 JSON 数据文件
    {
        QString jsonName = QString("report_%1.json").arg(report.startTime.toString("yyyyMMdd_HHmmss"));
        QFile f(dir.filePath("data/" + jsonName));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMsg) *errorMsg = "Cannot write data file";
            return false;
        }
        QTextStream out(&f);
        out.setEncoding(QStringConverter::Utf8);
        out << QJsonDocument(runToJson(report, runName)).toJson(QJsonDocument::Indented);
        f.close();
    }

    // 生成 / 追加 HTML
    if (QFile::exists(htmlPath)) {
        // ── 已存在：追加到 DATA 数组 ──
        QFile f(htmlPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMsg) *errorMsg = "Cannot read: " + htmlPath;
            return false;
        }
        QString html = QString::fromUtf8(f.readAll());
        f.close();

        int pos = html.indexOf("var DATA = [");
        if (pos < 0) pos = html.indexOf("var DATA=["); // 无空格版本
        if (pos < 0) {
            if (errorMsg) *errorMsg = "Invalid HTML: no DATA marker";
            return false;
        }
        int insertPos = pos + QString("var DATA = [").length();

        int closePos = html.indexOf("];", insertPos);
        if (closePos < 0) closePos = html.indexOf("] ;", insertPos);
        int arrEnd = html.indexOf("];\n(function", insertPos);
        if (arrEnd > 0 && (closePos < 0 || arrEnd < closePos)) closePos = arrEnd;
        if (closePos < 0) {
            if (errorMsg) *errorMsg = "Invalid HTML: no DATA array close";
            return false;
        }

        QString newEntry = QString::fromUtf8(
            QJsonDocument(runToJson(report, runName)).toJson(QJsonDocument::Compact));
        html.insert(closePos, ",\n  " + newEntry);

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMsg) *errorMsg = "Cannot write: " + htmlPath;
            return false;
        }
        QTextStream out(&f);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        f.close();
    } else {
        // ── 首次：从模板生成 ──
        QString tplPath = QCoreApplication::applicationDirPath() + "/template_report.html";
        QString html;
        QFile tpl(tplPath);
        if (tpl.open(QIODevice::ReadOnly | QIODevice::Text)) {
            html = QString::fromUtf8(tpl.readAll());
            tpl.close();
        } else {
            // 后备：从 exe 同目录读取
            tplPath = QCoreApplication::applicationDirPath() + "/template_report.html";
            QFile tpl2(tplPath);
            if (tpl2.open(QIODevice::ReadOnly | QIODevice::Text)) {
                html = QString::fromUtf8(tpl2.readAll());
                tpl2.close();
            } else {
                if (errorMsg) *errorMsg = "Template not found";
                return false;
            }
        }

        QJsonArray arr;
        arr.append(runToJson(report, runName));
        QString dataStr = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        html.replace("__DATA__", dataStr);

        QFile f(htmlPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMsg) *errorMsg = "Cannot write: " + htmlPath;
            return false;
        }
        QTextStream out(&f);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        f.close();
    }

    return true;
}
