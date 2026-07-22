#pragma once

#include <QDialog>
#include <QListWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QVector>
#include "models/TestResult.h"

// ────────────────────────────────────────────────────────────
//  筛选条件编辑对话框
//  左列：筛选组名列表，右列：选中组的条件编辑
// ────────────────────────────────────────────────────────────
class FilterEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit FilterEditDialog(const QVector<FilterSet>& filterSets,
                               const QStringList& propertyKeys = {},
                               const QMap<QString, QStringList>& propertyValues = {},
                               QWidget* parent = nullptr);

    QVector<FilterSet> result() const { return m_filterSets; }

private slots:
    void onGroupSelected(int row);
    void onNewGroup();
    void onDeleteGroup();
    void onRenameGroup();
    void onAddCondition();
    void onRemoveCondition(int row);
    void onAccept();

private:
    void refreshGroupList();
    void refreshConditionTable();
    void flushCurrentGroup();

    QVector<FilterSet> m_filterSets;
    int m_currentGroup = -1;

    QListWidget*    m_groupList;
    QPushButton*    m_btnNew;
    QPushButton*    m_btnDelete;
    QPushButton*    m_btnRename;

    QTableWidget*   m_condTable;
    QComboBox*      m_modeCombo;
    QPushButton*    m_btnAddCond;
    QPushButton*    m_btnRemoveCond;

    QStringList m_propertyKeys;
    QMap<QString, QStringList> m_propertyValues;
};
