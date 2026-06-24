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

    // ── 科幻风格 渐变布局 ──
    QPalette p;
    p.setColor(QPalette::Window,        QColor(0xf0, 0xf2, 0xf8));
    p.setColor(QPalette::WindowText,     QColor(0x1a, 0x1a, 0x2e));
    p.setColor(QPalette::Base,           QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::AlternateBase,  QColor(0xf5, 0xf7, 0xfd));
    p.setColor(QPalette::ToolTipBase,    QColor(0x6c, 0x5c, 0xe7));
    p.setColor(QPalette::ToolTipText,    QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Text,           QColor(0x1a, 0x1a, 0x2e));
    p.setColor(QPalette::Button,         QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ButtonText,     QColor(0x1a, 0x1a, 0x2e));
    p.setColor(QPalette::BrightText,     QColor(0xff, 0x47, 0x57));
    p.setColor(QPalette::Highlight,      QColor(0x6c, 0x5c, 0xe7));
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Link,           QColor(0x6c, 0x5c, 0xe7));
    app.setPalette(p);

    app.setStyleSheet(QString(
        /* 全局背景渐变 */
        "QMainWindow{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #e8ecf8,stop:1 #f0f2f8)}"
        /* 工具栏提示 */
        "QToolTip{border:1px solid #6c5ce7;border-radius:6px;padding:5px 10px;background:#6c5ce7;color:white;font-size:12px}"
        /* 菜单栏 */
        "QMenuBar{background:rgba(255,255,255,0.7);border-bottom:1px solid rgba(108,92,231,0.2);padding:2px 4px}"
        "QMenuBar::item{padding:7px 16px;color:#1a1a2e;border-radius:8px;margin:2px 1px;font-weight:500}"
        "QMenuBar::item:selected{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #6c5ce7,stop:1 #a29bfe);color:white}"
        /* 菜单 */
        "QMenu{background:rgba(255,255,255,0.95);border:1px solid rgba(108,92,231,0.15);border-radius:12px;padding:6px;backdrop-filter:blur(10px)}"
        "QMenu::item{padding:9px 32px 9px 18px;border-radius:8px;margin:2px 4px;font-size:13px}"
        "QMenu::item:selected{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #6c5ce7,stop:1 #a29bfe);color:white}"
        "QMenu::separator{height:1px;background:rgba(108,92,231,0.1);margin:6px 12px}"
        /* 状态栏 */
        "QStatusBar{background:rgba(255,255,255,0.6);border-top:1px solid rgba(108,92,231,0.1);color:#576574;font-size:12px;padding:2px 8px}"
        /* 按钮 — 毛玻璃风格 */
        "QPushButton{background:rgba(255,255,255,0.7);border:1px solid rgba(108,92,231,0.2);border-radius:10px;padding:6px 14px;color:#1a1a2e;font-size:13px;font-weight:500;min-width:60px}"
        "QPushButton:hover{background:rgba(108,92,231,0.1);border-color:#6c5ce7;color:#6c5ce7}"
        "QPushButton:pressed{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #6c5ce7,stop:1 #a29bfe);color:white;border-color:#6c5ce7}"
        "QPushButton:disabled{background:rgba(200,200,210,0.4);color:#999;border-color:transparent}"
        /* 树/列表/表格 — 磨砂白 */
        "QTreeView,QListView,QTableView{background:rgba(255,255,255,0.75);border:1px solid rgba(108,92,231,0.12);border-radius:12px;alternate-background-color:rgba(108,92,231,0.04);selection-background-color:rgba(108,92,231,0.12);selection-color:#1a1a2e;font-size:13px;padding:2px}"
        "QTreeView::item,QListView::item,QTableView::item{padding:5px 8px;border-radius:4px}"
        "QTreeView::item:hover,QListView::item:hover{background:rgba(108,92,231,0.06)}"
        "QHeaderView::section{background:rgba(108,92,231,0.06);color:#6c5ce7;border:none;border-bottom:1px solid rgba(108,92,231,0.1);padding:9px 12px;font-size:11px;font-weight:600}"
        /* 滚动条 */
        "QScrollBar:vertical{background:transparent;width:6px;margin:0}"
        "QScrollBar::handle:vertical{background:rgba(108,92,231,0.25);border-radius:3px;min-height:30px}"
        "QScrollBar::handle:vertical:hover{background:#6c5ce7}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0}"
        "QScrollBar:horizontal{background:transparent;height:6px;margin:0}"
        "QScrollBar::handle:horizontal{background:rgba(108,92,231,0.25);border-radius:3px;min-width:30px}"
        "QScrollBar::handle:horizontal:hover{background:#6c5ce7}"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0}"
        /* 分割器 */
        "QSplitter::handle{background:rgba(108,92,231,0.1);margin:2px 0}"
        "QSplitter::handle:hover{background:rgba(108,92,231,0.3)}"
        "QSplitter::handle:horizontal{width:4px}"
        "QSplitter::handle:vertical{height:4px}"
        /* GroupBox */
        "QGroupBox{border:1px solid rgba(108,92,231,0.15);border-radius:12px;margin-top:18px;padding:16px 14px 12px;font-weight:500;background:rgba(255,255,255,0.4)}"
        "QGroupBox::title{subcontrol-origin:margin;left:14px;padding:0 8px;color:#6c5ce7;font-weight:600;font-size:12px}"
        /* 标签 */
        "QLabel{color:#1a1a2e;font-size:13px}"
        /* 输入框 */
        "QLineEdit,QSpinBox,QComboBox{background:rgba(255,255,255,0.7);border:1px solid rgba(108,92,231,0.15);border-radius:8px;padding:6px 10px;color:#1a1a2e;font-size:13px}"
        "QLineEdit:focus,QSpinBox:focus,QComboBox:focus{border-color:#6c5ce7}"
        /* 复选框/单选框 */
        "QCheckBox::indicator,QRadioButton::indicator{width:18px;height:18px;border-radius:4px;border:2px solid rgba(108,92,231,0.3)}"
        "QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#6c5ce7;border-color:#6c5ce7}"
        /* 进度条 */
        "QProgressBar{background:rgba(255,255,255,0.5);border:1px solid rgba(108,92,231,0.1);border-radius:8px;height:16px;text-align:center;font-size:11px;color:#6c5ce7}"
        "QProgressBar::chunk{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #6c5ce7,stop:1 #a29bfe);border-radius:7px}"
    ));

    MainWindow w;
    w.show();

    if (argc > 1) {
        QString modelPath = QString::fromUtf8(argv[1]);
        QTimer::singleShot(500, [&w, modelPath](){ w.openModelFile(modelPath); });
    }

    return app.exec();
}
