#pragma once

#include "models/TestResult.h"
#include <QString>

class ReportExporter {
public:
    static bool exportRun(const TestReport& report,
                          const QString& htmlDir,
                          const QString& runName,
                          QString* errorMsg = nullptr);
};