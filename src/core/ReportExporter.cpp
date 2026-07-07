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
    jr["st"] = r.status == "PASSED" ? "PASSED" : "FAILED";
    jr["d"] = r.durationMs;
    if (!r.rawStderr.isEmpty()) jr["err"] = r.rawStderr;
    QJsonObject jp;
    for (auto it = r.properties.begin(); it != r.properties.end(); ++it)
        jp[it.key()] = it.value();
    jr["p"] = jp;
    return jr;
}

static QJsonObject runToJson(const TestReport& report, const QString& runName) {
    QJsonObject entry;
    entry["id"] = report.startTime.toString("yyyyMMdd_HHmmss");
    entry["name"] = runName.isEmpty() ? report.startTime.toString("HH:mm:ss") : runName;
    entry["time"] = report.startTime.toString("yyyy-MM-dd HH:mm:ss");
    entry["binary"] = report.testBinary;
    entry["filter"] = report.filterPattern;
    entry["total"] = report.total();
    entry["passed"] = report.passed();
    entry["failed"] = report.failed();
    entry["durationMs"] = (int)report.totalDurationMs();
    entry["savedFilters"] = QJsonArray();
    QJsonArray results;
    for (const auto& r : report.results)
        results.append(resultToJson(r));
    entry["results"] = results;
    return entry;
}

// ═══════════════════════════════════════════════════════════
//  导出主入口
// ═══════════════════════════════════════════════════════════
// 内置最小模板（外部 template_report.html 找不到时使用）
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
        // 已有文件时检查是否包含 __DATA__（旧版本生成的需重写）
        // 正常情况直接追加
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
        // 也找 DATA 数组的 ]; 结尾
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
