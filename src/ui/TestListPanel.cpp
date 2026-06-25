#include "TestListPanel.h"

#include <QHeaderView>
#include <QVBoxLayout>
#include <QShortcut>
#include <QHBoxLayout>
#include <QApplication>
#include <QStyle>

static const QString MARK_NO   = QString::fromUtf8("\xe2\x98\x90");
static const QString MARK_YES  = QString::fromUtf8("\xe2\x98\x91");
static const QString MARK_HALF = QString::fromUtf8("\xe2\x98\x92");

enum { Role_Type = Qt::UserRole + 1, Role_SuiteName, Role_CaseName };

TestListPanel::TestListPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    // Header: search
    auto* headerRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索用例...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &TestListPanel::onFilterChanged);
    headerRow->addWidget(m_searchEdit, 1);
    layout->addLayout(headerRow);

    // Toolbar
    m_toolbar = new QWidget(this);
    auto* tb = new QHBoxLayout(m_toolbar);
    tb->setContentsMargins(0, 0, 0, 0);
    tb->setSpacing(1);
    m_btnSelectAll   = new QPushButton("全选", this);
    m_btnDeselectAll = new QPushButton("全消", this);
    m_btnReverseFilter = new QPushButton(QString::fromUtf8("\xE2\x87\x84 \xE5\x8F\x8D\xE9\x80\x89"), this);
    m_btnReverseFilter->setToolTip("反转当前选区");
    m_lblStats = new QLabel("0", this);
    tb->addWidget(m_btnSelectAll);
    tb->addWidget(m_btnDeselectAll);
    tb->addWidget(m_btnReverseFilter);
    tb->addStretch();
    tb->addWidget(m_lblStats);
    layout->addWidget(m_toolbar);
    connect(m_btnSelectAll,   &QPushButton::clicked, this, &TestListPanel::onSelectAllClicked);
    connect(m_btnDeselectAll, &QPushButton::clicked, this, &TestListPanel::onDeselectAllClicked);
    connect(m_btnReverseFilter, &QPushButton::clicked, this, &TestListPanel::onReverseFilterClicked);

    // Tree
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(1);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(16);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setStyleSheet(
        "QTreeWidget { font-size:12px; border:1px solid #ddd; background:white; }");
    m_tree->setMinimumWidth(0);
    m_tree->setExpandsOnDoubleClick(false);
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item || m_updating) return;
        toggleItem(item);
    });
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &TestListPanel::onTreeContextMenu);
    layout->addWidget(m_tree, 1);

    m_contextMenu = new QMenu(this);

    // Alt+数字 = 折叠到对应层级, Alt+Shift+数字 = 展开到对应层级
    for (int d = 1; d <= 9; d++) {
        auto* scCollapse = new QShortcut(QKeySequence(Qt::ALT | (Qt::Key_0 + d)), this);
        connect(scCollapse, &QShortcut::activated, this, [this, d]() { collapseToLevel(d); });
        auto* scExpand = new QShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | (Qt::Key_0 + d)), this);
        connect(scExpand, &QShortcut::activated, this, [this, d]() { expandToLevel(d); });
    }

    showEmptyPlaceholder();
}

void TestListPanel::showEmptyPlaceholder() {
    m_tree->clear();
    auto* item = new QTreeWidgetItem(m_tree);
    item->setText(0, "No tests loaded. Click Config then Load Tests.");
    item->setForeground(0, QColor("#999"));
    item->setFlags(Qt::NoItemFlags);
}

static bool itemChecked(QTreeWidgetItem* item) {
    return item->data(0, Qt::UserRole).toBool();
}
static void setItemChecked(QTreeWidgetItem* item, bool checked) {
    item->setData(0, Qt::UserRole, checked);
}

void TestListPanel::toggleItem(QTreeWidgetItem* item) {
    m_updating = true;
    bool newState = !itemChecked(item);
    if (item->childCount() > 0) {
        setItemChecked(item, newState);
        applyToDescendants(item, newState);
        updateItemText(item);
        // 向上传播
        QTreeWidgetItem* p = item->parent();
        while (p) { updateItemText(p); p = p->parent(); }
    } else {
        setItemChecked(item, newState);
        item->setText(0, (newState ? MARK_YES : MARK_NO) + "  " +
                       item->data(0, Role_CaseName).toString());
        QTreeWidgetItem* p = item->parent();
        while (p) { updateItemText(p); p = p->parent(); }
    }
    m_updating = false;
    m_tree->viewport()->update();
    updateStats();
    emit selectionChanged(selectedTests().size());
}

void TestListPanel::applyToDescendants(QTreeWidgetItem* parent, bool checked) {
    for (int i = 0; i < parent->childCount(); ++i) {
        auto* child = parent->child(i);
        if (child->isHidden()) { setItemChecked(child, false); continue; }
        setItemChecked(child, checked);
        if (child->childCount() > 0) {
            applyToDescendants(child, checked);
            updateItemText(child);
        } else {
            child->setText(0, (checked ? MARK_YES : MARK_NO) + "  " +
                           child->data(0, Role_CaseName).toString());
        }
    }
}

void TestListPanel::updateParentState(QTreeWidgetItem* item) {
    if (!item) return;
    int total = 0, checked = 0;
    for (int i = 0; i < item->childCount(); ++i) {
        auto* child = item->child(i);
        if (child->isHidden()) continue;
        ++total;
        if (itemChecked(child)) ++checked;
    }
    bool full = (checked == total && total > 0);
    bool half = (checked > 0 && checked < total);
    QString base = item->data(0, Role_SuiteName).toString();
    if (base.isEmpty()) base = item->text(0).section("  ", 1).trimmed();
    if (full) {
        setItemChecked(item, true);
        item->setText(0, MARK_YES + "  " + base);
    } else if (half) {
        setItemChecked(item, false);
        item->setText(0, MARK_HALF + "  " + base);
    } else {
        setItemChecked(item, false);
        item->setText(0, MARK_NO + "  " + base);
    }
    updateParentState(item->parent());
}

void TestListPanel::updateItemText(QTreeWidgetItem* item) {
    int total = 0, checked = 0;
    std::function<void(QTreeWidgetItem*)> countCases = [&](QTreeWidgetItem* it) {
        for (int i = 0; i < it->childCount(); ++i) {
            auto* child = it->child(i);
            if (child->data(0, Role_Type).toString() == "case") {
                if (child->isHidden()) continue;
                ++total;
                if (itemChecked(child)) ++checked;
            } else if (child->childCount() > 0) {
                countCases(child);
            }
        }
    };
    countCases(item);
    QString base = item->data(0, Role_SuiteName).toString();
    if (base.isEmpty()) return;
    bool full = (checked == total && total > 0);
    bool half = (checked > 0 && checked < total);
    QString cntStr = QString(" (%1/%2)").arg(checked).arg(total);
    if (full)      item->setText(0, MARK_YES + "  " + base + cntStr);
    else if (half) item->setText(0, MARK_HALF + "  " + base + cntStr);
    else           item->setText(0, MARK_NO + "  " + base + cntStr);
}

int TestListPanel::countVisibleLeaf() const {
    int n = 0;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        n += countVisibleLeafRec(m_tree->topLevelItem(i));
    return n;
}
int TestListPanel::countVisibleLeafRec(QTreeWidgetItem* item) const {
    if (item->isHidden()) return 0;
    if (item->childCount() == 0) return (item->flags() & Qt::ItemIsSelectable) ? 1 : 0;
    int n = 0;
    for (int i = 0; i < item->childCount(); ++i)
        n += countVisibleLeafRec(item->child(i));
    return n;
}

void TestListPanel::updateStats() {
    int total = countVisibleLeaf();
    int sel = selectedTests().size();
    m_lblStats->setText(QString("%1/%2").arg(sel).arg(total));
}

void TestListPanel::loadTests(const QVector<TestCase>& cases,
                               const QVector<TestCategory>& categories)
{
    m_tree->clear();
    m_searchEdit->clear();
    if (cases.isEmpty()) {
        showEmptyPlaceholder();
        updateStats();
        return;
    }
    m_updating = true;
    buildTree(cases, categories);
    m_updating = false;
    QTimer::singleShot(50, m_tree, &QTreeWidget::expandAll);
    updateStats();
    emit selectionChanged(0);
}

void TestListPanel::buildTree(const QVector<TestCase>& cases,
                               const QVector<TestCategory>& categories)
{
    QMap<QString, QVector<TestCase>> groups;
    for (const auto& tc : cases) groups[tc.suiteName].append(tc);
    auto catOf = [&](const QString& s) -> QString {
        for (const auto& c : categories)
            for (const auto& p : c.prefixes)
                if (s.startsWith(p)) return c.name;
        return "Other";
    };
    QMap<QString, QMap<QString, QVector<TestCase>>> catGroups;
    for (auto it = groups.begin(); it != groups.end(); ++it)
        catGroups[catOf(it.key())][it.key()] = it.value();

    for (auto ci = catGroups.begin(); ci != catGroups.end(); ++ci) {
        int catTotal = 0;
        auto* catItem = new QTreeWidgetItem(m_tree);
        catItem->setData(0, Role_Type, "category");
        QFont f = catItem->font(0); f.setBold(true); catItem->setFont(0, f);
        catItem->setExpanded(true);
        for (auto si = ci.value().begin(); si != ci.value().end(); ++si) {
            auto* suiteItem = new QTreeWidgetItem(catItem);
            suiteItem->setData(0, Role_Type, "suite");
            suiteItem->setData(0, Role_SuiteName, si.key());
            QFont sf = suiteItem->font(0); sf.setBold(true); suiteItem->setFont(0, sf);
            for (const auto& tc : si.value()) {
                auto* caseItem = new QTreeWidgetItem(suiteItem);
                caseItem->setText(0, MARK_NO + "  " + tc.caseName);
                caseItem->setData(0, Role_Type, "case");
                caseItem->setData(0, Role_SuiteName, tc.suiteName);
                caseItem->setData(0, Role_CaseName, tc.caseName);
            }
            int sc = si.value().size(); catTotal += sc;
            suiteItem->setText(0, MARK_NO + "  " + si.key() + QString(" (%1)").arg(sc));
        }
        catItem->setData(0, Role_SuiteName, ci.key());
        catItem->setText(0, MARK_NO + "  " + ci.key() + QString(" (%1)").arg(catTotal));
    }
}

QVector<TestCase> TestListPanel::selectedTests() const {
    QVector<TestCase> res;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        collectChecked(m_tree->topLevelItem(i), res);
    return res;
}
void TestListPanel::collectChecked(QTreeWidgetItem* item, QVector<TestCase>& out) const {
    if (!item || item->isHidden()) return;
    if (item->childCount() == 0 && itemChecked(item)) {
        TestCase tc;
        tc.suiteName = item->data(0, Role_SuiteName).toString();
        tc.caseName  = item->data(0, Role_CaseName).toString();
        if (!tc.suiteName.isEmpty() && !tc.caseName.isEmpty()) out.append(tc);
    }
    for (int i = 0; i < item->childCount(); ++i)
        collectChecked(item->child(i), out);
}

void TestListPanel::selectAll(bool select) {
    m_updating = true;
    std::function<void(QTreeWidgetItem*)> selVis = [&](QTreeWidgetItem* item) {
        for (int i = 0; i < item->childCount(); ++i) {
            auto* child = item->child(i);
            if (child->isHidden()) {
                setItemChecked(child, false);
            } else if (child->childCount() > 0) {
                selVis(child);
            } else {
                setItemChecked(child, select);
                child->setText(0, (select ? MARK_YES : MARK_NO) + "  " +
                               child->data(0, Role_CaseName).toString());
            }
        }
        if (!item->isHidden()) {
            updateItemText(item);
            QTreeWidgetItem* p = item->parent();
            while (p) { updateItemText(p); p = p->parent(); }
        }
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (!m_tree->topLevelItem(i)->isHidden()) selVis(m_tree->topLevelItem(i));
    m_updating = false;
    m_tree->viewport()->update();
    emit selectionChanged(selectedTests().size());
    updateStats();
    emit selectionChanged(selectedTests().size());
}

void TestListPanel::onFilterChanged(const QString& text) {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        applyFilter(m_tree->topLevelItem(i), text);
    // 隐藏的项取消选中 + 向上传播更新
    std::function<void(QTreeWidgetItem*)> deselHidden = [&](QTreeWidgetItem* item) {
        for (int i = 0; i < item->childCount(); ++i) {
            auto* child = item->child(i);
            if (child->isHidden()) {
                setItemChecked(child, false);
                if (child->data(0, Role_Type).toString() == "case")
                    child->setText(0, MARK_NO + "  " + child->data(0, Role_CaseName).toString());
            }
            if (child->childCount() > 0) deselHidden(child);
        }
        if (!item->isHidden()) {
            updateItemText(item);
            QTreeWidgetItem* p = item->parent();
            while (p) { updateItemText(p); p = p->parent(); }
        }
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (!m_tree->topLevelItem(i)->isHidden()) deselHidden(m_tree->topLevelItem(i));
    updateStats();
}
bool TestListPanel::applyFilter(QTreeWidgetItem* item, const QString& text) {
    if (!item) return false;
    if (item->childCount() == 0) {
        bool match = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
        return match;
    }
    bool any = false;
    for (int i = 0; i < item->childCount(); ++i)
        if (applyFilter(item->child(i), text)) any = true;
    item->setHidden(!any);
    return any;
}

void TestListPanel::collapseToLevel(int level) {
    if (level == 1) {
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            if (!m_tree->topLevelItem(i)->isHidden())
                m_tree->topLevelItem(i)->setExpanded(false);
    } else {
        std::function<void(QTreeWidgetItem*,int)> rec = [&](QTreeWidgetItem* item, int depth) {
            for (int i = 0; i < item->childCount(); i++) {
                auto* child = item->child(i);
                if (!child->isHidden()) {
                    if (depth == level) child->setExpanded(false);
                    rec(child, depth + 1);
                }
            }
        };
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            if (!m_tree->topLevelItem(i)->isHidden()) rec(m_tree->topLevelItem(i), 2);
    }
}
void TestListPanel::expandToLevel(int level) {
    if (level == 1) {
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            if (!m_tree->topLevelItem(i)->isHidden())
                m_tree->topLevelItem(i)->setExpanded(true);
    } else {
        std::function<void(QTreeWidgetItem*,int)> rec = [&](QTreeWidgetItem* item, int depth) {
            for (int i = 0; i < item->childCount(); i++) {
                auto* child = item->child(i);
                if (!child->isHidden()) {
                    if (depth == level) child->setExpanded(true);
                    rec(child, depth + 1);
                }
            }
        };
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            if (!m_tree->topLevelItem(i)->isHidden()) rec(m_tree->topLevelItem(i), 2);
    }
}
void TestListPanel::onSelectAllClicked()   { selectAll(true); }
void TestListPanel::onDeselectAllClicked() { selectAll(false); }
void TestListPanel::onReverseFilterClicked() {
    std::function<void(QTreeWidgetItem*)> rev = [&](QTreeWidgetItem* item) {
        if (!item->childCount()) {
            bool wasHidden = item->isHidden();
            item->setHidden(!wasHidden);
            if (!wasHidden) { setItemChecked(item, false); item->setText(0, MARK_NO + "  " + item->data(0, Role_CaseName).toString()); }
        } else {
            bool anyVis = false;
            for (int i = 0; i < item->childCount(); i++) {
                rev(item->child(i));
                if (!item->child(i)->isHidden()) anyVis = true;
            }
            item->setHidden(!anyVis);
            if (!item->isHidden()) updateItemText(item);
        }
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) { rev(m_tree->topLevelItem(i)); if (!m_tree->topLevelItem(i)->isHidden()) updateItemText(m_tree->topLevelItem(i)); }
    m_tree->viewport()->update();
    updateStats();
}
void TestListPanel::onExpandAllClicked()   {
    m_tree->expandAll();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        m_tree->topLevelItem(i)->setExpanded(true);
}
void TestListPanel::onCollapseAllClicked() {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        m_tree->topLevelItem(i)->setExpanded(false);
}

void TestListPanel::onTreeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;
    m_contextMenu->clear();
    QString type = item->data(0, Role_Type).toString();
    if (type.isEmpty()) return;
    bool checked = itemChecked(item);
    auto act = [this](auto fn) {
        return [this, fn]() {
            m_updating = true; fn(); m_updating = false;
            updateStats(); emit selectionChanged(selectedTests().size());
        };
    };
    if (type == "category" || type == "suite") {
        m_contextMenu->addAction("Select All", act([=]() {
            applyToDescendants(item, true); updateParentState(item);
        }));
        m_contextMenu->addAction("Clear All", act([=]() {
            applyToDescendants(item, false); updateParentState(item);
        }));
        m_contextMenu->addSeparator();
        m_contextMenu->addAction("Expand", [item]() { item->setExpanded(true); });
        m_contextMenu->addAction("Collapse", [item]() { item->setExpanded(false); });
    } else {
        m_contextMenu->addAction(checked ? "Deselect" : "Select",
            [this, item, checked]() { toggleItem(item); });
    }
    m_contextMenu->popup(m_tree->viewport()->mapToGlobal(pos));
}
