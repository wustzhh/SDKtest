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
#include <QDialog>
#include <QCheckBox>
#include <QPainter>
#include <QListWidget>
#include <QScrollArea>

// ── ToggleSwitch（头文件中确保MOC正确生成） ──
class ToggleSwitch : public QWidget {
    Q_OBJECT
public:
    ToggleSwitch(QWidget* p=nullptr):QWidget(p),m_checked(false),m_pos(0){
        setFixedSize(40,22);setCursor(Qt::PointingHandCursor);
    }
    bool isChecked() const { return m_checked; }
    void setChecked(bool c) {
        if(c==m_checked) return;
        m_checked=c;
        m_pos=c?1.0:0.0;
        update();
        emit toggled(m_checked);
    }
signals:
    void toggled(bool);
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(m_checked?QColor("#6366f1"):QColor("#d1d5db"));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(),11,11);
        p.setBrush(Qt::white);
        p.drawEllipse((int)(1+m_pos*(width()-20)),1,20,20);
    }
    void mousePressEvent(QMouseEvent*) override { setChecked(!m_checked); }
private:
    bool m_checked;
    double m_pos;
};

#include "models/TestResult.h"
#include "core/ConfigManager.h"

struct FilterRule {
    QString keyword;
    bool include = true; // true=正选, false=反选
};

class AdvancedFilterDialog : public QDialog {
    Q_OBJECT
public:
    AdvancedFilterDialog(const QVector<TestCase>& allCases, QWidget* parent = nullptr);
    QVector<FilterRule> rules() const { return m_rules; }
    bool enabled() const;
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0,0,0,128));
    }
signals:
    void filterChanged();
private:
    void addRule();
    void removeRule(int idx);
    void toggleRule(int idx);
    void rebuildRuleWidgets();
    void updatePreview();
    QVector<TestCase> applyFilter() const;

    QLineEdit* m_input;
    bool m_enabled = false;
    ToggleSwitch* m_enableSwitch;
    QWidget* m_ruleWidget;
    class FlowLayout* m_ruleLayout;
    QTreeWidget* m_previewTree;
    QVector<TestCase> m_allCases;
    QVector<FilterRule> m_rules;
};

class TestListPanel : public QWidget {
    Q_OBJECT
public:
    explicit TestListPanel(QWidget* parent = nullptr);

    void loadTests(const QVector<TestCase>& cases,
                   const QVector<TestCategory>& categories = {});

    QVector<TestCase> selectedTests() const;
    QStringList selectedTestNames() const;
    void setSelectedTestNames(const QStringList& names);
    void selectAll(bool select);

signals:
    void selectionChanged(int selectedCount);
    void collapseRequested();

private slots:
    void onFilterChanged(const QString& text);
    void onSelectAllClicked();
    void onDeselectAllClicked();
    void onReverseFilterClicked();
    void onAdvancedFilter();
    void collapseToLevel(int level);
    void expandToLevel(int level);
    void onExpandAllClicked();
    void onCollapseAllClicked();
    void onTreeContextMenu(const QPoint& pos);

private:
    void buildTree(const QVector<TestCase>& cases,
                   const QVector<TestCategory>& categories);
    void updatePathLabel(QTreeWidgetItem* item);
    void buildGroupTree(QTreeWidgetItem* parent,
                        const QVector<TestCase>& cases,
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
    QTreeWidgetItem* m_lastHighlighted = nullptr;

    QLineEdit*      m_searchEdit;
    QLabel*         m_pathLabel = nullptr;
    QWidget*        m_toolbar;
    QPushButton*    m_btnCollapsePanel;
    QPushButton*    m_btnReverseFilter;
    QPushButton*    m_btnExpand;
    QPushButton*    m_btnCollapse;
    QPushButton*    m_btnSelectAll;
    QPushButton*    m_btnDeselectAll;
    QLabel*         m_lblStats;
    QTreeWidget*    m_tree;
    QMenu*          m_contextMenu;
    QVector<TestCase> m_allCases;  // 所有用例，用于高级筛选

    bool            m_updating = false;
};
