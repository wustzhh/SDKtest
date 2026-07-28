#include "TestListPanel.h"

#include <QHeaderView>
#include <QVBoxLayout>
#include <QShortcut>
#include <QHBoxLayout>
#include <QApplication>
#include <QStyle>
#include <QClipboard>
#include <QScreen>
#include <QPainter>
#include <QPropertyAnimation>

// 简易FlowLayout：自动换行的水平布局
class FlowLayout : public QLayout {
public:
    FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = 6, int vSpacing = 6)
        : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing) { setContentsMargins(margin,margin,margin,margin); }
    ~FlowLayout() { while (auto* item = takeAt(0)) delete item; }
    void addItem(QLayoutItem* item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int idx) const override { return (idx>=0&&idx<m_items.size())?m_items.at(idx):nullptr; }
    QLayoutItem* takeAt(int idx) override { return (idx>=0&&idx<m_items.size())?m_items.takeAt(idx):nullptr; }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return doLayout(QRect(0,0,w,0), true); }
    QSize minimumSize() const override { QSize s; for(auto* i:m_items) s=s.expandedTo(i->minimumSize()); return s; }
    void setGeometry(const QRect& rect) override { QLayout::setGeometry(rect); doLayout(rect, false); }
    QSize sizeHint() const override { return minimumSize(); }
private:
    int doLayout(const QRect& rect, bool testOnly) const {
        int x=rect.x(), y=rect.y(), lineH=0;
        for(auto* item:m_items){
            QSize sz=item->sizeHint();
            if(x+sz.width()>rect.right()&&x>rect.x()){x=rect.x();y+=lineH+m_vSpace;lineH=0;}
            if(!testOnly) item->setGeometry(QRect(QPoint(x,y),sz));
            x+=sz.width()+m_hSpace; lineH=qMax(lineH,sz.height());
        }
        return y+lineH-rect.y();
    }
    QList<QLayoutItem*> m_items;
    int m_hSpace, m_vSpace;
};

// 网页风格Toggle开关
class ToggleSwitch : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled)
public:
    ToggleSwitch(QWidget* p=nullptr):QWidget(p),m_checked(false),m_animPos(0){
        setFixedSize(40,22);setCursor(Qt::PointingHandCursor);
        m_anim=new QPropertyAnimation(this,"animPos");m_anim->setDuration(150);
    }
    bool isChecked() const { return m_checked; }
    void setChecked(bool c) {
        if(c==m_checked) return;
        m_checked=c;
        m_anim->stop();m_anim->setStartValue(m_animPos);m_anim->setEndValue(c?1.0:0.0);m_anim->start();
        emit toggled(m_checked);
    }
signals:
    void toggled(bool);
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        double t=m_animPos;
        p.setBrush(QColor(t>0.5?QString("#6366f1"):QString("#d1d5db")));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(),11,11);
        p.setBrush(Qt::white);
        p.drawEllipse((int)(1+t*(width()-20)),1,20,20);
    }
    void mousePressEvent(QMouseEvent*) override { setChecked(!m_checked); update(); }
private:
    bool m_checked;
    double m_animPos;
    QPropertyAnimation* m_anim;
};

// ═══════════════════════════════════════════════════════════
//  AdvancedFilterDialog
// ═══════════════════════════════════════════════════════════
AdvancedFilterDialog::AdvancedFilterDialog(const QVector<TestCase>& allCases, QWidget* parent)
    : QDialog(parent), m_allCases(allCases)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    auto* screen = QApplication::primaryScreen();
    if (screen) setGeometry(screen->geometry());
    setAutoFillBackground(false);
    
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(80, 60, 80, 60);
    lay->setSpacing(14);

    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel(QString::fromUtf8("\xf0\x9f\x94\x8d \xe9\xab\x98\xe7\xba\xa7\xe7\xad\x9b\xe9\x80\x89"));
    title->setStyleSheet("background:transparent;font-size:20px;font-weight:700;color:white;");
    titleRow->addWidget(title);
    titleRow->addStretch();
    auto* toggleLabel = new QLabel(QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8"));
    toggleLabel->setStyleSheet("background:transparent;font-size:14px;color:rgba(255,255,255,0.8);");
    titleRow->addWidget(toggleLabel);
    auto* btnOn = new QPushButton(QString::fromUtf8("\xe5\xbc\x80"));
    btnOn->setFixedSize(32,22);
    btnOn->setCheckable(true); btnOn->setChecked(false);
    btnOn->setStyleSheet("QPushButton:checked{background:#10b981;color:white;border:none;border-radius:3px;font-size:10px;}QPushButton{background:#374151;color:#9ca3af;border:none;border-radius:3px;font-size:10px;}");
    auto* btnOff = new QPushButton(QString::fromUtf8("\xe5\x85\xb3"));
    btnOff->setFixedSize(32,22);
    btnOff->setCheckable(true); btnOff->setChecked(true);
    btnOff->setStyleSheet("QPushButton:checked{background:#ef4444;color:white;border:none;border-radius:3px;font-size:10px;}QPushButton{background:#374151;color:#9ca3af;border:none;border-radius:3px;font-size:10px;}");
    m_enabled = false;
    auto toggleEnable = [this,btnOn,btnOff](bool on){
        m_enabled=on; btnOn->setChecked(on); btnOff->setChecked(!on);
        btnOn->setStyleSheet(on?"QPushButton{background:#10b981;color:white;border:none;border-radius:3px;font-size:10px;}":"QPushButton{background:#374151;color:#9ca3af;border:none;border-radius:3px;font-size:10px;}");
        btnOff->setStyleSheet(!on?"QPushButton{background:#ef4444;color:white;border:none;border-radius:3px;font-size:10px;}":"QPushButton{background:#374151;color:#9ca3af;border:none;border-radius:3px;font-size:10px;}");
        emit filterChanged();
    };
    connect(btnOn,&QPushButton::clicked,this,[toggleEnable](){ toggleEnable(true); });
    connect(btnOff,&QPushButton::clicked,this,[toggleEnable](){ toggleEnable(false); });
    titleRow->addWidget(btnOn);
    titleRow->addWidget(btnOff);
    lay->addLayout(titleRow);

    auto* inputRow = new QHBoxLayout;
    m_input = new QLineEdit;
    m_input->setPlaceholderText(QString::fromUtf8("\xe8\xbe\x93\xe5\x85\xa5\xe5\x85\xb3\xe9\x94\xae\xe8\xaf\x8d\xe5\x90\x8e\xe5\x9b\x9e\xe8\xbd\xa6..."));
    m_input->setStyleSheet("background:rgba(255,255,255,0.95);border:none;border-radius:10px;padding:12px 16px;font-size:15px;");
    connect(m_input, &QLineEdit::returnPressed, this, &AdvancedFilterDialog::addRule);
    inputRow->addWidget(m_input, 1);
    lay->addLayout(inputRow);

    m_ruleWidget = new QWidget;
    m_ruleWidget->setStyleSheet("background:rgba(255,255,255,0.08);border-radius:10px;");
    m_ruleLayout = new FlowLayout(m_ruleWidget, 12, 10, 8);
    lay->addWidget(m_ruleWidget);

    m_previewTree = new QTreeWidget;
    m_previewTree->setHeaderHidden(true);
    m_previewTree->setRootIsDecorated(true);
    m_previewTree->setStyleSheet("QTreeWidget{background:rgba(255,255,255,0.95);border-radius:10px;font-size:14px;padding:4px;}");
    lay->addWidget(m_previewTree, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* btnApply = new QPushButton(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8"));
    btnApply->setStyleSheet("QPushButton{background:#10b981;color:white;border-radius:8px;padding:10px 24px;font-size:15px;font-weight:600;}");
    auto* btnSave = new QPushButton(QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98"));
    btnSave->setStyleSheet("QPushButton{background:#6366f1;color:white;border-radius:8px;padding:10px 24px;font-size:15px;font-weight:600;}");
    auto* btnClose = new QPushButton(QString::fromUtf8("\xe5\x85\xb3\xe9\x97\xad"));
    btnClose->setStyleSheet("QPushButton{background:rgba(255,255,255,0.2);color:white;border-radius:8px;padding:10px 24px;font-size:15px;}");
    connect(btnSave, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnApply, &QPushButton::clicked, this, [this](){ emit filterChanged(); });
    btnRow->addWidget(btnApply); btnRow->addWidget(btnSave); btnRow->addWidget(btnClose);
    lay->addLayout(btnRow);
    updatePreview();
}

bool AdvancedFilterDialog::enabled() const { return m_enabled; }

void AdvancedFilterDialog::addRule() {
    QString kw = m_input->text().trimmed();
    if (kw.isEmpty()) return;
    m_input->clear();
    m_rules.append({kw, true});
    rebuildRuleWidgets();
    updatePreview();
    emit filterChanged();
}

void AdvancedFilterDialog::removeRule(int idx) {
    if (idx < 0 || idx >= m_rules.size()) return;
    m_rules.remove(idx);
    rebuildRuleWidgets();
    updatePreview();
    emit filterChanged();
}

void AdvancedFilterDialog::toggleRule(int idx) {
    if (idx < 0 || idx >= m_rules.size()) return;
    m_rules[idx].include = !m_rules[idx].include;
    rebuildRuleWidgets();
    updatePreview();
    emit filterChanged();
}

void AdvancedFilterDialog::rebuildRuleWidgets() {
    QLayoutItem* child;
    while ((child = m_ruleLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }
    for (int i = 0; i < m_rules.size(); i++) {
        const auto& r = m_rules[i];
        auto* chip = new QWidget;
        chip->setStyleSheet(QString("background:%1;border-radius:6px;").arg(r.include ? "#d1fae5" : "#fee2e2"));
        chip->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(chip, &QWidget::customContextMenuRequested, this, [this,i](){ removeRule(i); });
        auto* hl = new QHBoxLayout(chip);
        hl->setContentsMargins(6,2,6,2);
        hl->setSpacing(4);
        auto* label = new QLabel(r.keyword);
        label->setStyleSheet("background:transparent;font-size:12px;color:#374151;border:none;");
        QString elided = label->fontMetrics().elidedText(r.keyword, Qt::ElideRight, 120);
        label->setText(elided);
        if (elided != r.keyword) label->setToolTip(r.keyword);
        hl->addWidget(label);
        // 正/反：两个小按钮
        auto* btnYes = new QPushButton(QString::fromUtf8("\xe6\xad\xa3"));
        btnYes->setFixedSize(24,18);
        btnYes->setCheckable(true); btnYes->setChecked(r.include);
        btnYes->setStyleSheet(r.include
            ? "QPushButton{background:#10b981;color:white;border:none;border-radius:3px;font-size:10px;}"
            : "QPushButton{background:#e5e7eb;color:#9ca3af;border:none;border-radius:3px;font-size:10px;}");
        auto* btnNo = new QPushButton(QString::fromUtf8("\xe5\x8f\x8d"));
        btnNo->setFixedSize(24,18);
        btnNo->setCheckable(true); btnNo->setChecked(!r.include);
        btnNo->setStyleSheet(!r.include
            ? "QPushButton{background:#ef4444;color:white;border:none;border-radius:3px;font-size:10px;}"
            : "QPushButton{background:#e5e7eb;color:#9ca3af;border:none;border-radius:3px;font-size:10px;}");
        auto toggle = [this,i,btnYes,btnNo,chip](bool inc){
            m_rules[i].include=inc;
            btnYes->setChecked(inc); btnNo->setChecked(!inc);
            btnYes->setStyleSheet(inc?"QPushButton{background:#10b981;color:white;border:none;border-radius:3px;font-size:10px;}":"QPushButton{background:#e5e7eb;color:#9ca3af;border:none;border-radius:3px;font-size:10px;}");
            btnNo->setStyleSheet(!inc?"QPushButton{background:#ef4444;color:white;border:none;border-radius:3px;font-size:10px;}":"QPushButton{background:#e5e7eb;color:#9ca3af;border:none;border-radius:3px;font-size:10px;}");
            chip->setStyleSheet(QString("background:%1;border-radius:6px;").arg(inc?"#d1fae5":"#fee2e2"));
            rebuildRuleWidgets(); updatePreview(); emit filterChanged();
        };
        connect(btnYes,&QPushButton::clicked,this,[toggle](){ toggle(true); });
        connect(btnNo,&QPushButton::clicked,this,[toggle](){ toggle(false); });
        hl->addWidget(btnYes);
        hl->addWidget(btnNo);
        m_ruleLayout->addWidget(chip);
    }
    m_ruleWidget->updateGeometry();
}

QVector<TestCase> AdvancedFilterDialog::applyFilter() const {
    if (m_rules.isEmpty() || !m_enabled) return m_allCases;
    QVector<TestCase> result;
    for (const auto& tc : m_allCases) {
        QString name = tc.fullName();
        bool pass = true;
        for (const auto& r : m_rules) {
            bool match = name.contains(r.keyword, Qt::CaseInsensitive);
            if (r.include && !match) { pass = false; break; }
            if (!r.include && match) { pass = false; break; }
        }
        if (pass) result.append(tc);
    }
    return result;
}

void AdvancedFilterDialog::updatePreview() {
    m_previewTree->clear();
    auto filtered = applyFilter();
    QMap<QString, QVector<TestCase>> groups;
    for (const auto& tc : filtered) groups[tc.suiteName].append(tc);
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        auto* suiteItem = new QTreeWidgetItem(m_previewTree);
        suiteItem->setText(0, it.key() + QString(" (%1)").arg(it.value().size()));
        for (const auto& tc : it.value()) {
            auto* caseItem = new QTreeWidgetItem(suiteItem);
            caseItem->setText(0, tc.caseName);
        }
    }
}

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

    // Header: path + search
    m_pathLabel = new QLabel(this);
    m_pathLabel->setStyleSheet("font-size:11px;color:#6366f1;padding:2px 4px;min-height:16px");
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_pathLabel);

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
    QString tbBtn = "QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;"
                    "padding:4px 12px;font-size:13px;min-width:44px}"
                    "QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}";
    m_btnSelectAll   = new QPushButton("全选", this);
    m_btnSelectAll->setFixedHeight(28);m_btnSelectAll->setStyleSheet(tbBtn);
    m_btnDeselectAll = new QPushButton("全消", this);
    m_btnDeselectAll->setFixedHeight(28);m_btnDeselectAll->setStyleSheet(tbBtn);
    m_btnReverseFilter = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x8d \xe7\xad\x9b\xe9\x80\x89"), this);
    m_btnReverseFilter->setFixedHeight(28);m_btnReverseFilter->setStyleSheet(tbBtn);
    m_btnReverseFilter->setToolTip(QString::fromUtf8("\xe9\xab\x98\xe7\xba\xa7\xe7\xad\x9b\xe9\x80\x89"));
    m_lblStats = new QLabel("0", this);
    tb->addWidget(m_btnSelectAll);
    tb->addWidget(m_btnDeselectAll);
    tb->addWidget(m_btnReverseFilter);
    tb->addStretch();
    tb->addWidget(m_lblStats);
    layout->addWidget(m_toolbar);
    connect(m_btnSelectAll,   &QPushButton::clicked, this, &TestListPanel::onSelectAllClicked);
    connect(m_btnDeselectAll, &QPushButton::clicked, this, &TestListPanel::onDeselectAllClicked);
    connect(m_btnReverseFilter, &QPushButton::clicked, this, &TestListPanel::onAdvancedFilter);

    // Tree
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(1);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(16);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setStyleSheet(
        "QTreeWidget { font-size:13px; border:1px solid #e2e8f0; border-radius:6px; background:#ffffff; }"
        "QTreeWidget::item { padding:2px 6px; min-height:22px; color:#1e293b; }"
        "QTreeWidget::item:hover { background:#f8f9fb; }"
        "QTreeWidget{outline:none;}");
    m_tree->setMinimumWidth(0);
    m_tree->setExpandsOnDoubleClick(false);
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item || m_updating) return;
        // 取消上次高亮
        if (m_lastHighlighted) {
            m_lastHighlighted->setBackground(0, QBrush());
            m_lastHighlighted->setForeground(0, QBrush());
        }
        toggleItem(item);
        // 高亮当前项
        m_lastHighlighted = item;
        item->setBackground(0, QColor(0x63,0x66,0xf1));   // 紫色背景
        item->setForeground(0, QColor(0xff,0xff,0xff));    // 白字
        updatePathLabel(item);
    });
    layout->addWidget(m_tree, 1);

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &TestListPanel::onTreeContextMenu);

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
    item->setText(0, QString::fromUtf8("\xe6\x9c\xaa\xe5\x8a\xa0\xe8\xbd\xbd\xe6\xb5\x8b\xe8\xaf\x95\xef\xbc\x8c\xe8\xaf\xb7\xe5\x85\x88\xe9\x85\x8d\xe7\xbd\xae\xe5\xb9\xb6\xe5\x8a\xa0\xe8\xbd\xbd"));
    item->setForeground(0, QColor("#5a6278"));
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
    m_allCases = cases;  // 保存全部用例用于高级筛选
    m_lastHighlighted = nullptr;
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

void TestListPanel::buildGroupTree(QTreeWidgetItem* parent,
                                    const QVector<TestCase>& cases,
                                    const QVector<TestCategory>& categories)
{
    QMap<QString, QVector<TestCase>> groups;
    for (const auto& tc : cases) groups[tc.suiteName].append(tc);
    // 分类匹配：精确匹配套件名或完整用例名
    auto catOf = [&](const QString& suite, const QVector<TestCase>& suiteCases) -> QString {
        for (const auto& c : categories)
            for (const auto& p : c.prefixes) {
                // 精确匹配套件名
                if (suite == p) return c.name;
                // 也检查完整用例名（suite.case）
                for (const auto& tc : suiteCases)
                    if ((suite + "." + tc.caseName) == p) return c.name;
            }
        return "Other";
    };
    QMap<QString, QMap<QString, QVector<TestCase>>> catGroups;
    for (auto it = groups.begin(); it != groups.end(); ++it)
        catGroups[catOf(it.key(), it.value())][it.key()] = it.value();

    for (auto ci = catGroups.begin(); ci != catGroups.end(); ++ci) {
        int catTotal = 0;
        auto* catItem = new QTreeWidgetItem(parent);
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
                caseItem->setToolTip(0, tc.suiteName + "." + tc.caseName);
            }
            int sc = si.value().size(); catTotal += sc;
            suiteItem->setText(0, MARK_NO + "  " + si.key() + QString(" (%1)").arg(sc));
            suiteItem->setToolTip(0, si.key());
        }
        catItem->setData(0, Role_SuiteName, ci.key());
        catItem->setText(0, MARK_NO + "  " + ci.key() + QString(" (%1)").arg(catTotal));
    }
}

void TestListPanel::buildTree(const QVector<TestCase>& cases,
                               const QVector<TestCategory>& categories)
{
    // 分参数化和非参数化
    QVector<TestCase> paramCases, normalCases;
    for (const auto& tc : cases) {
        if (tc.suiteName.contains('/'))
            paramCases.append(tc);
        else
            normalCases.append(tc);
    }

    auto addGroup = [&](const QString& title, const QVector<TestCase>& group) {
        if (group.isEmpty()) return;
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, MARK_NO + "  " + title);
        item->setData(0, Role_Type, "category");
        item->setData(0, Role_SuiteName, title);
        QFont f = item->font(0); f.setBold(true); item->setFont(0, f);
        item->setExpanded(true);
        buildGroupTree(item, group, categories);
        // 更新总数
        int total = 0;
        std::function<void(QTreeWidgetItem*)> cnt = [&](QTreeWidgetItem* it) {
            for (int i = 0; i < it->childCount(); ++i) {
                auto* ch = it->child(i);
                if (ch->data(0, Role_Type).toString() == "case") total++;
                else if (ch->childCount() > 0) cnt(ch);
            }
        };
        cnt(item);
        item->setText(0, MARK_NO + "  " + title + QString(" (%1)").arg(total));
    };

    addGroup(QString::fromUtf8("\xe5\x8f\x82\xe6\x95\xb0\xe5\x8c\x96\xe6\xb5\x8b\xe8\xaf\x95"), paramCases);
    addGroup(QString::fromUtf8("\xe9\x9d\x9e\xe5\x8f\x82\xe6\x95\xb0\xe5\x8c\x96\xe6\xb5\x8b\xe8\xaf\x95"), normalCases);
}

QStringList TestListPanel::selectedTestNames() const {
    QStringList names;
    std::function<void(QTreeWidgetItem*)> collect = [&](QTreeWidgetItem* item) {
        if (!item || item->isHidden()) return;
        QString t = item->data(0, Role_Type).toString();
        if (t == "case" && itemChecked(item)) {
            QString s = item->data(0, Role_SuiteName).toString();
            QString c = item->data(0, Role_CaseName).toString();
            if (!s.isEmpty()) names << s + "." + c;
        }
        for (int i = 0; i < item->childCount(); i++) collect(item->child(i));
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) collect(m_tree->topLevelItem(i));
    return names;
}

void TestListPanel::setSelectedTestNames(const QStringList& names) {
    QSet<QString> ns(names.begin(), names.end());
    m_updating = true;
    // 第一遍：逐个设 case 的状态和文字（不改父节点）
    std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* item) {
        if (!item) return;
        if (item->data(0, Role_Type).toString() == "case") {
            QString s = item->data(0, Role_SuiteName).toString();
            QString c = item->data(0, Role_CaseName).toString();
            bool sel = ns.contains(s + "." + c);
            setItemChecked(item, sel);
            item->setText(0, (sel ? MARK_YES : MARK_NO) + "  " + c);
        }
        for (int i = 0; i < item->childCount(); i++) apply(item->child(i));
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) apply(m_tree->topLevelItem(i));
    // 第二遍：只刷新父节点（suite/category）
    std::function<void(QTreeWidgetItem*)> refreshParents = [&](QTreeWidgetItem* item) {
        if (!item) return;
        if (item->data(0, Role_Type).toString() != "case" && item->childCount() > 0)
            updateItemText(item);
        for (int i = 0; i < item->childCount(); i++) refreshParents(item->child(i));
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) refreshParents(m_tree->topLevelItem(i));
    m_updating = false;
    updateStats();
    emit selectionChanged(selectedTests().size());
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
    m_tree->viewport()->update();
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
            else { setItemChecked(item, true); item->setText(0, MARK_YES + "  " + item->data(0, Role_CaseName).toString()); }
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

void TestListPanel::onAdvancedFilter() {
    AdvancedFilterDialog dlg(m_allCases, this);
    if (dlg.exec() == QDialog::Accepted && dlg.enabled()) {
        auto filtered = dlg.rules();
        if (filtered.isEmpty()) return;
        std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* item) {
            if (item->childCount() == 0) {
                QString suite = item->data(0, Role_SuiteName).toString();
                QString name  = item->data(0, Role_CaseName).toString();
                QString full = suite + "." + name;
                bool pass = true;
                for (const auto& r : filtered) {
                    bool match = full.contains(r.keyword, Qt::CaseInsensitive);
                    if (r.include && !match) { pass = false; break; }
                    if (!r.include && match) { pass = false; break; }
                }
                item->setHidden(!pass);
                if (!pass) { setItemChecked(item, false); item->setText(0, MARK_NO + "  " + name); }
            } else {
                bool anyVis = false;
                for (int i = 0; i < item->childCount(); i++) {
                    apply(item->child(i));
                    if (!item->child(i)->isHidden()) anyVis = true;
                }
                item->setHidden(!anyVis);
                if (!item->isHidden()) updateItemText(item);
            }
        };
        m_updating = true;
        for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
            apply(m_tree->topLevelItem(i));
            if (!m_tree->topLevelItem(i)->isHidden()) updateItemText(m_tree->topLevelItem(i));
        }
        m_updating = false;
        m_tree->viewport()->update();
        updateStats();
        emit selectionChanged(selectedTests().size());
    }
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
        m_contextMenu->addAction(QString::fromUtf8("\xe5\x85\xa8\xe9\x80\x89"), act([=]() {
            applyToDescendants(item, true); updateParentState(item);
        }));
        m_contextMenu->addAction(QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88\xe5\x85\xa8\xe9\x80\x89"), act([=]() {
            applyToDescendants(item, false); updateParentState(item);
        }));
        m_contextMenu->addSeparator();
        m_contextMenu->addAction(QString::fromUtf8("\xe5\xb1\x95\xe5\xbc\x80"), [item]() { item->setExpanded(true); });
        m_contextMenu->addAction(QString::fromUtf8("\xe6\x8a\x98\xe5\x8f\xa0"), [item]() { item->setExpanded(false); });
    } else {
        m_contextMenu->addAction(checked ? QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88\xe9\x80\x89\xe4\xb8\xad") : QString::fromUtf8("\xe9\x80\x89\xe4\xb8\xad"),
            [this, item, checked]() { toggleItem(item); });
    }
    m_contextMenu->addSeparator();
    // 复制当前节点名称
    QString fullName;
    if (type == "case") {
        fullName = item->data(0, Role_SuiteName).toString() + "." + item->data(0, Role_CaseName).toString();
    } else if (type == "suite") {
        fullName = item->data(0, Role_SuiteName).toString();
    } else if (type == "category") {
        fullName = item->data(0, Role_SuiteName).toString();
    }
    if (!fullName.isEmpty()) {
        m_contextMenu->addAction(QString::fromUtf8("\xe5\xa4\x8d\xe5\x88\xb6\xe5\x90\x8d\xe7\xa7\xb0"), [fullName]() {
            QApplication::clipboard()->setText(fullName);
        });
    }
    // 复制该节点下所有选中用例的完整名（用于报告/对比）
    m_contextMenu->addAction(QString::fromUtf8("\xe5\xa4\x8d\xe5\x88\xb6\xe6\x89\x80\xe6\x9c\x89\xe9\x80\x89\xe4\xb8\xad\xe7\x94\xa8\xe4\xbe\x8b"), [this, item, fullName]() {
        QStringList names;
        std::function<void(QTreeWidgetItem*)> collect = [&](QTreeWidgetItem* it) {
            if (!it || it->isHidden()) return;
            if (it->data(0, Role_Type).toString() == "case" && itemChecked(it)) {
                QString sn = it->data(0, Role_SuiteName).toString();
                QString cn = it->data(0, Role_CaseName).toString();
                if (!sn.isEmpty()) names << sn + "." + cn;
            }
            for (int i = 0; i < it->childCount(); i++) collect(it->child(i));
        };
        collect(item);
        if (names.isEmpty()) {
            // 如果是case节点且被选中，复制自身
            names << fullName;
        }
        QApplication::clipboard()->setText(names.join("\n"));
    });
    m_contextMenu->popup(m_tree->viewport()->mapToGlobal(pos));
}

void TestListPanel::updatePathLabel(QTreeWidgetItem* item) {
    if (!m_pathLabel) return;
    QStringList parts;
    QTreeWidgetItem* cur = item;
    while (cur) {
        QString t;
        if (cur->data(0, Role_Type).toString() == "case")
            t = cur->data(0, Role_CaseName).toString();
        else if (cur->data(0, Role_Type).toString() == "suite")
            t = cur->data(0, Role_SuiteName).toString();
        else if (cur->data(0, Role_Type).toString() == "category")
            t = cur->data(0, Role_SuiteName).toString();
        else if (cur->childCount() > 0)
            t = cur->text(0).section("  ", -1).trimmed();
        if (!t.isEmpty()) parts.prepend(t);
        cur = cur->parent();
    }
    if (parts.isEmpty()) { m_pathLabel->setText(""); return; }
    // 每个层级一行，缩进表示深度
    QString html;
    for (int i = 0; i < parts.size(); i++) {
        if (i > 0) html += "<br>";
        html += QString("&nbsp;&nbsp;&nbsp;&nbsp;").repeated(i) + parts[i];
    }
    m_pathLabel->setText(html);
}
#include "TestListPanel.moc"
