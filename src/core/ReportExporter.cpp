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
    entry["savedFilters"] = QJsonArray();
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
