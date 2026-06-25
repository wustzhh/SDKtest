#include "ReportExporter.h"
#include "XlsxWriter.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QSet>

// ════════════════════════════════════════════════════════════
//  HTML 导出
// ════════════════════════════════════════════════════════════
bool ReportExporter::exportToHtml(const TestReport& report,
                                   const QString& filePath,
                                   QString* errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    int total = report.total();
    int passed = report.passed();
    int failed = report.failed();
    double passRate = total > 0 ? passed * 100.0 / total : 0;

    QSet<QString> allPropKeys;
    for (const auto& r : report.results)
        for (auto it = r.properties.begin(); it != r.properties.end(); ++it)
            allPropKeys.insert(it.key());
    QStringList propKeys = allPropKeys.values();
    propKeys.sort();

    // HTML 头
    out << "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n";
    out << "<meta charset=\"UTF-8\">\n";
    out << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">\n";
    out << "<title>Test Report</title>\n";
    out << "<style>\n";
    out << "*{margin:0;padding:0;box-sizing:border-box}\n";
    out << "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:linear-gradient(135deg,#e8ecf8,#f0f2f8);color:#1a1a2e;padding:40px 20px;display:flex;justify-content:center}\n";
    out << ".container{max-width:1200px;width:100%}\n";
    out << "h1{font-size:28px;font-weight:300;color:#6c5ce7;margin-bottom:4px;letter-spacing:1px}\n";
    out << "h1 small{font-size:13px;color:#999;margin-left:12px}\n";
    out << ".info{display:flex;gap:20px;flex-wrap:wrap;margin-bottom:28px;padding-bottom:16px;border-bottom:1px solid rgba(108,92,231,0.15);font-size:13px;color:#999}\n";
    out << ".info b{color:#1a1a2e;font-weight:500}\n";
    out << ".stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:14px;margin-bottom:32px}\n";
    out << ".card{background:rgba(255,255,255,0.7);border:1px solid rgba(108,92,231,0.15);border-radius:12px;padding:18px;text-align:center;backdrop-filter:blur(10px);transition:border-color .2s}\n";
    out << ".card:hover{border-color:#6c5ce7}\n";
    out << ".card .n{font-size:30px;font-weight:600}\n";
    out << ".card .l{font-size:11px;color:#999;margin-top:4px;text-transform:uppercase;letter-spacing:1px}\n";
    out << ".card.pass .n{color:#6c5ce7}\n.card.fail .n{color:#ff4757}\n.card.rate .n{color:#ffa502}\n";
    out << "h2{font-size:17px;font-weight:400;color:#6c5ce7;margin:24px 0 14px;letter-spacing:.5px}\n";
    out << ".table-wrap{border-radius:12px}\n";
    out << "table{width:100%;border-collapse:collapse;background:rgba(255,255,255,0.7);border-radius:12px;border:1px solid rgba(108,92,231,0.12)}\n";
    out << "th{background:rgba(108,92,231,0.06);color:#6c5ce7;font-size:10px;font-weight:600;letter-spacing:1px;padding:10px 12px;text-align:left;border-bottom:1px solid rgba(108,92,231,0.1)}\n";
    out << "td{padding:8px 12px;font-size:13px;border-bottom:1px solid rgba(108,92,231,0.04);color:#1a1a2e;word-break:break-all;vertical-align:top}\n";
    out << "tr:hover td{background:rgba(108,92,231,0.04)}\n";
    out << ".ok{color:#6c5ce7;font-weight:500}\n.fail{color:#ff4757;font-weight:500}\n";
    out << "</style>\n</head>\n<body>\n<div class=\"container\">\n";

    // 标题 + 信息
    out << "<h1>" << QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe6\x8a\xa5\xe5\x91\x8a") << " <small>" << report.testBinary.toHtmlEscaped() << "</small></h1>\n";
    out << "<div class=\"info\">";
    out << "<span>" << QString::fromUtf8("\xe6\x97\xb6\xe9\x97\xb4") << ": <b>" << report.startTime.toString("yyyy-MM-dd hh:mm:ss") << "</b></span>";
    out << "<span>" << QString::fromUtf8("\xe7\xad\x9b\xe9\x80\x89") << ": <b>" << report.filterPattern.toHtmlEscaped() << "</b></span>";
    out << "</div>\n";

    // 统计卡片
    out << "<div class=\"stats\">\n";
    out << "<div class=\"card pass\"><div class=\"n\">" << total << "</div><div class=\"l\">" << QString::fromUtf8("\xe5\x85\xb1\xe8\xae\xa1") << "</div></div>\n";
    out << "<div class=\"card pass\"><div class=\"n\">" << passed << "</div><div class=\"l\">" << QString::fromUtf8("\xe9\x80\x9a\xe8\xbf\x87") << "</div></div>\n";
    out << "<div class=\"card fail\"><div class=\"n\">" << failed << "</div><div class=\"l\">" << QString::fromUtf8("\xe5\xa4\xb1\xe8\xb4\xa5") << "</div></div>\n";
    out << "<div class=\"card rate\"><div class=\"n\">" << QString::number(passRate,'f',1) << "%</div><div class=\"l\">" << QString::fromUtf8("\xe9\x80\x9a\xe8\xbf\x87\xe7\x8e\x87") << "</div></div>\n";
    out << "</div>\n";

    // 结果表格
    out << "<h2>" << QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe7\xbb\x93\xe6\x9e\x9c") << "</h2>\n<div class=\"table-wrap\">\n<table>\n<thead><tr><th>" << QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81") << "</th><th>" << QString::fromUtf8("\xe5\xa5\x97\xe4\xbb\xb6") << "</th><th>" << QString::fromUtf8("\xe7\x94\xa8\xe4\xbe\x8b") << "</th><th>" << QString::fromUtf8("\xe8\x80\x97\xe6\x97\xb6(ms)") << "</th>";
    for (const auto& k : propKeys) out << "<th>" << k.toHtmlEscaped() << "</th>";
    out << "</tr></thead>\n<tbody>\n";
    for (const auto& r : report.results) {
        out << "<tr><td class=\"" << (r.passed()?"ok":"fail") << "\">" << (r.passed()?QString::fromUtf8("\xe9\x80\x9a\xe8\xbf\x87"):QString::fromUtf8("\xe5\xa4\xb1\xe8\xb4\xa5")) << "</td>";
        out << "<td>" << r.testCase.suiteName.toHtmlEscaped() << "</td>";
        out << "<td>" << r.testCase.caseName.toHtmlEscaped() << "</td>";
        out << "<td>" << QString::number(r.durationMs,'f',1) << "</td>";
        for (const auto& k : propKeys) out << "<td>" << r.properties.value(k).toHtmlEscaped() << "</td>";
        out << "</tr>\n";
    }
    out << "</tbody>\n</table>\n</div>\n";

    // 失败详情
    bool hasFail = false;
    for (const auto& r : report.results) {
        if (r.passed()) continue;
        if (!hasFail) { out << "<h2>" << QString::fromUtf8("\xe5\xa4\xb1\xe8\xb4\xa5\xe8\xaf\xa6\xe6\x83\x85") << "</h2>\n"; hasFail = true; }
        out << "<div style=\"background:rgba(255,71,87,0.06);border:1px solid rgba(255,71,87,0.2);border-radius:10px;padding:12px;margin:8px 0\">\n";
        out << "<strong style=\"color:#ff4757\">" << r.testCase.fullName().toHtmlEscaped() << "</strong>";
        out << " <span style=\"color:#999;font-size:12px\">" << QString::number(r.durationMs,'f',1) << " ms</span>\n";
        if (!r.rawStderr.isEmpty())
            out << "<pre style=\"font-size:12px;color:#e74c3c;margin-top:6px;max-height:200px;overflow:auto;white-space:pre-wrap\">" << r.rawStderr.toHtmlEscaped() << "</pre>\n";
        out << "</div>\n";
    }
    if (!hasFail) out << "<div style=\"color:#2ed573;padding:10px 0\">" << QString::fromUtf8("\xe2\x9c\x93 \xe5\x85\xa8\xe9\x83\xa8\xe5\xb7\xb2\xe9\x80\x9a\xe8\xbf\x87") << "</div>\n";

    out << "</div>\n</body>\n</html>\n";
    file.close();
    return true;
}

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportToXlsx(const TestReport& report,
                                   const QString& filePath,
                                   QString* errorMsg)
{
    XlsxWriter writer;
    QSet<QString> propKeys;
    for (const auto& r : report.results)
        for (auto it = r.properties.begin(); it != r.properties.end(); ++it)
            propKeys.insert(it.key());
    QStringList propKeysSorted = propKeys.values();
    propKeysSorted.sort();
    QStringList headers = { "Suite", "Case", "Status", "Duration(ms)" };
    for (const auto& k : propKeysSorted) headers << k;
    QVector<QStringList> rows;
    for (const auto& r : report.results) {
        QStringList row;
        row << r.testCase.suiteName << r.testCase.caseName
            << r.status << QString::number(r.durationMs, 'f', 1);
        for (const auto& k : propKeysSorted) row << r.properties.value(k, "");
        rows << row;
    }
    writer.addSheet("Test Summary", headers, rows);
    {
        QStringList h = { "Metric", "Value" };
        QVector<QStringList> rows;
        rows << QStringList{ "Total", QString::number(report.total()) };
        rows << QStringList{ "Passed", QString::number(report.passed()) };
        rows << QStringList{ "Failed", QString::number(report.failed()) };
        rows << QStringList{ "Pass Rate", QString::number(report.passed()*100.0/qMax(1,report.total()),'f',1)+"%" };
        rows << QStringList{ "Total Time (ms)", QString::number(report.totalDurationMs(),'f',0) };
        rows << QStringList{ "Time", report.startTime.toString("yyyy-MM-dd hh:mm:ss") };
        rows << QStringList{ "Binary", report.testBinary };
        rows << QStringList{ "Filter", report.filterPattern };
        writer.addSheet("Statistics", h, rows);
    }
    {
        QStringList h = { "Suite", "Case", "Duration(ms)", "Stderr" };
        QVector<QStringList> rows;
        for (const auto& r : report.results) {
            if (r.passed()) continue;
            rows << QStringList{ r.testCase.suiteName, r.testCase.caseName,
                QString::number(r.durationMs,'f',1), r.rawStderr.left(500) };
        }
        if (!rows.isEmpty()) writer.addSheet("Failures", h, rows);
    }
    if (!writer.save(filePath)) {
        if (errorMsg) *errorMsg = writer.lastError();
        return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportToTxt(const TestReport& report, const QString& filePath, QString* errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { if (errorMsg) *errorMsg = file.errorString(); return false; }
    QTextStream out(&file); out.setEncoding(QStringConverter::Utf8);
    QString sep = QString(72, '=');
    out << sep << "\n  Test Report\n  Time: " << report.startTime.toString("yyyy-MM-dd hh:mm:ss")
        << "\n  Binary: " << report.testBinary << "\n  Filter: " << report.filterPattern
        << "\n" << sep << "\n\n--- Statistics ---\n"
        << "  Total: " << report.total() << "\n  Passed: " << report.passed()
        << "\n  Failed: " << report.failed()
        << "\n  Rate: " << QString::number(report.passed()*100.0/qMax(1,report.total()),'f',1) << "%"
        << "\n  Time: " << QString::number(report.totalDurationMs(),'f',0) << " ms\n\n";
    QSet<QString> allPropKeys;
    for (const auto& r : report.results) for (auto it = r.properties.begin(); it != r.properties.end(); ++it) allPropKeys.insert(it.key());
    QStringList propKeys = allPropKeys.values(); propKeys.sort();
    out << "--- All Results ---\n";
    out << QString("%1  %2  %3  %4").arg("Status",-8).arg("Duration",-12).arg("Suite",-30).arg("Case",-30);
    for (const auto& k : propKeys) out << "  " << QString("%1").arg(k,-20);
    out << "\n" << QString(72+propKeys.size()*22,'-') << "\n";
    for (const auto& r : report.results) {
        out << QString("%1  %2  %3  %4").arg(r.passed()?"[PASS]":"[FAIL]",-8)
            .arg(QString::number(r.durationMs,'f',1)+"ms",-12)
            .arg(r.testCase.suiteName,-30).arg(r.testCase.caseName,-30);
        for (const auto& k : propKeys) out << "  " << QString("%1").arg(r.properties.value(k,""),-20);
        out << "\n";
    }
    out << "\n--- " << (report.failed()>0?"Failures":"All Passed") << " ---\n";
    for (const auto& r : report.results) {
        if (r.passed()) continue;
        out << "\n[FAIL] " << r.testCase.fullName() << "  (" << QString::number(r.durationMs,'f',1) << " ms)\n";
        if (!r.rawStderr.isEmpty()) out << "  Stderr:\n" << r.rawStderr.left(1000) << "\n";
    }
    out << "\n" << sep << "\n  End of Report\n" << sep << "\n"; file.close(); return true;
}

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportBoth(const TestReport& report, const QString& basePath, QString* errorMsg)
{
    if (!exportToHtml(report, basePath + ".html", errorMsg)) return false;
    if (!exportToXlsx(report, basePath + ".xlsx", errorMsg)) {
        if (errorMsg) *errorMsg = "HTML ok, XLSX failed: " + *errorMsg;
        return false;
    }
    return true;
}
