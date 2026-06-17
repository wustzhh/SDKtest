#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QMap>

#include "models/TestResult.h"

// ────────────────────────────────────────────────────────────
//  结果模型渲染视图
//  将结构化测试结果渲染为可交互的树形模型
// ────────────────────────────────────────────────────────────
class ModelRenderView : public QWidget {
    Q_OBJECT
public:
    explicit ModelRenderView(QWidget* parent = nullptr);

    // 显示所有运行结果
    void showResults(const QVector<TestRunResult>& results);

    // 清空
    void clear();

signals:
    void resultSelected(const TestRunResult& result);
    void collapseRequested();

private slots:
    void onSearchChanged(const QString& text);
    void onFilterChanged(int index);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onExpandAll();
    void onCollapseAll();

private:
    void buildResultTree(const QVector<TestRunResult>& results);
    void addNodeToTree(QTreeWidgetItem* parent, const ResultNode& node);
    void highlightMatches(QTreeWidgetItem* item, const QString& text);
    void updateDetailPanel(const TestRunResult* result);

    // UI 组件
    QVBoxLayout*    m_layout;
    QStackedWidget* m_stack;
    QLabel*         m_placeholder;

    // page 1 content
    QWidget*        m_content;
    QPushButton*    m_btnCollapsePanel;
    QLineEdit*      m_searchEdit;
    QComboBox*      m_filterCombo;
    QPushButton*    m_btnExpand;
    QPushButton*    m_btnCollapse;
    QLabel*         m_lblStats;
    QSplitter*      m_splitter;
    QTreeWidget*    m_tree;
    QTextBrowser*   m_detailPanel;

    // 数据
    QVector<TestRunResult> m_results;
    QMap<QString, TestRunResult*> m_resultMap;  // fullName → result
};
