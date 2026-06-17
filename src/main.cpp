#include <QApplication>
#include <QStyleFactory>
#include "core/Logger.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    Logger::init();

    app.setApplicationName("TestRunnerUI");
    app.setApplicationVersion("1.0.0");
    app.setStyle(QStyleFactory::create("Fusion"));

    // 深色 Fusion 调色板
    QPalette p = app.palette();
    p.setColor(QPalette::Window, QColor(0xF5, 0xF5, 0xF5));
    p.setColor(QPalette::WindowText, QColor(0x33, 0x33, 0x33));
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::AlternateBase, QColor(0xF0, 0xF0, 0xF0));
    p.setColor(QPalette::Highlight, QColor(0x21, 0x96, 0xF3));
    p.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(p);

    MainWindow w;
    w.show();

    return app.exec();
}
