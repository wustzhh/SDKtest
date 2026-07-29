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
#include <QPropertyAnimation>
#include <QEvent>

// ── ToggleSwitch（头文件中确保MOC正确生成） ──
class ToggleSwitch : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double animPos READ animPos WRITE setAnimPos)
public:
    ToggleSwitch(QWidget* p=nullptr):QWidget(p),m_checked(false),m_animPos(0){
        setFixedSize(44,24);setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_anim = new QPropertyAnimation(this,"animPos");
        m_anim->setDuration(180);
    }
    bool isChecked() const { return m_checked; }
    void setChecked(bool c) {
        if(c==m_checked) return;
        m_checked=c;
        m_anim->stop();
        m_anim->setStartValue(m_animPos);
        m_anim->setEndValue(c?1.0:0.0);
        m_anim->start();
        emit toggled(m_checked);
    }
    double animPos() const { return m_animPos; }
    void setAnimPos(double v) { m_animPos=v; update(); }
signals:
    void toggled(bool);
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        double t=m_animPos;
        p.setBrush(QColor(t>0.5?QString("#6366f1"):QString("#d1d5db")));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(),12,12);
        p.setBrush(Qt::white);
        p.drawEllipse((int)(2+t*(width()-22)),2,20,20);
    }
    void mousePressEvent(QMouseEvent*) override { setChecked(!m_checked); }
private:
    bool m_checked;
    double m_animPos;
    QPropertyAnimation* m_anim;
};

#include "models/TestResult.h"
#include "core/ConfigManager.h"

struct FilterRule {
    QString keyword;
    bool include = true; // true=正选, false=反选
};

class FilterDialog : public QDialog {
    Q_OBJECT
public:
    FilterDialog(const QVector<TestCase>& allCases, QWidget* parent = nullptr);
    QVector<FilterRule> rules() const { return m_rules; }
    bool enabled() const;
    void setSrcTree(QTreeWidget* t) { m_srcTree = t; updatePreview(); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0,0,0,128));
    }
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (obj == m_input && ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                addRule();
                return true;  // 消费事件，阻止传播到对话框
            }
        }
        return QDialog::eventFilter(obj, ev);
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
    QTreeWidget* m_srcTree = nullptr;
    QVector<TestCase> m_allCases;
    QVector<FilterRule> m_rules;
};

class TestTreePanel : public QWidget {
    Q_OBJECT
public:
    explicit TestTreePanel(QWidget* parent = nullptr);

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
    QVector<TestCase> m_allCases;
    QVector<FilterRule> m_advFilters;  // 当前方案的高级筛选规则

    bool            m_updating = false;
};
