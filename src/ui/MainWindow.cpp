#include "MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QTextEdit>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QCheckBox>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <functional>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QSplitter>

#include "core/Logger.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_runner(new TestRunner(this))
{
    setWindowTitle("Test Runner");
    resize(1280, 800);

    setupUi();
    setupMenu();
    setupConnections();

    LOG("APP", "Started");
    if (m_config.load()) {
        refreshProfileCombo();
        LOG("CFG", "Config loaded: " + m_config.configPath());
        LOG("LOAD", QString("ui state: geo=%1,%2 %3x%4 max=%5")
            .arg(m_config.uiState.windowX).arg(m_config.uiState.windowY)
            .arg(m_config.uiState.windowW).arg(m_config.uiState.windowH)
            .arg(m_config.uiState.maximized));
        // 恢复 UI 状态
        auto& ui = m_config.uiState;
        // 先存恢复值，再设显隐和几何（showEvent 中会用到这些值）
        m_restoreLW = ui.leftPanelVisible ? ui.splitterLeftPct : 0;
        m_restoreRW = ui.rightPanelVisible ? ui.splitterRightPct : 0;
        m_restoreVP = ui.splitterVPct;
        m_restoreVP2 = ui.splitterV2Pct;
        if (m_leftPanel) m_leftPanel->setVisible(ui.leftPanelVisible);
        if (m_rightPanel) m_rightPanel->setVisible(ui.rightPanelVisible);
        if (ui.windowX >= 0) {
            setGeometry(ui.windowX, ui.windowY, ui.windowW, ui.windowH);
            if (ui.maximized) showMaximized();
        }
    } else {
        LOG("CFG", "Config not found: " + m_config.configPath());
    }

    updateButtonStates();
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(4);

    m_mainSplitter = new QSplitter(Qt::Horizontal, central);
    m_mainSplitter->setHandleWidth(5);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setCollapsible(0, true);
    m_mainSplitter->setCollapsible(2, true);

    // ═══ 左：用例列表 ═══
    m_leftPanel = new QWidget;
    m_leftPanel->setMinimumWidth(0);
    auto* leftL = new QVBoxLayout(m_leftPanel);
    leftL->setContentsMargins(0, 0, 0, 0);
    m_testList = new TestListPanel(m_leftPanel);
    leftL->addWidget(m_testList, 1);
    connect(m_testList, &TestListPanel::collapseRequested, this, [=]() {
        m_leftPanel->setVisible(!m_leftPanel->isVisible());
    });

    // ═══ 中：进度(上) + 结果树(下) ═══
    auto* centerSplitter = new QSplitter(Qt::Vertical);
    m_progress = new TestProgressPanel;
    m_progress->setMinimumHeight(100);
    m_centerResultView = new ModelRenderView;
    m_centerSplitter = centerSplitter;
    centerSplitter->addWidget(m_progress);
    centerSplitter->addWidget(m_centerResultView);
    centerSplitter->setStretchFactor(0, 0);
    centerSplitter->setStretchFactor(1, 1);

    // ═══ 右：Model3DViewer(下) ═══
    m_rightPanel = new QWidget;
    m_rightPanel->setMinimumWidth(0);
    auto* rightL = new QVBoxLayout(m_rightPanel);
    rightL->setContentsMargins(0, 0, 0, 0);
    rightL->setSpacing(2);

    // 右栏：打开模型按钮
    auto* btnOpen = new QPushButton(QString::fromUtf8("\xF0\x9F\x93\x82 \xE6\x89\x93\xE5\xBC\x80\xE6\xA8\xA1\xE5\x9E\x8B"));
    btnOpen->setFixedHeight(28);
    btnOpen->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;padding:2px 10px;font-size:12px}QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}");
    rightL->addWidget(btnOpen);
    connect(btnOpen, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "选择模型文件",
            QString(), "模型文件 (*.step *.stp *.iges *.igs *.brep);;所有文件 (*)");
        if (!path.isEmpty()) {
            LOG("MODEL", "Open: " + path);
            m_model3D->loadFile(path);
        }
    });

    m_model3D = new Model3DViewer;
    rightL->addWidget(m_model3D, 1);

    // ── 组装 ──
    m_mainSplitter->addWidget(m_leftPanel);
    m_mainSplitter->addWidget(centerSplitter);
    m_mainSplitter->addWidget(m_rightPanel);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    mainLayout->addWidget(m_mainSplitter, 1);

    // ── 底栏 ──
    auto* bar = new QWidget; bar->setStyleSheet("background:#ffffff;border-top:1px solid #e2e8f0;");
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8, 3, 8, 3);
    bl->setSpacing(8);
    QString smallBtn = "QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;"
                       "padding:0;font-size:14px}QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}";
    QString mainBtn = "QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;"
                      "padding:4px 14px;font-size:13px}QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}";
    auto* bLeft = new QPushButton(QString::fromUtf8("\xe2\x97\x80"));
    bLeft->setFixedSize(30,28);bLeft->setStyleSheet(smallBtn);
    bLeft->setToolTip(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f \xe5\xb7\xa6\xe4\xbe\xa7\xe9\x9d\xa2\xe6\x9d\xbf"));
    connect(bLeft, &QPushButton::clicked, this, [=]() {
        bool vis = !m_leftPanel->isVisible();
        m_leftPanel->setVisible(vis);
        bLeft->setText(vis ? QString::fromUtf8("\xe2\x97\x80") : QString::fromUtf8("\xe2\x96\xb6"));
    });
    bl->addWidget(bLeft);
    auto* bCfg = new QPushButton(QString::fromUtf8("\u2699 \xE9\x85\x8D\xE7\xBD\xAE"));
    bCfg->setFixedHeight(30);bCfg->setStyleSheet(mainBtn);
    auto* bLd  = new QPushButton(QString::fromUtf8("\U0001F4C2 \xE5\x8A\xA0\xE8\xBD\xBD"));
    bLd->setFixedHeight(30);bLd->setStyleSheet(mainBtn);
    auto* bExp = new QPushButton(QString::fromUtf8("\U0001F4CA \xE5\xAF\xBC\xE5\x87\xBA"));
    bExp->setFixedHeight(30);bExp->setStyleSheet(mainBtn);
    auto* bRun = new QPushButton(QString::fromUtf8("\u25B6 \xE8\xBF\x90\xE8\xA1\x8C"));
    bRun->setFixedHeight(30);
    bRun->setStyleSheet(
        "QPushButton{background:#6366f1;color:white;border:none;border-radius:6px;"
        "font-size:13px;padding:4px 16px;font-weight:600}"
        "QPushButton:hover{background:#4f46e5}"
        "QPushButton:disabled{background:#cbd5e1;color:#94a3b8}");
    bl->addWidget(bCfg);

    // ── Profile 切换（菜单按钮） ──
    m_profileBtn = new QPushButton;
    m_profileBtn->setFixedHeight(30);
    m_profileBtn->setMinimumWidth(150);
    m_profileBtn->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;padding:2px 8px;font-size:12px;text-align:left}"
                                "QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1}");
    m_profileMenu = new QMenu(this);
    refreshProfileCombo();
    m_profileBtn->setMenu(m_profileMenu);
    bl->addWidget(m_profileBtn);
    bl->addWidget(bLd);
    bl->addStretch();
    bl->addWidget(bExp);
    bl->addWidget(bRun);

    auto* bRight = new QPushButton(QString::fromUtf8("\xe2\x96\xb6"));
    bRight->setFixedSize(30,28);bRight->setStyleSheet(smallBtn);
    bRight->setToolTip(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f \xe5\x8f\xb3\xe4\xbe\xa7\xe9\x9d\xa2\xe6\x9d\xbf"));
    connect(bRight, &QPushButton::clicked, this, [=]() {
        bool vis = !m_rightPanel->isVisible();
        m_rightPanel->setVisible(vis);
        bRight->setText(vis ? QString::fromUtf8("\xe2\x96\xb6") : QString::fromUtf8("\xe2\x97\x80"));
    });
    bl->addWidget(bRight);
    setCentralWidget(central);
    statusBar()->addPermanentWidget(bar, 1);

    connect(bCfg, &QPushButton::clicked, this, &MainWindow::onEditConfig);
    connect(bLd,  &QPushButton::clicked, this, &MainWindow::onLoadTests);
    connect(bExp, &QPushButton::clicked, this, &MainWindow::onExportReport);
    connect(bRun, &QPushButton::clicked, this, &MainWindow::onRunSelected);
}

void MainWindow::setupMenu() {
    auto* f = menuBar()->addMenu("\u6587\u4EF6(&F)");  // 文件(&F)
    m_actLoad   = f->addAction("\U0001F4C2 加载用例",    this, &MainWindow::onLoadTests);
    m_actRun    = f->addAction("\u25B6 运行选中",        this, &MainWindow::onRunSelected, QKeySequence("Ctrl+R"));
    m_actCancel = f->addAction("\u274C 取消运行",        this, &MainWindow::onCancelRun);
    m_actExport = f->addAction("\U0001F4CA 导出报告",    this, &MainWindow::onExportReport);
    f->addSeparator();
    m_actConfig = f->addAction("\u2699 配置", this, &MainWindow::onEditConfig);
    f->addSeparator();

    auto* v = menuBar()->addMenu(QString::fromUtf8("\xe8\xa7\x86\xe5\x9b\xbe(&V)"));
    v->addAction(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f \xe5\xb7\xa6\xe4\xbe\xa7\xe9\x9d\xa2\xe6\x9d\xbf"), this, [=](){ if(m_leftPanel)m_leftPanel->setVisible(!m_leftPanel->isVisible()); });
    v->addAction(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f \xe5\x8f\xb3\xe4\xbe\xa7\xe9\x9d\xa2\xe6\x9d\xbf"), this, [=](){ if(m_rightPanel)m_rightPanel->setVisible(!m_rightPanel->isVisible()); });
    v->addSeparator();

    // ── 主题选择 ──
    auto* themeMenu = v->addMenu(QString::fromUtf8("\xF0\x9F\x8E\xA8 \xe4\xb8\xbb\xe9\xa2\x98"));
    static int curTheme = 0;
    static const char* themeNames[] = {
        "\xe2\x98\x80\xef\xb8\x8f \xe4\xba\xae\xe8\x89\xb2",
        "\xf0\x9f\x8c\x99 \xe6\x9a\x97\xe8\x89\xb2",
        "\xf0\x9f\x94\xa5 \xe9\xab\x98\xe5\xaf\xb9\xe6\xaf\x94"
    };
    static const char* themeStyles[] = {
        R"(
            QMainWindow,QDialog{background:#f5f6f8}
            QMenuBar{background:#ffffff;border-bottom:1px solid #e2e8f0}
            QMenuBar::item:selected{background:#f1f5f9;color:#1e293b}
            QMenu{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px}
            QMenu::item:selected{background:#f1f5f9;color:#6366f1}
            QMenu::separator{height:1px;background:#e2e8f0}
            QStatusBar{background:#ffffff;border-top:1px solid #e2e8f0;color:#64748b}
            QToolTip{background:#ffffff;border:1px solid #e2e8f0;color:#1e293b}
            QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;padding:6px 14px;color:#1e293b}
            QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1}
            QPushButton:disabled{background:#f8f9fb;color:#94a3b8;border-color:#e2e8f0}
            QPushButton:checked{background:#eef2ff;border-color:#6366f1;color:#6366f1}
            QTreeView,QListView,QTableView{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;selection-background-color:#eef2ff;selection-color:#1e293b}
            QTreeView::item:hover,QListView::item:hover{background:#f8f9fb}
            QHeaderView::section{background:#f8f9fb;color:#6366f1;border-bottom:1px solid #e2e8f0}
            QLineEdit,QSpinBox,QComboBox{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;color:#1e293b}
            QLineEdit:focus{border-color:#6366f1}
            QLabel{color:#1e293b}
            QProgressBar{background:#f1f5f9;border:none;border-radius:4px;color:#64748b}
            QProgressBar::chunk{background:#6366f1;border-radius:4px}
            QGroupBox{border:1px solid #e2e8f0;border-radius:6px}
            QGroupBox::title{color:#6366f1}
            QScrollBar::handle:vertical,QScrollBar::handle:horizontal{background:#cbd5e1;border-radius:3px}
            QScrollBar::handle:vertical:hover,QScrollBar::handle:horizontal:hover{background:#6366f1}
            QCheckBox::indicator,QRadioButton::indicator{border:2px solid #cbd5e1;background:#ffffff}
            QCheckBox::indicator:checked{background:#6366f1;border-color:#6366f1}
            QSplitter::handle{background:#e2e8f0}
            QSplitter::handle:hover{background:#6366f1}
        )",
        R"(
            QMainWindow,QDialog{background:#0d0e12}
            QMenuBar{background:#16181e;border-bottom:1px solid #2a2d38}
            QMenuBar::item{color:#8892a6}QMenuBar::item:selected{background:#282a34;color:#e2e8f0}
            QMenu{background:#1e2028;border:1px solid #2a2d38;border-radius:6px}
            QMenu::item{color:#8892a6}QMenu::item:selected{background:#323540;color:#e2e8f0}
            QMenu::separator{height:1px;background:#2a2d38}
            QStatusBar{background:#16181e;border-top:1px solid #2a2d38;color:#8892a6}
            QToolTip{background:#1e2028;border:1px solid #818cf8;color:#e2e8f0}
            QPushButton{background:#1e2028;border:1px solid #2a2d38;border-radius:6px;padding:6px 14px;color:#e2e8f0;font-weight:500}
            QPushButton:hover{background:#282a34;border-color:#818cf8;color:#818cf8}
            QPushButton:disabled{background:#16181e;color:#5a6278;border-color:#1e2028}
            QPushButton:checked{background:#323540;border-color:#818cf8;color:#818cf8}
            QTreeView,QListView,QTableView{background:#1e2028;border:1px solid #2a2d38;border-radius:6px;color:#e2e8f0;selection-background-color:#323540;selection-color:#e2e8f0}
            QTreeView::item:hover,QListView::item:hover{background:#282a34}
            QHeaderView::section{background:#282a34;color:#818cf8;border-bottom:1px solid #2a2d38}
            QLineEdit,QSpinBox,QComboBox{background:#1e2028;border:1px solid #2a2d38;border-radius:6px;color:#e2e8f0}
            QLineEdit:focus{border-color:#818cf8}
            QLabel{color:#e2e8f0}
            QProgressBar{background:#1e2028;border:none;border-radius:4px;color:#8892a6}
            QProgressBar::chunk{background:#818cf8;border-radius:4px}
            QGroupBox{border:1px solid #2a2d38;border-radius:6px}
            QGroupBox::title{color:#818cf8}
            QScrollBar::handle:vertical,QScrollBar::handle:horizontal{background:#323540;border-radius:3px}
            QScrollBar::handle:vertical:hover,QScrollBar::handle:horizontal:hover{background:#818cf8}
            QCheckBox::indicator,QRadioButton::indicator{border:2px solid #2a2d38;background:#1e2028}
            QCheckBox::indicator:checked{background:#818cf8;border-color:#818cf8}
            QSplitter::handle{background:#2a2d38}QSplitter::handle:hover{background:#818cf8}
        )",
        R"(
            QMainWindow,QDialog{background:#000000}
            QMenuBar{background:#000000;border-bottom:2px solid #ffff00}
            QMenuBar::item{color:#ffffff}QMenuBar::item:selected{background:#ffff00;color:#000000}
            QMenu{background:#000000;border:2px solid #ffffff;border-radius:4px}
            QMenu::item{color:#ffffff}QMenu::item:selected{background:#ffff00;color:#000000}
            QStatusBar{background:#000000;border-top:2px solid #ffff00;color:#ffffff}
            QPushButton{background:#000000;border:2px solid #ffffff;border-radius:4px;padding:6px 14px;color:#ffffff;font-weight:bold}
            QPushButton:hover{background:#ffffff;color:#000000}
            QPushButton:disabled{color:#666666;border-color:#666666}
            QPushButton:checked{background:#ffff00;color:#000000;border-color:#ffff00}
            QTreeView,QListView,QTableView{background:#000000;border:2px solid #ffffff;border-radius:4px;color:#ffffff;selection-background-color:#ffff00;selection-color:#000000}
            QTreeView::item:hover{background:#333333}
            QHeaderView::section{background:#000000;color:#ffff00;border-bottom:2px solid #ffff00}
            QLineEdit,QSpinBox,QComboBox{background:#000000;border:2px solid #ffffff;border-radius:4px;color:#ffffff}
            QLineEdit:focus{border-color:#ffff00}
            QLabel{color:#ffffff}
            QProgressBar{background:#000000;border:2px solid #ffffff;border-radius:4px;color:#ffffff}
            QProgressBar::chunk{background:#ffff00;border-radius:2px}
            QGroupBox{border:2px solid #ffffff;border-radius:4px}
            QGroupBox::title{color:#ffff00}
            QScrollBar::handle:vertical,QScrollBar::handle:horizontal{background:#ffffff;border-radius:2px}
            QScrollBar::handle:vertical:hover,QScrollBar::handle:horizontal:hover{background:#ffff00}
            QCheckBox::indicator,QRadioButton::indicator{border:2px solid #ffffff;background:#000000}
            QCheckBox::indicator:checked{background:#ffff00;border-color:#ffff00}
            QSplitter::handle{background:#ffffff}QSplitter::handle:hover{background:#ffff00}
        )"
    };
    QVector<QAction*> themeActs;
    for (int i = 0; i < 3; i++) {
        auto* act = themeMenu->addAction(QString::fromUtf8(themeNames[i]));
        act->setCheckable(true);
        themeActs.append(act);
        if (i == 0) act->setChecked(true);
    }
    for (int i = 0; i < 3; i++) {
        connect(themeActs[i], &QAction::triggered, this, [themeActs, i]() {
            curTheme = i;
            for (auto* a : themeActs) a->setChecked(false);
            themeActs[i]->setChecked(true);
            if (i == 0)
                qApp->setStyleSheet(QString::fromUtf8(themeStyles[0]));  // 亮色 = 初始样式表
            else if (i == 1)
                qApp->setStyleSheet(QString::fromUtf8(themeStyles[1]));  // 暗色
            else
                qApp->setStyleSheet(QString::fromUtf8(themeStyles[2]));  // 高对比
        });
    }

    f->addAction(QString::fromUtf8("\xe9\x80\x80\xe5\x87\xba(&Q)"), qApp, &QApplication::quit, QKeySequence::Quit);
    auto* h = menuBar()->addMenu("\u5E2E\u52A9(&H)");  // 帮助(&H)
    h->addAction("\u5173\u4E8E", this, &MainWindow::onAbout);  // 关于
}

void MainWindow::setupConnections() {
    connect(m_runner, &TestRunner::testFinished,   this, &MainWindow::onTestFinished);
    connect(m_runner, &TestRunner::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_runner, &TestRunner::allFinished,     this, &MainWindow::onAllFinished);
    connect(m_runner, &TestRunner::rawOutput,       this, &MainWindow::onRawOutput);
    connect(m_runner, &TestRunner::errorOccurred,   this, [this](const QString& m) {
        LOG("ERR", m);
        QMessageBox::warning(this, "Error", m);
    });
    connect(m_testList, &TestListPanel::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_progress, &TestProgressPanel::cancelRequested, this, &MainWindow::onCancelRun);

    // 中栏结果树选中 → 更新右栏 ModelInfo
    connect(m_centerResultView, &ModelRenderView::openModelFile, this, [this](const QString& path) {
        m_model3D->loadFile(path);
    });
    connect(m_centerResultView, &ModelRenderView::toggleHighlight, this, [this](const QVector<int>& ids, bool on) {
        m_model3D->highlightFaces(on ? ids : QVector<int>{});
    });
    connect(m_centerResultView, &ModelRenderView::toggleHighlightBoxes, this, [this](const QString& propKey, const QVector<QVector<double>>& boxes, bool on) {
        m_model3D->highlightFacesInBoxes(propKey, boxes, on);
    });
    connect(m_model3D, &Model3DViewer::boxesResolved, this, [this](const QString& propKey, const QString& displayText) {
        m_centerResultView->updatePropertyText(propKey, displayText);
    });
    connect(m_centerResultView, &ModelRenderView::resultSelected, this, [this](const TestRunResult& r) {
        if (r.properties.contains("model"))
            m_model3D->loadFile(r.properties["model"]);
        else
            m_model3D->clear();
        QVector<int> hl;
        auto parseIds=[&](const QString& key){ QString v=r.properties.value(key); if(!v.isEmpty()){ for(auto& s:v.split(',',Qt::SkipEmptyParts)){ bool ok; int id=s.trimmed().toInt(&ok); if(ok) hl.append(id); } } };
        parseIds("searchResult");parseIds("removeResult");
        m_model3D->highlightFaces(hl);
    });

}


void MainWindow::updateButtonStates() {
    bool running = m_runner->isRunning();
    m_actRun->setEnabled(!running && !m_loader.testCases().isEmpty());
    m_actLoad->setEnabled(!running);
    m_actCancel->setEnabled(running);
    m_actExport->setEnabled(!m_report.results.isEmpty());
}

// ────────────────────────────────────────────────────────────

void MainWindow::onLoadTests() {
    QString binary = m_config.testBinary();
    if (binary.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"), QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe5\x9c\xa8\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\xad\xe8\xae\xbe\xe7\xbd\xae exe \xe8\xb7\xaf\xe5\xbe\x84"));
        onEditConfig();
        return;
    }

    // 把当前进程的工作目录切到 exe 所在目录，子进程继承后 DLL 搜索优先
    QDir::setCurrent(QFileInfo(binary).absolutePath());
    LOG("LOAD", "Binary: " + binary);
    LOG("LOAD", "WorkDir: " + m_config.workingDir());

    statusBar()->showMessage("Loading...");
    QApplication::processEvents();

    if (m_loader.load(binary, m_config.extraArgs(), m_config.workingDir(), m_config.currentProfile().dependencies, m_config.currentProfile().envVars)) {
        int n = m_loader.testCases().size();
        LOG("LOAD", "OK, found: " + QString::number(n) + " tests");
        m_suiteNames = m_loader.groupedBySuite().keys();
        m_testList->loadTests(m_loader.testCases(), m_config.categories());
        m_centerResultView->clear();
        m_report = {};
        statusBar()->showMessage(QString("Loaded %1 tests").arg(n), 5000);
    } else {
        LOG("LOAD", "FAILED: " + m_loader.lastError());
        QMessageBox::warning(this, "Load Failed", m_loader.lastError());
        statusBar()->showMessage("Load failed", 3000);
    }
    updateButtonStates();
}

void MainWindow::onRunSelected() {
    auto sel = m_testList->selectedTests();
    if (sel.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"), QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe6\xb5\x8b\xe8\xaf\x95\xe7\x94\xa8\xe4\xbe\x8b"));
        return;
    }

    int actualRunCount = sel.size();
    // 优化 filter：全选所有用例 → "*"
    bool allSelected = sel.size() == m_loader.testCases().size();
    if (allSelected) {
        sel.clear();
        TestCase all; all.suiteName = "*"; all.caseName = "*";
        sel.append(all);
        LOG("RUN", "All tests selected, filter=*");
    } else {
        // 按套件分组，如果某套件下所有用例都选中 → 用 Suite.*
        auto allCases = m_loader.groupedBySuite();
        QMap<QString, int> selCount;
        for (const auto& tc : sel) selCount[tc.suiteName]++;
        QVector<TestCase> optimized;
        for (auto it = allCases.begin(); it != allCases.end(); ++it) {
            if (selCount.value(it.key()) == it.value().size()) {
                TestCase suiteAll;
                suiteAll.suiteName = it.key();
                suiteAll.caseName = "*";
                optimized.append(suiteAll);
            } else {
                // 只加选中的单个用例
                for (const auto& tc : sel)
                    if (tc.suiteName == it.key()) optimized.append(tc);
            }
        }
        sel = optimized;
        LOG("RUN", "Optimized filter: " + QString::number(sel.size()) + " entries");
    }

    LOG("RUN", "Selected", QString::number(sel.size()) + " tests");

    m_report = {};
    m_report.startTime = QDateTime::currentDateTime();
    m_report.testBinary = m_config.testBinary();
    m_report.filterPattern = allSelected ? "*" : "custom";

    m_centerResultView->clear();
    m_progress->startRun(actualRunCount);
    m_runner->run(m_config.testBinary(), sel, m_config.extraArgs(), m_config.workingDir(),
                  m_config.currentProfile().dependencies, m_config.currentProfile().envVars,
                  actualRunCount);
    updateButtonStates();
}

void MainWindow::onCancelRun() {
    m_runner->cancel();
    m_progress->appendLog("\n[CANCELLED]");
    m_progress->finishRun();
    updateButtonStates();
}

void MainWindow::onExportReport() {
    if (m_report.results.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba"), QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe5\x8f\xaf\xe5\xaf\xbc\xe5\x87\xba\xe7\x9a\x84\xe7\xbb\x93\xe6\x9e\x9c"));
        return;
    }
    QString exeDir = QCoreApplication::applicationDirPath();
    QString outDir = exeDir + "/output";
    QDir().mkpath(outDir);
    QString base = outDir + "/test_report_"
                   + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    QString err;
    if (ReportExporter::exportBoth(m_report, base, &err)) {
        statusBar()->showMessage("Exported: " + base + ".xlsx/.txt", 5000);
        auto r = QMessageBox::question(this, QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe5\xae\x8c\xe6\x88\x90"),
            QString::fromUtf8("\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98:\n  ") + base + ".xlsx\n  " + base + ".txt\n\n" + QString::fromUtf8("\xe6\x89\x93\xe5\xbc\x80\xe6\x96\x87\xe4\xbb\xb6\xe5\xa4\xb9?"));
        if (r == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl::fromLocalFile(outDir));
    } else {
        QMessageBox::critical(this, "Export Failed", err);
    }
}

// 分类树编辑委托——确保内联编辑器高度足够显示全部字符
class CatDelegate : public QStyledItemDelegate {
public:
    CatDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override {
        auto* ed = new QLineEdit(parent);
        ed->setFont(QFont("Microsoft YaHei UI", 13));
        ed->setFixedHeight(34);
        return ed;
    }
};

void MainWindow::onEditConfig() {
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe9\x85\x8d\xe7\xbd\xae"));
    dlg.resize(m_config.uiState.cfgDialogW, m_config.uiState.cfgDialogH);
    dlg.setMinimumWidth(580);
    dlg.setStyleSheet("QDialog{background:#ffffff;border-radius:12px}");
    auto* lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(16, 12, 16, 12);

    // Profile 选择器（按钮+菜单）
    auto* profRow = new QHBoxLayout;
    auto* profLabel = new QLabel(QString::fromUtf8("\xe9\x85\x8d\xe7\xbd\xae:"));
    profLabel->setStyleSheet("font-weight:600;font-size:13px");
    auto* dlgProfBtn = new QPushButton;
    dlgProfBtn->setMinimumWidth(180);
    dlgProfBtn->setFixedHeight(30);
    dlgProfBtn->setStyleSheet("QPushButton{font-size:13px;padding:2px 8px;background:#ffffff;border:1px solid #cbd5e1;border-radius:4px;text-align:left}"
                              "QPushButton:hover{background:#f8f9fb;border-color:#6366f1}");
    auto* dlgProfMenu = new QMenu(&dlg);
    dlgProfMenu->setStyleSheet("QMenu{font-size:13px;background:#ffffff;border:1px solid #cbd5e1;border-radius:6px;padding:4px}"
                               "QMenu::item{padding:6px 20px;border-radius:4px}"
                               "QMenu::item:selected{background:#eef2ff;color:#1e293b}");
    // 用 std::function 以支持 forward reference
    QTextEdit* edEnv = nullptr; // forward decl
    std::function<void(int)> loadProfile;
    // 当前编辑的 profile 索引（不修改全局 activeProfile，避免影响其他逻辑）
    int currentEditIdx = m_config.activeProfile();
    auto fillMenu = [&]() {
        dlgProfMenu->clear();
        dlgProfBtn->setText(m_config.profiles()[currentEditIdx].name);
        for (int i = 0; i < m_config.profiles().size(); i++) {
            auto* act = dlgProfMenu->addAction(m_config.profiles()[i].name);
            act->setEnabled(i != currentEditIdx);
            connect(act, &QAction::triggered, &dlg, [this, i, &loadProfile, dlgProfBtn, &currentEditIdx]() {
                currentEditIdx = i;
                dlgProfBtn->setText(m_config.profiles()[i].name);
                if (loadProfile) loadProfile(i);
            });
        }
    };
    fillMenu();
    connect(dlgProfBtn, &QPushButton::clicked, &dlg, [&dlg, dlgProfBtn, dlgProfMenu]() {
        dlgProfMenu->exec(dlgProfBtn->mapToGlobal(QPoint(0, dlgProfBtn->height())));
    });
    auto* btnNewP = new QPushButton(QString::fromUtf8("\xe6\x96\xb0\xe5\xbb\xba"));
    btnNewP->setFixedHeight(30);
    btnNewP->setStyleSheet("QPushButton{font-size:12px;padding:2px 12px;background:#ffffff;border:1px solid #cbd5e1;border-radius:4px}QPushButton:hover{background:#f1f5f9;border-color:#6366f1}");
    auto* btnDelP = new QPushButton(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"));
    btnDelP->setFixedHeight(30);
    btnDelP->setStyleSheet("QPushButton{font-size:12px;padding:2px 12px;background:#ffffff;border:1px solid #fecaca;border-radius:4px;color:#dc2626}QPushButton:hover{background:#fef2f2;border-color:#ef4444}");
    profRow->addWidget(profLabel);
    profRow->addWidget(dlgProfBtn, 1);
    profRow->addWidget(btnNewP);
    profRow->addWidget(btnDelP);
    lay->addLayout(profRow);

    // 表单字段
    auto* tabs = new QTabWidget;
    auto* profileTab = new QWidget;
    auto* pf = new QFormLayout(profileTab);
    pf->setLabelAlignment(Qt::AlignRight);
    auto* edName = new QLineEdit;
    edName->setToolTip(QString::fromUtf8("\xe9\x85\x8d\xe7\xbd\xae\xe5\x90\x8d\xef\xbc\x8c\xe7\x94\xa8\xe4\xba\x8e\xe5\x9c\xa8\xe4\xb8\x8b\xe6\x8b\x89\xe6\xa0\x8f\xe4\xb8\xad\xe8\xaf\x86\xe5\x88\xab"));
    auto* edBinary = new QLineEdit;
    edBinary->setToolTip(QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95 exe \xe7\x9a\x84\xe5\xae\x8c\xe6\x95\xb4\xe8\xb7\xaf\xe5\xbe\x84"));
    auto* btnBrowseBin = new QPushButton(QString::fromUtf8("\xe6\xb5\x8f\xe8\xa7\x88..."));
    auto* binaryRow = new QHBoxLayout;
    binaryRow->addWidget(edBinary, 1);
    binaryRow->addWidget(btnBrowseBin);
    auto* edDeps = new QTextEdit;
    edDeps->setMaximumHeight(100);
    edDeps->setToolTip(QString::fromUtf8("\xe6\xaf\x8f\xe8\xa1\x8c\xe4\xb8\x80\xe4\xb8\xaa\xe7\x9b\xae\xe5\xbd\x95\xef\xbc\x8c\xe5\x90\xaf\xe5\x8a\xa8 exe \xe6\x97\xb6\xe8\x87\xaa\xe5\x8a\xa8\xe5\x8a\xa0\xe5\x88\xb0 PATH"));
    auto* btnBrowseDep = new QPushButton(QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe7\x9b\xae\xe5\xbd\x95..."));
    auto* depsRow = new QVBoxLayout;
    auto* depsTop = new QHBoxLayout;
    depsTop->addWidget(edDeps, 1);
    depsTop->addWidget(btnBrowseDep);
    depsRow->addLayout(depsTop);
    auto* edWorkDir = new QLineEdit;
    edWorkDir->setToolTip(QString::fromUtf8("\xe5\xb7\xa5\xe4\xbd\x9c\xe7\x9b\xae\xe5\xbd\x95\xef\xbc\x8c\xe7\x95\x99\xe7\xa9\xba\xe5\x88\x99\xe7\x94\xa8 exe \xe6\x89\x80\xe5\x9c\xa8\xe7\x9b\xae\xe5\xbd\x95"));
    auto* edArgs = new QLineEdit;
    edArgs->setToolTip(QString::fromUtf8("\xe4\xbc\xa0\xe9\x80\x92\xe7\xbb\x99 gtest \xe7\x9a\x84\xe9\xa2\x9d\xe5\xa4\x96\xe5\x8f\x82\xe6\x95\xb0\xef\xbc\x8c\xe5\xa6\x82 --gtest_also_run_disabled_tests"));
    pf->addRow(QString::fromUtf8("\xe9\x85\x8d\xe7\xbd\xae\xe5\x90\x8d"), edName);
    pf->addRow(QString::fromUtf8("Exe \xe8\xb7\xaf\xe5\xbe\x84"), binaryRow);
    pf->addRow(QString::fromUtf8("\xe4\xbe\x9d\xe8\xb5\x96\xe8\xb7\xaf\xe5\xbe\x84"), depsRow);
    pf->addRow(QString::fromUtf8("\xe5\xb7\xa5\xe4\xbd\x9c\xe7\x9b\xae\xe5\xbd\x95"), edWorkDir);
    pf->addRow(QString::fromUtf8("\xe9\xa2\x9d\xe5\xa4\x96\xe5\x8f\x82\xe6\x95\xb0"), edArgs);
    profileTab->setLayout(pf);
    tabs->addTab(profileTab, QString::fromUtf8("\xe9\x85\x8d\xe7\xbd\xae"));

    // 分类
    // 分类标签页（从已加载的套件列表中选择前缀）
    auto* catTab = new QWidget;
    auto* catLay = new QVBoxLayout(catTab);
    auto* catTree = new QTreeWidget(catTab);
    catTree->setHeaderLabels({QString::fromUtf8("\xe5\x88\x86\xe7\xb1\xbb"), QString::fromUtf8("\xe5\x8c\xb9\xe9\x85\x8d\xe5\xa5\x97\xe4\xbb\xb6")});
    catTree->setRootIsDecorated(false);
    catTree->setSelectionMode(QAbstractItemView::SingleSelection);
    catTree->setStyleSheet("QTreeWidget::item{padding:8px 12px;min-height:36px;font-size:14px}");
    catLay->addWidget(catTree, 1);
    // 双击打开套件选择对话框
    connect(catTree, &QTreeWidget::itemDoubleClicked, &dlg, [&](QTreeWidgetItem* item, int) {
        if (!item) return;
        QDialog selDlg(&dlg);
        selDlg.setWindowTitle(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe5\x8c\xb9\xe9\x85\x8d\xe5\xa5\x97\xe4\xbb\xb6"));
        selDlg.resize(400, 500);
        auto* sl = new QVBoxLayout(&selDlg);
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        auto* sw = new QWidget;
        auto* swl = new QVBoxLayout(sw);
        QString raw = item->text(1);
        if (raw.startsWith(QString::fromUtf8("\xe2\x98\x91"))) raw = raw.mid(2).trimmed();
        QStringList oldPrefs;
        for (const auto& p : raw.split(',', Qt::SkipEmptyParts))
            oldPrefs << p.trimmed();
        QVector<QCheckBox*> checks;
        for (const auto& sn : m_suiteNames) {
            auto* cb = new QCheckBox(sn);
            cb->setChecked(oldPrefs.contains(sn));
            checks.append(cb);
            swl->addWidget(cb);
        }
        if (m_suiteNames.isEmpty())
            swl->addWidget(new QLabel(QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe5\x8a\xa0\xe8\xbd\xbd\xe6\xb5\x8b\xe8\xaf\x95")));
        swl->addStretch();
        scroll->setWidget(sw);
        sl->addWidget(scroll, 1);
        // 全选/取消按钮
        auto* selAllRow = new QHBoxLayout;
        auto* btnSelAll = new QPushButton(QString::fromUtf8("\xe5\x85\xa8\xe9\x80\x89"));
        auto* btnSelNone = new QPushButton(QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88\xe5\x85\xa8\xe9\x80\x89"));
        selAllRow->addWidget(btnSelAll);
        selAllRow->addWidget(btnSelNone);
        selAllRow->addStretch();
        sl->addLayout(selAllRow);
        connect(btnSelAll, &QPushButton::clicked, [&checks]() { for (auto* cb : checks) cb->setChecked(true); });
        connect(btnSelNone, &QPushButton::clicked, [&checks]() { for (auto* cb : checks) cb->setChecked(false); });
        auto* sb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(sb, &QDialogButtonBox::accepted, &selDlg, &QDialog::accept);
        connect(sb, &QDialogButtonBox::rejected, &selDlg, &QDialog::reject);
        sl->addWidget(sb);
        if (selDlg.exec() == QDialog::Accepted) {
            QStringList sel;
            for (auto* cb : checks) if (cb->isChecked()) sel << cb->text();
            item->setText(1, sel.join(", "));
        }
    });
    auto* catBtns = new QHBoxLayout;
    auto* btnAddCat = new QPushButton(QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0"));
    auto* btnDelCat = new QPushButton(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"));
    catBtns->addWidget(btnAddCat);
    catBtns->addWidget(btnDelCat);
    catBtns->addStretch();
    catLay->addLayout(catBtns);

    // 加载 profile 数据
    loadProfile = [&](int idx) {
        if (idx < 0 || idx >= m_config.profiles().size()) return;
        const auto& p = m_config.profiles()[idx];
        edName->setText(p.name);
        edBinary->setText(p.testBinary);
        edDeps->setText(p.dependencies.join("\n"));
        edWorkDir->setText(p.workingDir);
        edArgs->setText(p.extraArgs.join(" "));
        QStringList envLines;
        for (auto it = p.envVars.begin(); it != p.envVars.end(); ++it)
            envLines << it.key() + "=" + it.value();
        edEnv->setText(envLines.join("\n"));
        catTree->clear();
        for (const auto& c : p.categories) {
            auto* item = new QTreeWidgetItem(catTree);
            item->setText(0, c.name);
            item->setText(1, c.prefixes.join(", "));
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        }
    };

    // 环境变量标签页
    auto* envTab = new QWidget;
    auto* envLay = new QVBoxLayout(envTab);
    auto* envHelp = new QLabel(QString::fromUtf8("\xe6\xaf\x8f\xe8\xa1\x8c\xe4\xb8\x80\xe4\xb8\xaa KEY=VALUE\xef\xbc\x8c\xe4\xbe\x8b\xe5\xa6\x82:"));
    envHelp->setStyleSheet("color:#64748b;font-size:12px");
    envHelp->setWordWrap(true);
    envLay->addWidget(envHelp);
    auto* envExplain = new QLabel("QT_QPA_PLATFORM_PLUGIN_PATH=D:/sdk/plugins\nPYTHONPATH=D:/sdk/python");
    envExplain->setStyleSheet("color:#94a3b8;font-size:11px;padding:0 0 4px 0");
    envLay->addWidget(envExplain);
    edEnv = new QTextEdit;
    edEnv->setPlaceholderText("KEY=VALUE");
    edEnv->setMaximumHeight(200);
    envLay->addWidget(edEnv, 1);
    tabs->addTab(envTab, QString::fromUtf8("\xe7\x8e\xaf\xe5\xa2\x83\xe5\x8f\x98\xe9\x87\x8f"));
    tabs->addTab(catTab, QString::fromUtf8("\xe5\x88\x86\xe7\xb1\xbb"));

    // 连接
    connect(btnBrowseBin, &QPushButton::clicked, [&]() {
        QString p = QFileDialog::getOpenFileName(nullptr, QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9 exe"), edBinary->text(), "*.exe");
        if (!p.isEmpty()) edBinary->setText(p);
    });
    connect(btnBrowseDep, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(nullptr, QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe4\xbe\x9d\xe8\xb5\x96\xe7\x9b\xae\xe5\xbd\x95"));
        if (!dir.isEmpty()) {
            QString cur = edDeps->toPlainText().trimmed();
            edDeps->setText(cur.isEmpty() ? dir : cur + "\n" + dir);
        }
    });
    connect(btnNewP, &QPushButton::clicked, [&]() {
        ExeProfile p;
        p.name = QString::fromUtf8("\xe6\x96\xb0\xe9\x85\x8d\xe7\xbd\xae %1").arg(m_config.profiles().size() + 1);
        TestCategory c1, c2;
        c1.name = "test_p*"; c1.prefixes << "test_p";
        c2.name = QString::fromUtf8("\xe5\x85\xb6\xe4\xbb\x96");
        p.categories << c1 << c2;
        m_config.addProfile(p);
        m_config.setActiveProfile(m_config.profiles().size() - 1);
        currentEditIdx = m_config.activeProfile();
        fillMenu();
        loadProfile(currentEditIdx);
    });
    connect(btnDelP, &QPushButton::clicked, [&]() {
        if (m_config.profiles().size() <= 1) {
            QMessageBox::warning(&dlg, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"), QString::fromUtf8("\xe8\x87\xb3\xe5\xb0\x91\xe4\xbf\x9d\xe7\x95\x99\xe4\xb8\x80\xe4\xb8\xaa\xe9\x85\x8d\xe7\xbd\xae"));
            return;
        }
        m_config.removeProfile(currentEditIdx);
        if (currentEditIdx >= m_config.profiles().size())
            currentEditIdx = 0;
        m_config.setActiveProfile(currentEditIdx);
        fillMenu();
        loadProfile(currentEditIdx);
    });
    connect(btnAddCat, &QPushButton::clicked, [&]() {
        auto* item = new QTreeWidgetItem(catTree);
        item->setText(0, QString::fromUtf8("\xe6\x96\xb0\xe5\x88\x86\xe7\xb1\xbb"));
        item->setText(1, "");
        // 自动弹出套件选择
        QTimer::singleShot(0, [&, item]() {
            // 触发双击逻辑（通过直接调用）
            QDialog selDlg(&dlg);
            selDlg.setWindowTitle(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe5\x8c\xb9\xe9\x85\x8d\xe5\xa5\x97\xe4\xbb\xb6"));
            selDlg.resize(400, 500);
            auto* sl = new QVBoxLayout(&selDlg);
            auto* scroll = new QScrollArea;
            scroll->setWidgetResizable(true);
            auto* sw = new QWidget;
            auto* swl = new QVBoxLayout(sw);
            QVector<QCheckBox*> checks;
            for (const auto& sn : m_suiteNames) {
                auto* cb = new QCheckBox(sn); checks.append(cb); swl->addWidget(cb);
            }
            if (m_suiteNames.isEmpty())
                swl->addWidget(new QLabel(QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe5\x8a\xa0\xe8\xbd\xbd\xe6\xb5\x8b\xe8\xaf\x95")));
            swl->addStretch();
            scroll->setWidget(sw);
            sl->addWidget(scroll, 1);
            auto* selAllRow = new QHBoxLayout;
            auto* btnSelAll = new QPushButton(QString::fromUtf8("\xe5\x85\xa8\xe9\x80\x89"));
            auto* btnSelNone = new QPushButton(QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88\xe5\x85\xa8\xe9\x80\x89"));
            selAllRow->addWidget(btnSelAll);
            selAllRow->addWidget(btnSelNone);
            selAllRow->addStretch();
            sl->addLayout(selAllRow);
            connect(btnSelAll, &QPushButton::clicked, [&checks]() { for (auto* cb : checks) cb->setChecked(true); });
            connect(btnSelNone, &QPushButton::clicked, [&checks]() { for (auto* cb : checks) cb->setChecked(false); });
            auto* sb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            connect(sb, &QDialogButtonBox::accepted, &selDlg, &QDialog::accept);
            connect(sb, &QDialogButtonBox::rejected, &selDlg, &QDialog::reject);
            sl->addWidget(sb);
            if (selDlg.exec() == QDialog::Accepted) {
                QStringList sel;
                for (auto* cb : checks) if (cb->isChecked()) sel << cb->text();
                item->setText(1, sel.join(", "));
            }
        });
    });
    connect(btnDelCat, &QPushButton::clicked, [&]() {
        for (auto* s : catTree->selectedItems()) delete s;
    });

    lay->addWidget(tabs);

    // 加载当前 profile
    loadProfile(currentEditIdx);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnBox->setStyleSheet("QPushButton{padding:6px 28px;min-width:90px;font-size:13px}");
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btnBox);

    // 保存对话框尺寸
    m_config.uiState.cfgDialogW = dlg.width();
    m_config.uiState.cfgDialogH = dlg.height();

    if (dlg.exec() == QDialog::Accepted) {
        int idx = currentEditIdx;
        if (idx >= 0 && idx < m_config.profiles().size()) {
            ExeProfile p = m_config.profiles()[idx];
            p.name = edName->text().trimmed();
            p.testBinary = edBinary->text().trimmed();
            QStringList deps;
            for (const auto& line : edDeps->toPlainText().split('\n', Qt::SkipEmptyParts))
                deps << line.trimmed();
            p.dependencies = deps;
            p.workingDir = edWorkDir->text().trimmed();
            p.extraArgs = edArgs->text().trimmed().split(' ', Qt::SkipEmptyParts);
            QVector<TestCategory> cats;
            for (int i = 0; i < catTree->topLevelItemCount(); i++) {
                auto* item = catTree->topLevelItem(i);
                TestCategory c;
                c.name = item->text(0);
                for (const auto& pr : item->text(1).split(',', Qt::SkipEmptyParts)) {
                    QString p = pr.trimmed();
                    if (p.startsWith(QString::fromUtf8("\xe2\x98\x91"))) p = p.mid(2).trimmed();
                    if (!p.isEmpty()) c.prefixes << p;
                }
                if (!c.name.isEmpty()) cats << c;
            }
            p.categories = cats;
            p.envVars.clear();
            for (const auto& line : edEnv->toPlainText().split('\n', Qt::SkipEmptyParts)) {
                int eq = line.indexOf('=');
                if (eq > 0) p.envVars[line.left(eq).trimmed()] = line.mid(eq+1).trimmed();
            }
            m_config.updateProfile(idx, p);
            m_config.setActiveProfile(idx);
            m_config.save();
            refreshProfileCombo();
        }
    }
}


void MainWindow::onAbout() {
    QMessageBox::about(this, "About",
        "<h3>Test Runner UI</h3>"
        "<p>gtest GUI frontend with model rendering.</p>");
}

void MainWindow::onTestFinished(const TestRunResult& result) {
    auto parsed = ResultParser::parse(
        result.testCase, result.rawStdout, result.rawStderr,
        result.durationMs, result.status);
    // 复制 TestRunner 从 XML 解析出的 RecordProperty
    parsed.properties = result.properties;
    if (!parsed.properties.isEmpty()) {
        LOG("PROP", "Copied " + QString::number(parsed.properties.size()) + " properties");
        for (auto it = parsed.properties.begin(); it != parsed.properties.end(); ++it)
            LOG("PROP", "  " + it.key() + " = " + it.value());
    }
    m_report.results.append(parsed);
}

void MainWindow::onProgressUpdated(int done, int total) {
    m_progress->updateProgress(done, total);
}

void MainWindow::onAllFinished() {
    m_report.endTime = QDateTime::currentDateTime();
    m_progress->finishRun();
    m_centerResultView->showResults(m_report.results);

    int p = m_report.passed(), f = m_report.failed();
    LOG("RUN", "Done: passed=" + QString::number(p) + " failed=" + QString::number(f));
    statusBar()->showMessage(
        QString("Done! Passed=%1 Failed=%2 Total=%3 ms")
            .arg(p).arg(f).arg(m_report.totalDurationMs(), 0, 'f', 0), 10000);
    updateButtonStates();
    if (f > 0) m_progress->appendLog(QString("\n%1 tests failed.").arg(f));
}

void MainWindow::onRawOutput(const QString& line) {
    m_progress->appendLog(line.trimmed());
    if (line.length() > 5)  // 避免刷太多 log 文件
        LOG("COUT", line.left(200).trimmed());
}

void MainWindow::onSelectionChanged(int count) {
    updateButtonStates();
}

void MainWindow::refreshProfileCombo() {
    if (!m_profileBtn || !m_profileMenu) return;
    m_profileBtn->setText(m_config.currentProfile().name);
    m_profileMenu->clear();
    int active = m_config.activeProfile();
    for (int i = 0; i < m_config.profiles().size(); i++) {
        const auto& p = m_config.profiles()[i];
        auto* act = m_profileMenu->addAction(p.name.isEmpty() ? QString::fromUtf8("\xe6\x9c\xaa\xe5\x91\xbd\xe5\x90\x8d") : p.name);
        act->setCheckable(true);
        if (i == active) act->setChecked(true);
        connect(act, &QAction::triggered, this, [this, i]() {
            m_config.setActiveProfile(i);
            refreshProfileCombo();
        });
    }
}



void MainWindow::saveLayout() {
    auto& ui = m_config.uiState;
    if (isMaximized()) { ui.maximized = true; }
    else {
        ui.maximized = false;
        auto geo = geometry();
        ui.windowX = geo.x(); ui.windowY = geo.y();
        ui.windowW = geo.width(); ui.windowH = geo.height();
    }
    if (m_mainSplitter) { auto s = m_mainSplitter->sizes(); if (s.size()>=3) { int w = s[0]+s[1]+s[2]; if (w>0) { ui.splitterLeftPct=qRound(s[0]*100.0/w); ui.splitterRightPct=qRound(s[2]*100.0/w); } } }
    if (m_centerSplitter) { auto s = m_centerSplitter->sizes(); if (s.size()>=2) { int h = s[0]+s[1]; if (h>0) ui.splitterVPct = qRound(s[0]*100.0/h); } }
    if (m_centerResultView) { int bp = m_centerResultView->saveBottomSplitPos(); int total = m_centerResultView->height(); if (total>0) ui.splitterV2Pct = qRound(bp*100.0/total); }
    if (m_leftPanel) ui.leftPanelVisible = m_leftPanel->isVisible();
    if (m_rightPanel) ui.rightPanelVisible = m_rightPanel->isVisible();
    m_config.save();
    // 保存时输出当前 splitter 实际像素值
    if (m_mainSplitter) {
        auto sz = m_mainSplitter->sizes();
        if (sz.size()>=3) LOG("SAVE", QString("SIZES main=[%1,%2,%3] sum=%4 pct=[%5,%6,%7]")
            .arg(sz[0]).arg(sz[1]).arg(sz[2]).arg(sz[0]+sz[1]+sz[2])
            .arg(ui.splitterLeftPct).arg(100-ui.splitterLeftPct-ui.splitterRightPct).arg(ui.splitterRightPct));
    }
    if (m_centerSplitter) {
        auto sz = m_centerSplitter->sizes();
        if (sz.size()>=2) LOG("SAVE", QString("SIZES center=[%1,%2] sum=%3 pct=[%4,%5]")
            .arg(sz[0]).arg(sz[1]).arg(sz[0]+sz[1]).arg(ui.splitterVPct).arg(100-ui.splitterVPct));
    }
    LOG("SAVE", QString("geo=%1,%2 %3x%4 max=%5 Lpct=%6 Rpct=%7 Vpct=%8 V2pct=%9")
        .arg(ui.windowX).arg(ui.windowY).arg(ui.windowW).arg(ui.windowH)
        .arg(ui.maximized).arg(ui.splitterLeftPct).arg(ui.splitterRightPct)
        .arg(ui.splitterVPct).arg(ui.splitterV2Pct));
}

MainWindow::~MainWindow() {
    // 析构时不保存，closeEvent 中已完成且数据有效
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    // 窗口首次显示后，用 setSizes 恢复 splitter 比例
    if (m_restoreLW > 0 || m_restoreRW > 0 || m_restoreVP > 0 || m_restoreVP2 > 0) {
        QTimer::singleShot(200, [this]() {
            if (m_mainSplitter && m_restoreLW > 0) {
                auto s = m_mainSplitter->sizes();
                if (s.size() >= 3) {
                    int cw = s[0] + s[1] + s[2];
                    int lw = qRound(cw * m_restoreLW / 100.0);
                    int rw = qRound(cw * m_restoreRW / 100.0);
                    LOG("RESTORE", QString("BEFORE main=[%1,%2,%3] sum=%4 savedPct=[%5,%6]")
                        .arg(s[0]).arg(s[1]).arg(s[2]).arg(cw).arg(m_restoreLW).arg(m_restoreRW));
                    LOG("RESTORE", QString("TARGET main=[%1,%2,%3]").arg(lw).arg(cw-lw-rw).arg(rw));
                    if (lw + rw + 20 < cw) {
                        m_mainSplitter->setSizes({lw, cw - lw - rw, rw});
                        {
                            auto a = m_mainSplitter->sizes();
                            if (a.size()>=3) LOG("RESTORE", QString("AFTER  main=[%1,%2,%3] sum=%4")
                                .arg(a[0]).arg(a[1]).arg(a[2]).arg(a[0]+a[1]+a[2]));
                        }
                        // 不再修改 stretch factor，保持 setupUi 的 (0,1,0)
                    }
                }
            }
            if (m_centerSplitter && m_restoreVP > 0) {
                auto s = m_centerSplitter->sizes();
                if (s.size() >= 2) {
                    int ch = s[0] + s[1];
                    int ph = qBound(60, qRound(ch * m_restoreVP / 100.0), ch - 60);
                    LOG("RESTORE", QString("CENTER before=[%1,%2] sum=%3 savedPct=%4 target=[%5,%6]")
                        .arg(s[0]).arg(s[1]).arg(ch).arg(m_restoreVP).arg(ph).arg(ch-ph));
                    m_centerSplitter->setSizes({ph, ch - ph});
                    {
                        auto a = m_centerSplitter->sizes();
                        if (a.size()>=2) LOG("RESTORE", QString("CENTER after=[%1,%2]").arg(a[0]).arg(a[1]));
                    }
                    m_centerSplitter->setStretchFactor(0, m_restoreVP);
                    m_centerSplitter->setStretchFactor(1, 100 - m_restoreVP);
                }
            }
            if (m_centerResultView && m_restoreVP2 > 0) {
                int h = m_centerResultView->height();
                if (h > 0)
                    m_centerResultView->restoreBottomSplitPos(qRound(h * m_restoreVP2 / 100.0));
            }
            m_restoreLW = m_restoreRW = m_restoreVP = m_restoreVP2 = 0;
        });
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    saveLayout();
    QMainWindow::closeEvent(e);
}
