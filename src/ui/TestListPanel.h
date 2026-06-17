#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QVector>
#include <QMenu>
#include <QTimer>

#include "models/TestResult.h"
#include "core/ConfigManager.h"

class TestListPanel : public QWidget {
    Q_OBJECT
public:
    explicit TestListPanel(QWidget* parent = nullptr);

    void loadTests(const QVector<TestCase>& cases,
                   const QVector<TestCategory>& categories = {});

    QVector<TestCase> selectedTests() const;
    void selectAll(bool select);

signals:
    void selectionChanged(int selectedCount);
    void collapseRequested();

private slots:
    void onFilterChanged(const QString& text);
    void onSelectAllClicked();
    void onDeselectAllClicked();
    void onExpandAllClicked();
    void onCollapseAllClicked();
    void onTreeContextMenu(const QPoint& pos);

private:
    void buildTree(const QVector<TestCase>& cases,
                   const QVector<TestCategory>& categories);
    void toggleItem(QTreeWidgetItem* item);
    void applyToDescendants(QTreeWidgetItem* parent, bool checked);
    void updateParentState(QTreeWidgetItem* item);
    void updateItemText(QTreeWidgetItem* item);
    void collectChecked(QTreeWidgetItem* item, QVector<TestCase>& out) const;
    bool applyFilter(QTreeWidgetItem* item, const QString& text);
    void showEmptyPlaceholder();
    int  countVisibleLeaf() const;
    int  countVisibleLeafRec(QTreeWidgetItem* item) const;
    void updateStats();

    QLineEdit*      m_searchEdit;
    QWidget*        m_toolbar;
    QPushButton*    m_btnCollapsePanel;
    QPushButton*    m_btnExpand;
    QPushButton*    m_btnCollapse;
    QPushButton*    m_btnSelectAll;
    QPushButton*    m_btnDeselectAll;
    QLabel*         m_lblStats;
    QTreeWidget*    m_tree;
    QMenu*          m_contextMenu;

    bool            m_updating = false;
};
