#pragma once

#include "models/TestResult.h"
#include <QString>

class ReportExporter {
public:
    static bool exportToXlsx(const TestReport& report, const QString& filePath, QString* errorMsg = nullptr);
    static bool exportToTxt(const TestReport& report, const QString& filePath, QString* errorMsg = nullptr);
    static bool exportToHtml(const TestReport& report, const QString& filePath, QString* errorMsg = nullptr);
    static bool exportBoth(const TestReport& report, const QString& basePath, QString* errorMsg = nullptr);
};
