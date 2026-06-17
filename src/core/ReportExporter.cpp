#include "ReportExporter.h"
#include "XlsxWriter.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QSet>

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportToXlsx(const TestReport& report,
                                   const QString& filePath,
                                   QString* errorMsg)
{
    XlsxWriter writer;

    // Sheet 1: 测试概要（含模型属性）
    {
        // 动态表头：基础列 + 所有出现的属性名
        QSet<QString> propKeys;
        for (const auto& r : report.results)
            for (auto it = r.properties.begin(); it != r.properties.end(); ++it)
                propKeys.insert(it.key());
        QStringList propKeysSorted = propKeys.values();
        propKeysSorted.sort();

        QStringList headers = { "Suite", "Case", "Status", "Duration(ms)" };
        for (const auto& k : propKeysSorted)
            headers << k;

        QVector<QStringList> rows;
        for (const auto& r : report.results) {
            QStringList row;
            row << r.testCase.suiteName
                << r.testCase.caseName
                << r.status
                << QString::number(r.durationMs, 'f', 1);
            for (const auto& k : propKeysSorted)
                row << r.properties.value(k, "");
            rows << row;
        }
        writer.addSheet("Test Summary", headers, rows);
    }

    // Sheet 2: 统计汇总
    {
        QStringList headers = { "Metric", "Value" };
        QVector<QStringList> rows;
        rows << QStringList{ "Total",     QString::number(report.total()) };
        rows << QStringList{ "Passed",    QString::number(report.passed()) };
        rows << QStringList{ "Failed",    QString::number(report.failed()) };
        rows << QStringList{ "Pass Rate", QString::number(
                report.passed() * 100.0 / qMax(1, report.total()), 'f', 1) + "%" };
        rows << QStringList{ "Total Time (ms)",
                QString::number(report.totalDurationMs(), 'f', 0) };
        rows << QStringList{ "Time",      report.startTime.toString("yyyy-MM-dd hh:mm:ss") };
        rows << QStringList{ "Binary",    report.testBinary };
        rows << QStringList{ "Filter",    report.filterPattern };
        writer.addSheet("Statistics", headers, rows);
    }

    // Sheet 3: 失败详情（仅当有失败时）
    {
        QStringList headers = { "Suite", "Case", "Duration(ms)", "Stderr" };
        QVector<QStringList> rows;
        for (const auto& r : report.results) {
            if (r.passed()) continue;
            rows << QStringList{
                r.testCase.suiteName,
                r.testCase.caseName,
                QString::number(r.durationMs, 'f', 1),
                r.rawStderr.left(500)
            };
        }
        if (!rows.isEmpty())
            writer.addSheet("Failures", headers, rows);
    }

    if (!writer.save(filePath)) {
        if (errorMsg) *errorMsg = writer.lastError();
        return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportToTxt(const TestReport& report,
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

    QString sep = QString(72, '=');

    // ── Header ──
    out << sep << "\n";
    out << "  Test Report\n";
    out << "  Time: " << report.startTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "  Binary: " << report.testBinary << "\n";
    out << "  Filter: " << report.filterPattern << "\n";
    out << sep << "\n\n";

    // ── Statistics ──
    out << "--- Statistics ---\n";
    out << QString("  Total:  %1\n").arg(report.total());
    out << QString("  Passed: %1\n").arg(report.passed());
    out << QString("  Failed: %1\n").arg(report.failed());
    out << QString("  Rate:   %1%\n")
               .arg(report.passed() * 100.0 / qMax(1, report.total()), 0, 'f', 1);
    out << QString("  Time:   %1 ms\n").arg(report.totalDurationMs(), 0, 'f', 0);
    out << "\n";

    // ── 收集所有属性 key ──
    QSet<QString> allPropKeys;
    for (const auto& r : report.results)
        for (auto it = r.properties.begin(); it != r.properties.end(); ++it)
            allPropKeys.insert(it.key());
    QStringList propKeys = allPropKeys.values();
    propKeys.sort();

    // ── All Results ──
    out << "--- All Results ---\n";
    out << QString("%1  %2  %3  %4")
               .arg("Status", -8)
               .arg("Duration", -12)
               .arg("Suite", -30)
               .arg("Case", -30);
    for (const auto& k : propKeys)
        out << "  " << QString("%1").arg(k, -20);
    out << "\n";
    out << QString(72 + propKeys.size() * 22, '-') << "\n";

    QString statusIcon;
    for (const auto& r : report.results) {
        statusIcon = r.passed() ? "[PASS]" : "[FAIL]";
        out << QString("%1  %2  %3  %4")
                   .arg(statusIcon, -8)
                   .arg(QString::number(r.durationMs, 'f', 1) + "ms", -12)
                   .arg(r.testCase.suiteName, -30)
                   .arg(r.testCase.caseName, -30);
        for (const auto& k : propKeys)
            out << "  " << QString("%1").arg(r.properties.value(k, ""), -20);
        out << "\n";
    }
    out << "\n";

    // ── Failures Detail ──
    bool hasFailures = false;
    for (const auto& r : report.results) {
        if (r.passed()) continue;
        if (!hasFailures) {
            out << "--- Failures ---\n";
            hasFailures = true;
        }
        out << QString("\n[FAIL] %1\n").arg(r.testCase.fullName());
        out << QString("  Duration: %1 ms\n").arg(r.durationMs, 0, 'f', 1);
        if (!r.rawStderr.isEmpty()) {
            out << "  Stderr:\n";
            for (const auto& line : r.rawStderr.split('\n'))
                out << "    " << line.trimmed() << "\n";
        }
    }

    if (!hasFailures)
        out << "--- No Failures ---\n";

    out << "\n" << sep << "\n";
    out << "  End of Report\n";
    out << sep << "\n";

    file.close();
    return true;
}

// ════════════════════════════════════════════════════════════
bool ReportExporter::exportBoth(const TestReport& report,
                                 const QString& basePath,
                                 QString* errorMsg)
{
    QString xlsxPath = basePath + ".xlsx";
    QString txtPath  = basePath + ".txt";

    if (!exportToXlsx(report, xlsxPath, errorMsg))
        return false;
    if (!exportToTxt(report, txtPath, errorMsg)) {
        if (errorMsg)
            *errorMsg = "XLSX ok, but TXT failed: " + *errorMsg;
        return false;
    }
    return true;
}
