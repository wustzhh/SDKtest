#include "Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QTextStream>

static QFile* g_file = nullptr;

void Logger::init() {
    if (g_file) return;
    QString logPath = QCoreApplication::applicationDirPath() + "/test_runner_ui.log";
    g_file = new QFile(logPath);
    if (g_file->open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(g_file);
        out << "=== test_runner_ui log ===\n"
            << "Started: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n"
            << "Exe dir: " << QCoreApplication::applicationDirPath() << "\n"
            << "===========================\n\n";
        out.flush();
    }
}

void Logger::write(const QString& tag, const QString& msg) {
    if (!g_file || !g_file->isOpen()) return;
    QTextStream out(g_file);
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    out << ts << "  [" << tag << "]  " << msg << "\n";
    out.flush();
}

void Logger::write(const QString& tag, const QString& key, const QString& val) {
    write(tag, key + " = " + val);
}
