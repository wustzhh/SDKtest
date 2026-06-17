#pragma once

#include "models/TestResult.h"
#include "XlsxWriter.h"

#include <QString>

// ────────────────────────────────────────────────────────────
//  将 TestReport 导出为 XLSX 报告
// ────────────────────────────────────────────────────────────
class ReportExporter {
public:
    // 导出完整报告到 XLSX
    static bool exportToXlsx(const TestReport& report,
                             const QString& filePath,
                             QString* errorMsg = nullptr);

    // 导出 TXT 文本报告
    static bool exportToTxt(const TestReport& report,
                            const QString& filePath,
                            QString* errorMsg = nullptr);

    // 同时导出 XLSX + TXT（文件名自动加后缀）
    static bool exportBoth(const TestReport& report,
                           const QString& basePath,  // 不含扩展名
                           QString* errorMsg = nullptr);
};
