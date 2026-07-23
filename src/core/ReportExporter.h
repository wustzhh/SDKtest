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

    // 加载 data 目录下所有 JSON，按 binary 去重（保留最新），返回 entries
    static QVector<QPair<TestReport, QString>> loadAllData(const QString& dataDir,
                                                            QString* errorMsg = nullptr);
};