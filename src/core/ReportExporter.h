#pragma once

#include "models/TestResult.h"
#include <QString>
#include <QVector>
#include <QPair>

class ReportExporter {
public:
    static bool exportRun(const TestReport& report,
                          const QString& htmlDir,
                          const QString& runName,
                          QString* errorMsg = nullptr);

    // 从多条数据记录重建完整 HTML（一次写入，避免重复追加）
    static bool rebuildHtml(const QVector<QPair<TestReport, QString>>& entries,
                            const QString& htmlDir,
                            QString* errorMsg = nullptr);

    // 仅保存 JSON 数据文件（不操作 HTML）
    static bool saveJson(const TestReport& report,
                         const QString& htmlDir,
                         const QString& runName,
                         QString* errorMsg = nullptr);
};