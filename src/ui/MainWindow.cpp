#include "MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
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
        LOG("CFG", "Config loaded: " + m_config.configPath());
        LOG("CFG", "binary: " + m_config.testBinary());
        LOG("CFG", "workdir: " + m_config.workingDir());
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
    centerSplitter->addWidget(m_progress);
    centerSplitter->addWidget(m_centerResultView);
    centerSplitter->setStretchFactor(0, 0);
    centerSplitter->setStretchFactor(1, 1);

    // ═══ 右：ModelInfo(上) + Model3DViewer(下) ═══
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
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 2);
    m_mainSplitter->setStretchFactor(2, 3);
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
        QMessageBox::information(this, "Info", "Set test binary path in Config first.");
        onEditConfig();
        return;
    }

    LOG("LOAD", "Binary: " + binary);
    LOG("LOAD", "WorkDir: " + m_config.workingDir());

    statusBar()->showMessage("Loading...");
    QApplication::processEvents();

    if (m_loader.load(binary, m_config.extraArgs(), m_config.workingDir())) {
        int n = m_loader.testCases().size();
        LOG("LOAD", "OK, found: " + QString::number(n) + " tests");
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
        QMessageBox::information(this, "Info", "Select tests first.");
        return;
    }

    LOG("RUN", "Selected", QString::number(sel.size()) + " tests");

    m_report = {};
    m_report.startTime = QDateTime::currentDateTime();
    m_report.testBinary = m_config.testBinary();
    m_report.filterPattern = "custom";

    m_centerResultView->clear();
    m_progress->startRun(sel.size());
    m_runner->run(m_config.testBinary(), sel, m_config.extraArgs(), m_config.workingDir());
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
        QMessageBox::information(this, "Export", "No results to export.");
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
        auto r = QMessageBox::question(this, "Exported",
            "Saved:\n  " + base + ".xlsx\n  " + base + ".txt\n\nOpen folder?");
        if (r == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl::fromLocalFile(outDir));
    } else {
        QMessageBox::critical(this, "Export Failed", err);
    }
}

void MainWindow::onEditConfig() {
    QString start = m_config.testBinary().isEmpty()
        ? QDir::currentPath()
        : QFileInfo(m_config.testBinary()).absolutePath();
    QString bin = QFileDialog::getOpenFileName(this, "Select test exe", start,
                                                "Executable (*.exe);;All (*)");
    if (bin.isEmpty()) return;
    m_config.setTestBinary(bin);
    m_config.save();
    LOG("CFG", "Binary set", bin);
    statusBar()->showMessage("Config saved: " + bin, 5000);
    updateButtonStates();
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
