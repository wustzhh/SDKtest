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
    btnOpen->setMinimumWidth(100);
    rightL->addWidget(btnOpen);
    connect(btnOpen, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "选择模型文件",
            QString(), "模型文件 (*.step *.stp *.iges *.igs *.brep);;所有文件 (*)");
        if (!path.isEmpty()) {
            LOG("MODEL", "Open: " + path);
            m_modelInfo->showModelInfo(nullptr);
            m_model3D->loadFile(path);
        }
    });

    m_modelInfo = new ModelInfoPanel;
    m_modelInfo->setMaximumHeight(180);
    rightL->addWidget(m_modelInfo);

    m_model3D = new Model3DViewer;
    rightL->addWidget(m_model3D, 1);

    // ── 组装 ──
    m_mainSplitter->addWidget(m_leftPanel);
    m_mainSplitter->addWidget(centerSplitter);
    m_mainSplitter->addWidget(m_rightPanel);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);
    m_mainSplitter->setStretchFactor(2, 2);
    mainLayout->addWidget(m_mainSplitter, 1);

    // ── 底栏 ──
    auto* bar = new QWidget;
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(10, 5, 10, 5);
    auto* bLeft = new QPushButton(QString::fromUtf8("\xe2\x97\x80"));
    bLeft->setFixedSize(60, 30);
    bLeft->setStyleSheet("QPushButton{padding:0;min-width:60;border-radius:8px}");
    bLeft->setToolTip(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f \xe5\xb7\xa6\xe4\xbe\xa7\xe9\x9d\xa2\xe6\x9d\xbf"));
    connect(bLeft, &QPushButton::clicked, this, [=]() {
        bool vis = !m_leftPanel->isVisible();
        m_leftPanel->setVisible(vis);
        bLeft->setText(vis ? QString::fromUtf8("\xe2\x97\x80") : QString::fromUtf8("\xe2\x96\xb6"));
    });
    bl->addWidget(bLeft);
    auto* bCfg = new QPushButton("\u2699 配置");
    auto* bLd  = new QPushButton("\U0001F4C2 加载");
    auto* bExp = new QPushButton("\U0001F4CA 导出");
    auto* bRun = new QPushButton("\u25B6 运行");
    bRun->setStyleSheet(
        "QPushButton { background:#4CAF50; color:white; border:none; "
        "border-radius:4px; padding:6px 20px; font-size:13px; font-weight:bold; }"
        "QPushButton:hover { background:#388E3C; }"
        "QPushButton:disabled { background:#ccc; }");
    bl->addWidget(bCfg);
    bl->addWidget(bLd);
    bl->addStretch();
    bl->addWidget(bExp);
    bl->addWidget(bRun);
    auto* bRight = new QPushButton(QString::fromUtf8("\xe2\x96\xb6"));
    bRight->setFixedSize(60, 30);
    bRight->setStyleSheet("QPushButton{padding:0;min-width:60;border-radius:8px}");
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
    connect(m_centerResultView, &ModelRenderView::resultSelected, this, [this](const TestRunResult& r) {
        m_modelInfo->showModelInfo(&r);
        if (r.properties.contains("model"))
            m_model3D->loadFile(r.properties["model"]);
    });

    connect(m_modelInfo, &ModelInfoPanel::openFileRequested, this, [this](const QString& path) {
        m_model3D->loadFile(path);
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
