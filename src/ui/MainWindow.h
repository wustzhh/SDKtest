#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>

#include "core/ConfigManager.h"
#include "core/TestLoader.h"
#include "core/TestRunner.h"
#include "core/ResultParser.h"
#include "core/ReportExporter.h"

#include "ui/CaseTreePanel.h"
#include "ui/TestProgressPanel.h"
#include "ui/CaseListView.h"

#include "ui/Model3DViewer.h"
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void closeEvent(QCloseEvent* e) override;
    void showEvent(QShowEvent* e) override;

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
    void refreshProfileCombo();
    void refreshScenarioCombo(bool applySelection = true);  // false=只刷新下拉项不应用勾选
    void refreshRestoreCombo();
    void onRestoreFromFile(int index);
    void saveLayout();
    // 模型截图
    void captureAllModelScreenshots(const QString& screenshotDir);

    ConfigManager     m_config;
    TestLoader        m_loader;
    TestRunner*       m_runner;
    TestReport        m_report;
    QVector<TestReport> m_allRuns;
    QStringList       m_runNames;

    QSplitter*        m_mainSplitter    = nullptr;
    QSplitter*        m_centerSplitter  = nullptr;
    CaseTreePanel*    m_testList        = nullptr;
    TestProgressPanel* m_progress       = nullptr;
    CaseListView*  m_centerResultView = nullptr;  // 用例列表（中间结果）
    Model3DViewer*    m_model3D         = nullptr;
    QWidget*          m_leftPanel       = nullptr;
    QWidget*          m_rightPanel      = nullptr;

    QAction* m_actLoad   = nullptr;
    QAction* m_actRun    = nullptr;
    QAction* m_actCancel = nullptr;
    QAction* m_actExport = nullptr;
    QAction* m_actConfig = nullptr;
    QPushButton* m_profileBtn = nullptr;
    QMenu* m_profileMenu = nullptr;
    QComboBox* m_scenarioCombo = nullptr;
    QCheckBox* m_chkSingleTest = nullptr;
    QComboBox* m_restoreCombo = nullptr;
    QList<QAction*> m_lwActions;
    int m_restoreLW = 0, m_restoreRW = 0, m_restoreVP = 0, m_restoreVP2 = 0;
    QStringList m_suiteNames;
    QSet<QString> m_seenResults;
};
