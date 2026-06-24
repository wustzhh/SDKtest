#include <QApplication>
#include <QStyleFactory>
#include <QTimer>
#include "core/Logger.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
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

    // 命令行参数：自动加载模型
    if (argc > 1) {
        QString modelPath = QString::fromUtf8(argv[1]);
        QTimer::singleShot(500, [&w, modelPath](){ w.openModelFile(modelPath); });
    }

    return app.exec();
}
