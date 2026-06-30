#include <QApplication>
#include <QStyleFactory>
#include <QTimer>
#include <QFile>
#include <QCoreApplication>
#include "core/Logger.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    Logger::init();

    // 清空模型调试日志（每次启动重建）
    QFile::remove(QCoreApplication::applicationDirPath() + "/test_runner_ui_debug.log");

    app.setApplicationName("TestRunnerUI");
    app.setApplicationVersion("1.0.0");
    app.setStyle(QStyleFactory::create("Fusion"));

    // ── 现代扁平浅色 ──
    QPalette p;
    p.setColor(QPalette::Window,        QColor(0xf5, 0xf6, 0xf8));
    p.setColor(QPalette::WindowText,     QColor(0x1e, 0x29, 0x3b));
    p.setColor(QPalette::Base,           QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::AlternateBase,  QColor(0xf8, 0xf9, 0xfb));
    p.setColor(QPalette::ToolTipBase,    QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipText,    QColor(0x1e, 0x29, 0x3b));
    p.setColor(QPalette::Text,           QColor(0x1e, 0x29, 0x3b));
    p.setColor(QPalette::Button,         QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ButtonText,     QColor(0x1e, 0x29, 0x3b));
    p.setColor(QPalette::BrightText,     QColor(0xef, 0x44, 0x44));
    p.setColor(QPalette::Highlight,      QColor(0x63, 0x66, 0xf1));
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Link,           QColor(0x63, 0x66, 0xf1));
    app.setPalette(p);

    // ── 全局样式表 ──
    const char* darkStyle = R"(
        QMainWindow, QDialog { background:#f5f6f8; }

        QToolTip {
            border:1px solid #e2e8f0; border-radius:4px;
            padding:4px 10px; background:#ffffff; color:#1e293b; font-size:12px;
        }

        QMenuBar {
            background:#ffffff; border-bottom:1px solid #e2e8f0; padding:2px 6px;
        }
        QMenuBar::item {
            padding:6px 14px; color:#64748b; border-radius:4px; margin:1px;
        }
        QMenuBar::item:selected { background:#f1f5f9; color:#1e293b; }

        QMenu {
            background:#ffffff; border:1px solid #e2e8f0; border-radius:6px; padding:4px;
        }
        QMenu::item {
            padding:7px 28px 7px 14px; border-radius:4px; font-size:13px;
        }
        QMenu::item:selected { background:#f1f5f9; color:#6366f1; }
        QMenu::separator { height:1px; background:#e2e8f0; margin:4px 8px; }

        QStatusBar {
            background:#ffffff; border-top:1px solid #e2e8f0;
            color:#64748b; font-size:12px; padding:2px 10px;
        }
        QStatusBar::item { border:none; }

        /* ── 按钮：全局只设背景/边框，padding 由各按钮内联控制 ── */
        QPushButton {
            background:#ffffff; border:1px solid #e2e8f0; border-radius:6px;
            color:#1e293b;
        }
        QPushButton:hover { background:#f1f5f9; border-color:#cbd5e1; }
        QPushButton:disabled { background:#f8f9fb; color:#94a3b8; border-color:#e2e8f0; }
        QPushButton:checked { background:#eef2ff; border-color:#6366f1; color:#6366f1; }

        /* ── 树/列表/表格 ── */
        QTreeView, QListView, QTableView {
            background:#ffffff; border:1px solid #e2e8f0; border-radius:6px;
            selection-background-color:#eef2ff;
            selection-color:#1e293b;
            font-size:13px;
            outline:none;
        }
        QTreeView::item, QListView::item, QTableView::item {
            padding:6px 10px; min-height:28px;
        }
        QTreeView::item:hover, QListView::item:hover { background:#f8f9fb; }
        QHeaderView::section {
            background:#f8f9fb; color:#6366f1; border:none;
            border-bottom:1px solid #e2e8f0;
            padding:8px 12px; font-size:12px; font-weight:600;
        }

        /* ── 滚动条 ── */
        QScrollBar:vertical {
            background:transparent; width:6px; margin:0;
        }
        QScrollBar::handle:vertical {
            background:#cbd5e1; border-radius:3px; min-height:28px;
        }
        QScrollBar::handle:vertical:hover { background:#6366f1; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal {
            background:transparent; height:6px; margin:0;
        }
        QScrollBar::handle:horizontal {
            background:#cbd5e1; border-radius:3px; min-width:28px;
        }
        QScrollBar::handle:horizontal:hover { background:#6366f1; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }

        /* ── Splitter ── */
        QSplitter::handle { background:#e2e8f0; }
        QSplitter::handle:hover { background:#6366f1; }
        QSplitter::handle:horizontal { width:2px; }
        QSplitter::handle:vertical { height:2px; }

        QGroupBox {
            border:1px solid #e2e8f0; border-radius:6px;
            margin-top:16px; padding:14px 12px 10px;
        }
        QGroupBox::title {
            subcontrol-origin:margin; left:12px;
            padding:0 6px; color:#6366f1; font-weight:600; font-size:12px;
        }

        QLabel { color:#1e293b; font-size:13px; background:transparent; }

        QLineEdit, QSpinBox, QComboBox {
            background:#ffffff; border:1px solid #e2e8f0; border-radius:6px;
            padding:7px 10px; color:#1e293b; font-size:13px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color:#6366f1; }
        QComboBox::drop-down { border:none; width:20px; }
        QComboBox::down-arrow {
            image:none;
            border-left:4px solid transparent; border-right:4px solid transparent;
            border-top:5px solid #94a3b8; margin-right:4px;
        }
        QComboBox QAbstractItemView {
            background:#ffffff; border:1px solid #e2e8f0;
            selection-background-color:#eef2ff; selection-color:#1e293b;
        }

        QCheckBox, QRadioButton { color:#1e293b; font-size:13px; spacing:6px; }
        QCheckBox::indicator, QRadioButton::indicator {
            width:16px; height:16px; border-radius:3px;
            border:2px solid #cbd5e1; background:#ffffff;
        }
        QCheckBox::indicator:checked { background:#6366f1; border-color:#6366f1; }
        QCheckBox::indicator:hover { border-color:#6366f1; }

        QProgressBar {
            background:#f1f5f9; border:none; border-radius:4px;
            height:20px; text-align:center; font-size:11px; color:#64748b;
        }
        QProgressBar::chunk { background:#6366f1; border-radius:4px; }
    )";
    app.setStyleSheet(QString::fromUtf8(darkStyle));

    MainWindow w;
    w.show();

    if (argc > 1) {
        QString modelPath = QString::fromUtf8(argv[1]);
        QTimer::singleShot(500, [&w, modelPath](){ w.openModelFile(modelPath); });
    }

    // 窗口显示后重新应用样式表（触发按钮布局修正）
    QTimer::singleShot(0, [&app, darkStyle]() {
        app.setStyleSheet(QString::fromUtf8(darkStyle) + "/* relayout */");
    });
    return app.exec();
}
