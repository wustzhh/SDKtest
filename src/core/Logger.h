#pragma once

#include <QString>
#include <QFile>
#include <QMutex>

// ────────────────────────────────────────────────────────────
//  简单文件日志器，输出到 exe 同级的 test_runner_ui.log
// ────────────────────────────────────────────────────────────
class Logger {
public:
    static void init();
    static void write(const QString& tag, const QString& msg);
    static void write(const QString& tag, const QString& key, const QString& val);

private:
    static QFile* s_file;
    static QMutex s_mutex;
    static bool   s_ready;
};

// 便捷宏
#define LOG(tag, ...)    Logger::write(tag, __VA_ARGS__)
