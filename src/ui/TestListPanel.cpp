#include "TestListPanel.h"

#include <QHeaderView>
#include <QVBoxLayout>
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

    // Header: collapse + search
    auto* headerRow = new QHBoxLayout();
    m_btnCollapsePanel = new QPushButton("\u25C0", this);
    m_btnCollapsePanel->setFixedSize(18, 22);
    m_btnCollapsePanel->setToolTip("Hide panel");
    connect(m_btnCollapsePanel, &QPushButton::clicked, this, &TestListPanel::collapseRequested);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &TestListPanel::onFilterChanged);

    headerRow->addWidget(m_btnCollapsePanel);
    headerRow->addWidget(m_searchEdit, 1);
    layout->addLayout(headerRow);

    // Toolbar
    m_toolbar = new QWidget(this);
    auto* tb = new QHBoxLayout(m_toolbar);
    tb->setContentsMargins(0, 0, 0, 0);
    tb->setSpacing(1);
    m_btnExpand   = new QPushButton("Expand", this);
    m_btnCollapse = new QPushButton("Fold", this);
    m_btnSelectAll   = new QPushButton("All", this);
    m_btnDeselectAll = new QPushButton("None", this);
    m_lblStats = new QLabel("0", this);
    tb->addWidget(m_btnExpand);
    tb->addWidget(m_btnCollapse);
    tb->addWidget(m_btnSelectAll);
    tb->addWidget(m_btnDeselectAll);
    tb->addStretch();
    tb->addWidget(m_lblStats);
    layout->addWidget(m_toolbar);
    connect(m_btnExpand,   &QPushButton::clicked, this, &TestListPanel::onExpandAllClicked);
    connect(m_btnCollapse, &QPushButton::clicked, this, &TestListPanel::onCollapseAllClicked);
    connect(m_btnSelectAll,   &QPushButton::clicked, this, &TestListPanel::onSelectAllClicked);
    connect(m_btnDeselectAll, &QPushButton::clicked, this, &TestListPanel::onDeselectAllClicked);

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
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item || m_updating) return;
        toggleItem(item);
    });
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &TestListPanel::onTreeContextMenu);
    layout->addWidget(m_tree, 1);

    m_contextMenu = new QMenu(this);
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
    } else {
        setItemChecked(item, newState);
        item->setText(0, (newState ? MARK_YES : MARK_NO) + "  " +
                       item->data(0, Role_CaseName).toString());
        updateParentState(item->parent());
    }
    m_updating = false;
    updateStats();
    emit selectionChanged(selectedTests().size());
}

void TestListPanel::applyToDescendants(QTreeWidgetItem* parent, bool checked) {
    for (int i = 0; i < parent->childCount(); ++i) {
        auto* child = parent->child(i);
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
        ++total;
        if (itemChecked(item->child(i))) ++checked;
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
    for (int i = 0; i < item->childCount(); ++i) {
        ++total;
        if (itemChecked(item->child(i))) ++checked;
    }
    bool full = (checked == total && total > 0);
    bool half = (checked > 0 && checked < total);
    QString base = item->data(0, Role_SuiteName).toString();
    if (base.isEmpty()) base = item->text(0).section("  ", 1).trimmed();
    QString cnt = QString(" (%1/%2)").arg(checked).arg(total);
    if (full)      item->setText(0, MARK_YES + "  " + base + cnt);
    else if (half) item->setText(0, MARK_HALF + "  " + base + cnt);
    else           item->setText(0, MARK_NO + "  " + base + cnt);
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
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        applyToDescendants(m_tree->topLevelItem(i), select);
    m_updating = false;
    updateStats();
    emit selectionChanged(selectedTests().size());
}

void TestListPanel::onFilterChanged(const QString& text) {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        applyFilter(m_tree->topLevelItem(i), text);
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

void TestListPanel::onSelectAllClicked()   { selectAll(true); }
void TestListPanel::onDeselectAllClicked() { selectAll(false); }
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
