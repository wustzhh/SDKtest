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
#include <QSet>
#include <QVector>
#include <QPair>
#include <QEventLoop>
#include <QRegularExpression>
#include <QProgressDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QClipboard>
#include <QKeyEvent>
#include <QShortcut>

#include "ui/FilterEditDialog.h"
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
        refreshScenarioCombo();
        // 恢复线宽设置
        m_model3D->glViewer()->setEdgeWidthPct(m_config.uiState.edgeWidthPct);
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
            QString(), "模型文件 (*.step *.stp *.iges *.igs *.brep *.nas *.bdf *.dat);;所有文件 (*)");
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
    auto* bExp = new QPushButton(QString::fromUtf8("\U0001F4CA \xE6\x9F\xA5\xE7\x9C\x8B\xE7\xBB\x93\xE6\x9E\x9C"));
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
    refreshScenarioCombo();
    bl->addWidget(m_profileBtn);
    bl->addWidget(new QLabel(QString::fromUtf8("\xe6\x96\xb9\xe6\xa1\x88:")));
    m_scenarioCombo = new QComboBox;
    m_scenarioCombo->setMinimumWidth(120);
    m_scenarioCombo->setStyleSheet("QComboBox{background:#fff;border:1px solid #e2e8f0;border-radius:4px;padding:2px 8px;height:26px;font-size:12px}");
    bl->addWidget(m_scenarioCombo);
    // 逐个运行模式
    m_chkSingleTest = new QCheckBox(QString::fromUtf8("\xe9\x80\x90\xe4\xb8\xaa"));
    m_chkSingleTest->setToolTip(QString::fromUtf8("\xe6\xaf\x8f\xe4\xb8\xaa\xe7\x94\xa8\xe4\xbe\x8b\xe5\x8d\x95\xe7\x8b\xac\xe8\xbf\x90\xe8\xa1\x8c\xef\xbc\x8c\xe5\xae\x9a\xe4\xbd\x8d\xe5\xb4\xa9\xe6\xba\x83\xe7\x94\xa8\xe4\xbe\x8b"));
    m_chkSingleTest->setStyleSheet("QCheckBox{font-size:12px;color:#64748b;}QCheckBox::indicator{width:18px;height:18px;}");
    // 切换时即时保存到当前方案（未选方案时存到首个）
    QObject::connect(m_chkSingleTest, &QCheckBox::toggled, this, [this](bool checked) {
        int idx = m_scenarioCombo ? m_scenarioCombo->currentIndex() - 1 : -1;
        auto& scs = m_config.currentProfile().scenarios;
        if (idx < 0 && !scs.isEmpty()) idx = 0;
        if (idx >= 0 && idx < scs.size()) {
            scs[idx].singleTest = checked;
            m_config.save();
        }
    });
    bl->addWidget(m_chkSingleTest);
    QObject::connect(m_scenarioCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
        if (idx <= 0) return; // 第一项是占位提示
        auto& prof = m_config.currentProfile();
        if (idx-1 < prof.scenarios.size()) {
            const auto& sc = prof.scenarios[idx-1];
            m_testList->setSelectedTestNames(sc.selectedTests);
            if (m_chkSingleTest) m_chkSingleTest->setChecked(sc.singleTest);
        }
    });

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
    m_actExport = f->addAction("\U0001F4CA \xe6\x9f\xa5\xe7\x9c\x8b\xe7\xbb\x93\xe6\x9e\x9c", this, &MainWindow::onExportReport);
    f->addSeparator();
    m_actConfig = f->addAction("\u2699 配置", this, &MainWindow::onEditConfig);
    f->addSeparator();

    auto* v = menuBar()->addMenu(QString::fromUtf8("\xe8\xa7\x86\xe5\x9b\xbe(&V)"));
    v->addAction(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f \xe5\xb7\xa6\xe4\xbe\xa7\xe9\x9d\xa2\xe6\x9d\xbf"), this, [=](){ if(m_leftPanel)m_leftPanel->setVisible(!m_leftPanel->isVisible()); });
    v->addAction(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f \xe5\x8f\xb3\xe4\xbe\xa7\xe9\x9d\xa2\xe6\x9d\xbf"), this, [=](){ if(m_rightPanel)m_rightPanel->setVisible(!m_rightPanel->isVisible()); });
    v->addSeparator();

    // ── 线宽（屏幕宽度百分比）──
    auto* lwMenu = v->addMenu(QString::fromUtf8("\xe7\xba\xbf\xe5\xae\xbd"));
    for (float pct : {0.05f, 0.1f, 0.15f, 0.2f, 0.3f, 0.5f}) {
        auto* act = lwMenu->addAction(QString("%1%").arg(pct, 0, 'f', 2));
        act->setCheckable(true);
        if (qAbs(pct - 0.1f) < 0.001f) act->setChecked(true);
        connect(act, &QAction::triggered, this, [this, pct, lwMenu]() {
            m_model3D->glViewer()->setEdgeWidthPct(pct);
            m_config.uiState.edgeWidthPct = pct; m_config.save();
            for (auto* a : lwMenu->actions()) a->setChecked(false);
            qobject_cast<QAction*>(sender())->setChecked(true);
        });
    }
    lwMenu->addSeparator();
    lwMenu->addAction(QString::fromUtf8("\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89..."), this, [this, lwMenu]() {
        bool ok;
        double val = QInputDialog::getDouble(this, QString::fromUtf8("\xe7\xba\xbf\xe5\xae\xbd"),
            QString::fromUtf8("\xe5\xb1\x8f\xe5\xb9\x95\xe5\xae\xbd\xe5\xba\xa6\xe7\x99\xbe\xe5\x88\x86\xe6\xaf\x94 (0.01-2.0):"),
            m_model3D->glViewer()->edgeWidthPct(), 0.01, 2.0, 2, &ok);
        if (ok) {
            m_model3D->glViewer()->setEdgeWidthPct((float)val);
            m_config.uiState.edgeWidthPct = (float)val; m_config.save();
            for (auto* a : lwMenu->actions()) a->setChecked(false);
        }
    });

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
    QVector<TestCase> originalSel = sel;  // 保存优化前的完整用例列表（含 DISABLED）
    // 按套件优化 filter：全选所有用例或部分选中都走同一路径
    // 每个套件若全选则用 "Suite.*"，否则逐个添加
    bool allSelected = sel.size() == m_loader.testCases().size();
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
            for (const auto& tc : sel)
                if (tc.suiteName == it.key()) optimized.append(tc);
        }
    }
    sel = optimized;
    LOG("RUN", QString(allSelected ? "All tests selected" : "Partial selected")
        + ", optimized filter: " + QString::number(sel.size()) + " entries");

    LOG("RUN", "Selected", QString::number(sel.size()) + " tests");

    m_report = {};
    m_report.startTime = QDateTime::currentDateTime();
    m_report.testBinary = m_config.testBinary();
    m_report.filterPattern = allSelected ? "*" : "custom";

    m_centerResultView->clear();
    m_progress->startRun(actualRunCount);
    bool singleMode = m_chkSingleTest && m_chkSingleTest->isChecked();
    // 保存到当前选中的方案
    int scIdx = m_scenarioCombo ? m_scenarioCombo->currentIndex() - 1 : -1;
    if (scIdx >= 0 && scIdx < m_config.currentProfile().scenarios.size()) {
        m_config.currentProfile().scenarios[scIdx].singleTest = singleMode;
        m_config.save();
    }
    QVector<TestCase> runCases = singleMode ? originalSel : sel;
    m_runner->run(m_config.testBinary(), runCases, m_config.extraArgs(), m_config.workingDir(),
                  m_config.currentProfile().dependencies, m_config.currentProfile().envVars,
                  actualRunCount, originalSel, singleMode);
    updateButtonStates();
}

void MainWindow::onCancelRun() {
    m_runner->cancel();      // 现在会同步触发 allFinished → onAllFinished（finishRun + updateButtonStates）
    m_progress->appendLog("\n[CANCELLED]");   // 在 onAllFinished 的 finishRun 之后追加取消标记
}

void MainWindow::captureAllModelScreenshots(const QString& screenshotDir) {
    QDir().mkpath(screenshotDir);
    
    // 收集所有唯一模型路径
    QSet<QString> modelPaths;
    for (const auto& result : m_report.results) {
        if (result.properties.contains("model"))
            modelPaths.insert(result.properties["model"]);
        if (result.properties.contains("resultModel"))
            modelPaths.insert(result.properties["resultModel"]);
    }
    
    if (modelPaths.isEmpty()) return;
    
    // 进度对话框
    QProgressDialog prog(QString::fromUtf8("\u622a\u56fe\u6a21\u578b\u56fe\u7247..."),
                         QString::fromUtf8("\u53d6\u6d88"), 0, modelPaths.size(), this);
    prog.setWindowTitle(QString::fromUtf8("\u751f\u6210\u62a5\u544a"));
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);
    prog.setValue(0);
    
    int idx = 0;
    // 对每个唯一模型截图（软件渲染，快速同步，无 OpenGL）
    for (const auto& path : modelPaths) {
        if (prog.wasCanceled()) break;
        
        QString baseName = QFileInfo(path).completeBaseName();
        prog.setLabelText(QString::fromUtf8("\u6b63\u5728\u622a\u56fe: %1").arg(baseName));
        QApplication::processEvents();
        
        // GL 截图：加载到现有渲染器，grab 真实渲染效果
        QImage img;
        if (QFile::exists(path)) {
            QEventLoop loop;
            QTimer::singleShot(30000, &loop, &QEventLoop::quit);
            bool loaded = false;
            QMetaObject::Connection conn = connect(m_model3D, &Model3DViewer::modelLoaded,
                [&]() { loaded = true; loop.quit(); });
            m_model3D->loadFile(path);
            if (!loaded) loop.exec();
            QObject::disconnect(conn);
            if (loaded) {
                QImage raw = m_model3D->glViewer()->grab().toImage();
                if (!raw.isNull())
                    img = raw.scaled(800, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
        if (img.isNull()) continue;
        
        // 保存
        QString safeName = QFileInfo(path).completeBaseName();
        safeName.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
        QString fileName = safeName + "_" +
            QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch()) + ".png";
        img.save(QDir::fromNativeSeparators(screenshotDir + "/" + fileName), "PNG");
        
        // 存储相对路径（相对于 reports/ 目录），并确保正斜杠
        QString relPath = QString("screenshots/%1").arg(fileName);
        
        // 将截图路径关联到所有引用此模型的结果
        for (auto& result : m_report.results) {
            if (result.properties.value("model") == path)
                result.properties["_screenshot_import"] = relPath;
            if (result.properties.value("resultModel") == path)
                result.properties["_screenshot_export"] = relPath;
        }
        idx++;
        prog.setValue(idx);
        QApplication::processEvents();
    }
    prog.close();
}

void MainWindow::onExportReport() {
    QString dir = QFileInfo(m_config.configPath()).absolutePath() + "/reports";
    QString htmlPath = dir + "/test_report.html";

    // 生成报告前先截取模型图片
    captureAllModelScreenshots(dir + "/screenshots");

    // 将更新后的截图路径写回 JSON 数据文件（持久化）
    if (!m_report.results.isEmpty()) {
        QString runName = m_config.profiles().value(m_config.activeProfile()).name;
        if (runName.isEmpty()) runName = m_report.startTime.toString("HH:mm:ss");
        QString err;
        ReportExporter::saveJson(m_report, dir, runName, &err);
    }

    if (m_report.results.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"),
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe5\x8f\xaf\xe7\x94\x9f\xe6\x88\x90\xe6\x8a\xa5\xe5\x91\x8a\xe7\x9a\x84\xe6\x95\xb0\xe6\x8d\xae"));
        return;
    }

    // 加载已有数据（按binary去重，每个配置只保留最新）+ 当前运行结果
    QString err;
    QVector<QPair<TestReport, QString>> entries = ReportExporter::loadAllData(dir, &err);

    QString runName = m_config.profiles().value(m_config.activeProfile()).name;
    if (runName.isEmpty()) runName = m_report.startTime.toString("HH:mm:ss");
    // 附带当前方案的筛选条件（未选择时默认第一个）
    int sceneIdx = m_scenarioCombo && m_scenarioCombo->currentIndex() > 0
                   ? m_scenarioCombo->currentIndex() - 1 : 0;
    auto& scenarios = m_config.currentProfile().scenarios;
    if (sceneIdx >= 0 && sceneIdx < scenarios.size())
        m_report.savedFilters = scenarios[sceneIdx].filterSets;
    // 移除同binary的旧条目，用当前结果替换
    entries.erase(std::remove_if(entries.begin(), entries.end(),
        [&](const QPair<TestReport, QString>& e) { return e.first.testBinary == m_report.testBinary; }),
        entries.end());
    entries.append({m_report, runName});

    // 一次性重建 HTML
    QFile::remove(htmlPath);
    if (!ReportExporter::rebuildHtml(entries, dir, &err)) {
        QMessageBox::warning(this, QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe5\xa4\xb1\xe8\xb4\xa5"), err);
        return;
    }

    auto r = QMessageBox::question(this, QString::fromUtf8("\xe6\x9f\xa5\xe7\x9c\x8b\xe7\xbb\x93\xe6\x9e\x9c"),
        QString::fromUtf8("\xe6\x8a\xa5\xe5\x91\x8a\xe5\xb7\xb2\xe7\x94\x9f\xe6\x88\x90:\n") + htmlPath + QString::fromUtf8("\n\n\xe6\x89\x93\xe5\xbc\x80\xe6\x96\x87\xe4\xbb\xb6\xe5\xa4\xb9?"),
        QMessageBox::Yes | QMessageBox::No);
    if (r == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
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
    dlg.setMinimumWidth(700);
    dlg.setMinimumHeight(550);
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
    QTreeWidget* sceneTree = nullptr;
    // 当前编辑的 profile 索引（不修改全局 activeProfile，避免影响其他逻辑）
    int currentEditIdx = m_config.activeProfile();
    auto fillMenu = [&]() {
        dlgProfMenu->clear();
        dlgProfBtn->setText(m_config.profiles()[currentEditIdx].name + QString::fromUtf8(" \xe2\x96\xbe"));
        for (int i = 0; i < m_config.profiles().size(); i++) {
            auto* act = dlgProfMenu->addAction(m_config.profiles()[i].name);
            act->setCheckable(true);
            act->setChecked(i == currentEditIdx);
            connect(act, &QAction::triggered, &dlg, [this, i, act, &loadProfile, dlgProfBtn, dlgProfMenu, &currentEditIdx]() {
                currentEditIdx = i;
                dlgProfBtn->setText(m_config.profiles()[i].name + QString::fromUtf8(" \xe2\x96\xbe"));
                // 更新菜单项的勾选状态
                for (auto* a : dlgProfMenu->actions())
                    a->setChecked(a == act);
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
        sceneTree->clear();
        for (const auto& sc : p.scenarios) {
            auto* item = new QTreeWidgetItem(sceneTree);
            item->setText(0, sc.name);
            item->setText(1, QString::number(sc.selectedTests.size()));
            item->setText(2, QString::fromUtf8("%1 \xe7\xbb\x84").arg(sc.filterSets.size()));
            item->setData(0, Qt::UserRole, sc.name);  // 保存方案名用于查找
            if (!sc.selectedTests.isEmpty()) {
                QStringList preview = sc.selectedTests.mid(0, 5);
                if (sc.selectedTests.size() > 5) preview.append("...");
                item->setToolTip(0, preview.join("\n"));
            }
        }
    };

    // 环境变量标签页
    auto* envTab = new QWidget;
    auto* envLay = new QVBoxLayout(envTab);
    auto* envHelp = new QLabel(QString::fromUtf8("\xe6\xaf\x8f\xe8\xa1\x8c\xe4\xb8\x80\xe4\xb8\xaa KEY=VALUE\xef\xbc\x8c\xe4\xbc\xa0\xe7\xbb\x99\xe6\xb5\x8b\xe8\xaf\x95 exe \xe4\xbd\x9c\xe4\xb8\xba\xe7\x8e\xaf\xe5\xa2\x83\xe5\x8f\x98\xe9\x87\x8f\xe3\x80\x82\xe5\x8f\xaf\xe7\x94\xa8\xe4\xba\x8e\xe4\xbc\xa0\xe9\x80\x92\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84\xef\xbc\x8c\xe7\x84\xb6\xe5\x90\x8e\xe5\x9c\xa8\xe6\xb5\x8b\xe8\xaf\x95\xe4\xb8\xad\xe7\x94\xa8 getenv() \xe8\xaf\xbb\xe5\x8f\x96\xe3\x80\x82"));
    envHelp->setStyleSheet("color:#64748b;font-size:12px");
    envHelp->setWordWrap(true);
    envLay->addWidget(envHelp);
    auto* envExplain = new QLabel("MODEL_DIR=D:\\data\\models          # \xe6\xb5\x8b\xe8\xaf\x95\xe7\x94\xa8 getenv(\"MODEL_DIR\") \xe8\x8e\xb7\xe5\x8f\x96\nTEST_DATA_DIR=D:\\data\\tests        # \xe6\xb5\x8b\xe8\xaf\x95\xe7\x94\xa8 getenv(\"TEST_DATA_DIR\") \xe8\x8e\xb7\xe5\x8f\x96");
    envExplain->setStyleSheet("color:#94a3b8;font-size:11px;padding:0 0 4px 0");
    envLay->addWidget(envExplain);
    edEnv = new QTextEdit;
    edEnv->setPlaceholderText("KEY=VALUE");
    edEnv->setMaximumHeight(200);
    envLay->addWidget(edEnv, 1);
    tabs->addTab(envTab, QString::fromUtf8("\xe7\x8e\xaf\xe5\xa2\x83\xe5\x8f\x98\xe9\x87\x8f"));
    tabs->addTab(catTab, QString::fromUtf8("\xe5\x88\x86\xe7\xb1\xbb"));

    // ── 方案标签页 ──
    auto* sceneTab = new QWidget;
    auto* sceneLay = new QVBoxLayout(sceneTab);
    sceneTree = new QTreeWidget(sceneTab);
    sceneTree->setHeaderLabels({QString::fromUtf8("\xe6\x96\xb9\xe6\xa1\x88\xe5\x90\x8d"), QString::fromUtf8("\xe6\x95\xb0\xe9\x87\x8f"), QString::fromUtf8("\xe7\xad\x9b\xe9\x80\x89\xe6\x9d\xa1\xe4\xbb\xb6")});
    sceneTree->setColumnWidth(0, 160); sceneTree->setColumnWidth(1, 60);
    sceneTree->setRootIsDecorated(false);
    sceneTree->setStyleSheet("QTreeWidget::item{padding:6px 10px;min-height:32px;font-size:13px}");
    sceneLay->addWidget(sceneTree, 1);
    auto* sceneBtns = new QHBoxLayout;
    auto* btnSceneSave = new QPushButton(QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe5\xbd\x93\xe5\x89\x8d\xe9\x80\x89\xe6\x8b\xa9"));
    auto* btnSceneRename = new QPushButton(QString::fromUtf8("\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d"));
    auto* btnSceneDel = new QPushButton(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"));
    btnSceneSave->setMinimumWidth(130); btnSceneRename->setMinimumWidth(70); btnSceneDel->setMinimumWidth(60);
    sceneBtns->addWidget(btnSceneSave);
    sceneBtns->addWidget(btnSceneRename);
    sceneBtns->addWidget(btnSceneDel);
    auto* btnSceneFilter = new QPushButton(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe7\xad\x9b\xe9\x80\x89\xe6\x9d\xa1\xe4\xbb\xb6"));
    btnSceneFilter->setMinimumWidth(130);
    sceneBtns->addWidget(btnSceneFilter);

    auto* btnSceneCopy = new QPushButton(QString::fromUtf8("\xe5\xa4\x8d\xe5\x88\xb6"));
    btnSceneCopy->setToolTip(QString::fromUtf8("\xe5\xa4\x8d\xe5\x88\xb6\xe9\x80\x89\xe4\xb8\xad\xe6\x96\xb9\xe6\xa1\x88 (Ctrl+C)"));
    btnSceneCopy->setMinimumWidth(70);
    auto* btnScenePaste = new QPushButton(QString::fromUtf8("\xe7\xb2\x98\xe8\xb4\xb4"));
    btnScenePaste->setToolTip(QString::fromUtf8("\xe7\xb2\x98\xe8\xb4\xb4\xe6\x96\xb9\xe6\xa1\x88 (Ctrl+V)"));
    btnScenePaste->setMinimumWidth(70);
    sceneBtns->addWidget(btnSceneCopy);
    sceneBtns->addWidget(btnScenePaste);

    sceneBtns->addStretch();
    sceneLay->addLayout(sceneBtns);

    // 保存：覆盖当前选中的方案
    connect(btnSceneSave, &QPushButton::clicked, &dlg, [&]() {
        auto selItems = sceneTree->selectedItems();
        if (selItems.isEmpty()) { QMessageBox::information(&dlg, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"), QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe4\xb8\x80\xe4\xb8\xaa\xe6\x96\xb9\xe6\xa1\x88")); return; }
        QStringList sel = m_testList->selectedTestNames();
        if (sel.isEmpty()) { QMessageBox::information(&dlg, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"), QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe5\x9c\xa8\xe5\xb7\xa6\xe4\xbe\xa7\xe9\x80\x89\xe6\x8b\xa9\xe7\x94\xa8\xe4\xbe\x8b")); return; }
        int idx = sceneTree->indexOfTopLevelItem(selItems[0]);
        m_config.currentProfile().scenarios[idx].selectedTests = sel;
        selItems[0]->setText(1, QString::number(sel.size()));
    });
    // 另存为：新建方案
    auto* btnSceneSaveAs = new QPushButton(QString::fromUtf8("\xe5\x8f\xa6\xe5\xad\x98\xe4\xb8\xba\xe6\x96\xb0\xe6\x96\xb9\xe6\xa1\x88"));
    btnSceneSaveAs->setMinimumWidth(140);
    sceneBtns->insertWidget(1, btnSceneSaveAs);
    connect(btnSceneSaveAs, &QPushButton::clicked, &dlg, [&]() {
        QStringList sel = m_testList->selectedTestNames();
        if (sel.isEmpty()) { QMessageBox::information(&dlg, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"), QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe7\x94\xa8\xe4\xbe\x8b")); return; }
        bool ok; QString name = QInputDialog::getText(&dlg, QString::fromUtf8("\xe6\x96\xb0\xe6\x96\xb9\xe6\xa1\x88"),
            QString::fromUtf8("\xe6\x96\xb9\xe6\xa1\x88\xe5\x90\x8d"), QLineEdit::Normal,
            QString::fromUtf8("\xe6\x96\xb9\xe6\xa1\x88 %1").arg(m_config.currentProfile().scenarios.size()+1), &ok);
        if (!ok || name.isEmpty()) return;
        TestScenario s; s.name = name; s.selectedTests = sel;
        m_config.addScenario(s);
        auto* item = new QTreeWidgetItem(sceneTree);
        item->setText(0, name); item->setText(1, QString::number(sel.size()));
        item->setText(2, QString::fromUtf8("0 \xe7\xbb\x84"));
    });
    // 从上次运行结果更新默认方案
    auto* btnSceneUpdate = new QPushButton(QString::fromUtf8("\xe6\x9b\xb4\xe6\x96\xb0"));
    bool hasResults = !m_report.results.isEmpty();
    btnSceneUpdate->setEnabled(hasResults);
    btnSceneUpdate->setStyleSheet(hasResults
        ? "QPushButton{background:#22c55e;color:#fff;border:none;border-radius:4px;padding:4px 12px;font-size:12px}"
        : "QPushButton{background:#e2e8f0;color:#94a3b8;border:none;border-radius:4px;padding:4px 12px;font-size:12px}");
    sceneBtns->addWidget(btnSceneUpdate);
    connect(btnSceneUpdate, &QPushButton::clicked, &dlg, [&]() {
        QStringList withProp, withoutProp;
        for (const auto& r : m_report.results) {
            QString full = r.testCase.fullName();
            if (r.properties.isEmpty()) withoutProp << full;
            else withProp << full;
        }
        for (auto& s : m_config.currentProfile().scenarios) {
            if (s.name == QString::fromUtf8("\xe6\x9c\x89\xe5\x8f\x82\xe6\x95\xb0\xe8\xbe\x93\xe5\x87\xba")) { s.selectedTests = withProp; }
            else if (s.name == QString::fromUtf8("\xe6\x97\xa0\xe5\x8f\x82\xe6\x95\xb0\xe8\xbe\x93\xe5\x87\xba")) { s.selectedTests = withoutProp; }
        }
        sceneTree->clear();
        for (const auto& sc : m_config.currentProfile().scenarios) {
            auto* item = new QTreeWidgetItem(sceneTree);
            item->setText(0, sc.name);
            item->setText(1, QString::number(sc.selectedTests.size()));
            item->setText(2, QString::fromUtf8("%1 \xe7\xbb\x84").arg(sc.filterSets.size()));
            if (!sc.selectedTests.isEmpty()) {
                QStringList preview = sc.selectedTests.mid(0, 5);
                if (sc.selectedTests.size() > 5) preview.append("...");
                item->setToolTip(0, preview.join("\n"));
            }
        }
    });

    connect(btnSceneRename, &QPushButton::clicked, &dlg, [&]() {
        auto sel = sceneTree->selectedItems();
        if (sel.isEmpty()) return;
        int idx = sceneTree->indexOfTopLevelItem(sel[0]);
        auto& s = m_config.currentProfile().scenarios[idx];
        bool ok; QString name = QInputDialog::getText(&dlg, QString::fromUtf8("\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d"),
            QString::fromUtf8("\xe6\x96\xb9\xe6\xa1\x88\xe5\x90\x8d"), QLineEdit::Normal, s.name, &ok);
        if (ok && !name.isEmpty()) { s.name = name; sel[0]->setText(0, name); }
    });
    connect(btnSceneDel, &QPushButton::clicked, &dlg, [&]() {
        auto sel = sceneTree->selectedItems();
        if (sel.isEmpty()) return;
        int idx = sceneTree->indexOfTopLevelItem(sel[0]);
        m_config.currentProfile().scenarios.remove(idx);
        delete sel[0];
    });
    // 编辑筛选条件
    connect(btnSceneFilter, &QPushButton::clicked, &dlg, [&]() {
        auto sel = sceneTree->selectedItems();
        if (sel.isEmpty()) { QMessageBox::information(&dlg, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"), QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe4\xb8\x80\xe4\xb8\xaa\xe6\x96\xb9\xe6\xa1\x88")); return; }
        int idx = sceneTree->indexOfTopLevelItem(sel[0]);
        auto& s = m_config.currentProfile().scenarios[idx];
        // 收集当前结果的属性键值
        QStringList propKeys;
        propKeys << "#" << QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81")
                 << QString::fromUtf8("\xe5\xa5\x97\xe4\xbb\xb6")
                 << QString::fromUtf8("\xe7\x94\xa8\xe4\xbe\x8b")
                 << QString::fromUtf8("\xe8\x80\x97\xe6\x97\xb6(ms)");
        QMap<QString, QStringList> propVals;
        // 收集标准列的值
        QStringList statusVals, suiteVals, caseVals;
        for (const auto& r : m_report.results) {
            QString st = r.status == "PASSED" ? QString::fromUtf8("\xe2\x9c\x85 \xe9\x80\x9a\xe8\xbf\x87")
                       : r.status == "SKIPPED" ? QString::fromUtf8("\xe2\x8f\xad \xe8\xb7\xb3\xe8\xbf\x87")
                       : r.status == "DISABLED" ? QString::fromUtf8("\xf0\x9f\x94\x92 \xe7\xa6\x81\xe7\x94\xa8")
                       : QString::fromUtf8("\xe2\x9d\x8c \xe5\xa4\xb1\xe8\xb4\xa5");
            if (!statusVals.contains(st)) statusVals << st;
            if (!suiteVals.contains(r.testCase.suiteName)) suiteVals << r.testCase.suiteName;
            if (!caseVals.contains(r.testCase.caseName)) caseVals << r.testCase.caseName;
            for (auto it = r.properties.begin(); it != r.properties.end(); ++it) {
                if (!propKeys.contains(it.key())) propKeys << it.key();
                if (!propVals[it.key()].contains(it.value()))
                    propVals[it.key()].append(it.value());
            }
        }
        propVals["#"] = QStringList();
        propVals[QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81")] = statusVals;
        propVals[QString::fromUtf8("\xe5\xa5\x97\xe4\xbb\xb6")] = suiteVals;
        propVals[QString::fromUtf8("\xe7\x94\xa8\xe4\xbe\x8b")] = caseVals;
        propVals[QString::fromUtf8("\xe8\x80\x97\xe6\x97\xb6(ms)")] = QStringList();
        for (auto& v : propVals) v.sort();
        // 对属性部分排序（保留前5个标准列顺序）
        if (propKeys.size() > 5) {
            auto mid = propKeys.mid(5);
            mid.sort();
            propKeys = propKeys.first(5) + mid;
        }
        FilterEditDialog fdlg(s.filterSets, propKeys, propVals, &dlg);
        if (fdlg.exec() == QDialog::Accepted) {
            s.filterSets = fdlg.result();
            sel[0]->setText(2, QString::fromUtf8("%1 \xe7\xbb\x84").arg(s.filterSets.size()));
        }
    });

    // 复制方案
    connect(btnSceneCopy, &QPushButton::clicked, &dlg, [&]() {
        auto sel = sceneTree->selectedItems();
        if (sel.isEmpty()) { QMessageBox::information(&dlg, QString::fromUtf8("\xe6\x8f\x90\xe7\xa4\xba"), QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe4\xb8\x80\xe4\xb8\xaa\xe6\x96\xb9\xe6\xa1\x88")); return; }
        int idx = sceneTree->indexOfTopLevelItem(sel[0]);
        const auto& s = m_config.currentProfile().scenarios[idx];
        QJsonObject obj;
        obj["name"] = s.name;
        QJsonArray tests;
        for (const auto& t : s.selectedTests) tests.append(t);
        obj["selectedTests"] = tests;
        obj["singleTest"] = s.singleTest;
        QJsonArray filters;
        for (const auto& fs : s.filterSets) {
            QJsonObject fo;
            fo["name"] = fs.name;
            fo["mode"] = fs.mode;
            QJsonArray conds;
            for (const auto& c : fs.conditions) {
                QJsonObject co;
                co["key"] = c.key; co["op"] = c.op; co["value"] = c.value;
                conds.append(co);
            }
            fo["conditions"] = conds;
            filters.append(fo);
        }
        obj["filterSets"] = filters;
        QApplication::clipboard()->setText(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    });

    // 粘贴方案
    connect(btnScenePaste, &QPushButton::clicked, &dlg, [&]() {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            QMessageBox::information(&dlg, QString::fromUtf8("\xe7\xb2\x98\xe8\xb4\xb4\xe5\xa4\xb1\xe8\xb4\xa5"),
                QString::fromUtf8("\xe5\x89\xaa\xe8\xb4\xb4\xe6\x9d\xbf\xe5\x86\x85\xe5\xae\xb9\xe4\xb8\x8d\xe6\x98\xaf\xe6\x9c\x89\xe6\x95\x88\xe7\x9a\x84\xe6\x96\xb9\xe6\xa1\x88\xe6\x95\xb0\xe6\x8d\xae\xe3\x80\x82"));
            return;
        }
        QJsonObject obj = doc.object();
        QString name = obj["name"].toString();
        if (name.isEmpty()) name = QString::fromUtf8("\xe7\xb2\x98\xe8\xb4\xb4\xe6\x96\xb9\xe6\xa1\x88");
        // 重名自动加后缀
        auto& scs = m_config.currentProfile().scenarios;
        int copyIdx = 1;
        QString baseName = name;
        auto nameExists = [&](const QString& n) {
            for (const auto& s : scs) if (s.name == n) return true;
            return false;
        };
        while (nameExists(name))
            name = baseName + QString::fromUtf8(" (%1)").arg(++copyIdx);
        TestScenario s;
        s.name = name;
        s.singleTest = obj["singleTest"].toBool(false);
        for (const auto& v : obj["selectedTests"].toArray())
            if (v.isString()) s.selectedTests << v.toString();
        for (const auto& fv : obj["filterSets"].toArray()) {
            QJsonObject fo = fv.toObject();
            FilterSet fs;
            fs.name = fo["name"].toString();
            fs.mode = fo["mode"].toString("and");
            for (const auto& cv : fo["conditions"].toArray()) {
                QJsonObject co = cv.toObject();
                FilterCondition fc;
                fc.key = co["key"].toString();
                fc.op = co["op"].toString();
                fc.value = co["value"].toString();
                fs.conditions.append(fc);
            }
            s.filterSets.append(fs);
        }
        m_config.addScenario(s);
        auto* item = new QTreeWidgetItem(sceneTree);
        item->setText(0, s.name);
        item->setText(1, QString::number(s.selectedTests.size()));
        item->setText(2, QString::fromUtf8("%1 \xe7\xbb\x84").arg(s.filterSets.size()));
    });

    // Ctrl+C/V 快捷键
    auto* shortcutCopy = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), sceneTree, nullptr, nullptr, Qt::WidgetShortcut);
    QObject::connect(shortcutCopy, &QShortcut::activated, &dlg, [btnSceneCopy]() { btnSceneCopy->click(); });
    auto* shortcutPaste = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_V), sceneTree, nullptr, nullptr, Qt::WidgetShortcut);
    QObject::connect(shortcutPaste, &QShortcut::activated, &dlg, [btnScenePaste]() { btnScenePaste->click(); });

    tabs->addTab(sceneTab, QString::fromUtf8("\xe6\x96\xb9\xe6\xa1\x88"));

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
            refreshScenarioCombo();
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
    // 保存到历史（用于后续可能的对比）
    m_allRuns.append(m_report);
    QString profileName = m_config.profiles().value(m_config.activeProfile()).name;
    m_runNames.append(profileName.isEmpty() ? QString::number(m_report.results.size()) + " tests" : profileName);
    m_progress->finishRun();
    m_centerResultView->showResults(m_report.results);

    int p = m_report.passed(), f = m_report.failed(), s = m_report.skipped(), d = m_report.disabled();
    LOG("RUN", QString("Done: passed=%1 failed=%2 skipped=%3 disabled=%4 total=%5")
        .arg(p).arg(f).arg(s).arg(d).arg(m_report.total()));
    QString msg = QString("Done! Passed=%1 Failed=%2").arg(p).arg(f);
    if (s > 0) msg += QString(" Skipped=%1").arg(s);
    if (d > 0) msg += QString(" Disabled=%1").arg(d);
    msg += QString(" Total=%1 ms").arg(m_report.totalDurationMs(), 0, 'f', 0);
    statusBar()->showMessage(msg, 10000);
    updateButtonStates();
    // 仅首次自动生成默认方案（如果还不存在）
    auto& prof = m_config.currentProfile();
    bool foundWith = false, foundWithout = false;
    for (const auto& s : prof.scenarios) {
        if (s.name == QString::fromUtf8("\xe6\x9c\x89\xe5\x8f\x82\xe6\x95\xb0\xe8\xbe\x93\xe5\x87\xba")) foundWith = true;
        else if (s.name == QString::fromUtf8("\xe6\x97\xa0\xe5\x8f\x82\xe6\x95\xb0\xe8\xbe\x93\xe5\x87\xba")) foundWithout = true;
    }
    // 自动导出：仅保存 JSON 数据文件，不操作 HTML（点击"查看结果"时才统一重建）
    // 附带当前方案的筛选条件（未选择时默认第一个）
    int sceneIdx = m_scenarioCombo && m_scenarioCombo->currentIndex() > 0
                   ? m_scenarioCombo->currentIndex() - 1 : 0;
    if (sceneIdx >= 0 && sceneIdx < prof.scenarios.size())
        m_report.savedFilters = prof.scenarios[sceneIdx].filterSets;
    QString reportDir = QFileInfo(m_config.configPath()).absolutePath() + "/reports";
    if (!m_report.results.isEmpty()) {
        QString autoName = m_config.profiles().value(m_config.activeProfile()).name;
        if (autoName.isEmpty()) autoName = QDateTime::currentDateTime().toString("HH:mm:ss");
        QString err;
        if (ReportExporter::saveJson(m_report, reportDir, autoName, &err)) {
            statusBar()->showMessage(
                QString::fromUtf8("\xe6\x95\xb0\xe6\x8d\xae\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xef\xbc\x8c\xe7\x82\xb9\xe5\x87\xbb\xe2\x80\x9c\xe6\x9f\xa5\xe7\x9c\x8b\xe7\xbb\x93\xe6\x9e\x9c\xe2\x80\x9d\xe7\x94\x9f\xe6\x88\x90\xe6\x8a\xa5\xe5\x91\x8a"),
                5000);
        } else {
            LOG("EXPORT", "saveJson failed: " + err);
        }
        // 不再清空 m_allRuns / m_runNames，保留运行历史
    }

    if (!foundWith || !foundWithout) {
        QStringList withProp, withoutProp;
        for (const auto& r : m_report.results) {
            QString full = r.testCase.fullName();
            if (r.properties.isEmpty()) withoutProp << full;
            else withProp << full;
        }
        if (!foundWith && !withProp.isEmpty()) { TestScenario s; s.name = QString::fromUtf8("\xe6\x9c\x89\xe5\x8f\x82\xe6\x95\xb0\xe8\xbe\x93\xe5\x87\xba"); s.selectedTests = withProp; prof.scenarios.push_back(s); }
        if (!foundWithout && !withoutProp.isEmpty()) { TestScenario s; s.name = QString::fromUtf8("\xe6\x97\xa0\xe5\x8f\x82\xe6\x95\xb0\xe8\xbe\x93\xe5\x87\xba"); s.selectedTests = withoutProp; prof.scenarios.push_back(s); }
        m_config.save();
        refreshScenarioCombo();
    }
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

void MainWindow::refreshScenarioCombo() {
    if (!m_scenarioCombo) return;
    // 保存当前选中的方案名（直接从combo取文本，避免索引漂移）
    QString prevName;
    int prevIdx = m_scenarioCombo->currentIndex();
    if (prevIdx > 0)
        prevName = m_scenarioCombo->currentText();
    m_scenarioCombo->blockSignals(true);
    m_scenarioCombo->clear();
    m_scenarioCombo->addItem(QString::fromUtf8("\xe2\x80\x94 \xe6\x96\xb9\xe6\xa1\x88 \xe2\x80\x94"));
    int restoreIdx = 0;
    for (int i = 0; i < m_config.currentProfile().scenarios.size(); i++) {
        const auto& s = m_config.currentProfile().scenarios[i];
        m_scenarioCombo->addItem(s.name);
        if (!prevName.isEmpty() && s.name == prevName)
            restoreIdx = i + 1;  // +1 因为第0项是占位符
    }
    m_scenarioCombo->setCurrentIndex(restoreIdx);
    m_scenarioCombo->blockSignals(false);
    // 恢复逐个运行复选框状态（从第1个方案）
    if (m_chkSingleTest) {
        auto& scs = m_config.currentProfile().scenarios;
        m_chkSingleTest->setChecked(!scs.isEmpty() && scs[0].singleTest);
    }
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
            refreshScenarioCombo();
            m_centerResultView->clear();
            m_report = {};
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
