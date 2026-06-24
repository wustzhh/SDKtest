#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QLabel>

#include "core/ConfigManager.h"
#include "core/TestLoader.h"
#include "core/TestRunner.h"
#include "core/ResultParser.h"
#include "core/ReportExporter.h"

#include "ui/TestListPanel.h"
#include "ui/TestProgressPanel.h"
#include "ui/ModelRenderView.h"
#include "ui/ModelInfoPanel.h"
#include "ui/Model3DViewer.h"
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onLoadTests();
    void onRunSelected();
    void onCancelRun();
    void onExportReport();
    void onEditConfig();
    void onAbout();
    void onTestFinished(const TestRunResult& result);
    void onProgressUpdated(int done, int total);
    void onAllFinished();
    void onRawOutput(const QString& line);
    void onSelectionChanged(int count);

public:
    void openModelFile(const QString& path) {
        if (m_model3D) m_model3D->loadFile(path);
    }

private:
    void setupUi();
    void setupMenu();
    void setupConnections();
    void updateButtonStates();

    ConfigManager     m_config;
    TestLoader        m_loader;
    TestRunner*       m_runner;
    TestReport        m_report;

    QSplitter*        m_mainSplitter    = nullptr;
    TestListPanel*    m_testList        = nullptr;
    TestProgressPanel* m_progress       = nullptr;
    ModelRenderView*  m_centerResultView = nullptr;  // 中栏结果树
    ModelInfoPanel*   m_modelInfo       = nullptr;
    Model3DViewer*    m_model3D         = nullptr;

    QAction* m_actLoad   = nullptr;
    QAction* m_actRun    = nullptr;
    QAction* m_actCancel = nullptr;
    QAction* m_actExport = nullptr;
    QAction* m_actConfig = nullptr;
};
