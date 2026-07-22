#include "FilterEditDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QWheelEvent>

FilterEditDialog::FilterEditDialog(const QVector<FilterSet>& filterSets,
                                     const QStringList& propertyKeys,
                                     const QMap<QString, QStringList>& propertyValues,
                                     QWidget* parent)
    : QDialog(parent), m_filterSets(filterSets), m_propertyKeys(propertyKeys), m_propertyValues(propertyValues)
{
    setWindowTitle(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe7\xad\x9b\xe9\x80\x89\xe6\x9d\xa1\xe4\xbb\xb6"));
    resize(900, 600);
    setMinimumSize(700, 400);

    auto* mainLayout = new QVBoxLayout(this);

    // ── 左右分栏 ──
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // 左列：组列表
    auto* leftWidget = new QWidget(splitter);
    auto* leftLay = new QVBoxLayout(leftWidget);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->addWidget(new QLabel(QString::fromUtf8("\xe7\xad\x9b\xe9\x80\x89\xe7\xbb\x84")));

    m_groupList = new QListWidget(leftWidget);
    m_groupList->setStyleSheet("QListWidget::item{padding:6px 10px;font-size:13px}"
                               " QListWidget::item:selected{background:#eef2ff;color:#1e293b}");
    leftLay->addWidget(m_groupList, 1);

    auto* glBtns = new QHBoxLayout;
    m_btnNew    = new QPushButton(QString::fromUtf8("\xe6\x96\xb0\xe5\xbb\xba"));
    m_btnDelete = new QPushButton(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"));
    m_btnRename = new QPushButton(QString::fromUtf8("\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d"));
    QString gbStyle = "QPushButton{font-size:12px;padding:3px 10px;background:#fff;border:1px solid #e2e8f0;border-radius:4px}"
                      " QPushButton:hover{background:#f1f5f9}";
    m_btnNew->setStyleSheet(gbStyle); m_btnDelete->setStyleSheet(gbStyle); m_btnRename->setStyleSheet(gbStyle);
    glBtns->addWidget(m_btnNew); glBtns->addWidget(m_btnDelete); glBtns->addWidget(m_btnRename);
    glBtns->addStretch();
    leftLay->addLayout(glBtns);

    splitter->addWidget(leftWidget);

    // 右列：条件编辑
    auto* rightWidget = new QWidget(splitter);
    auto* rightLay = new QVBoxLayout(rightWidget);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->addWidget(new QLabel(QString::fromUtf8("\xe6\x9d\xa1\xe4\xbb\xb6")));

    // AND/OR 模式切换
    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(QString::fromUtf8("\xe5\x8c\xb9\xe9\x85\x8d\xe6\xa8\xa1\xe5\xbc\x8f:")));
    m_modeCombo = new QComboBox;
    m_modeCombo->addItems({QString::fromUtf8("AND (\xe6\x89\x80\xe6\x9c\x89\xe6\x9d\xa1\xe4\xbb\xb6)"), QString::fromUtf8("OR  (\xe4\xbb\xbb\xe6\x84\x8f\xe6\x9d\xa1\xe4\xbb\xb6)")});
    modeRow->addWidget(m_modeCombo);
    modeRow->addStretch();
    rightLay->addLayout(modeRow);

    m_condTable = new QTableWidget(rightWidget);
    m_condTable->setColumnCount(3);
    m_condTable->setHorizontalHeaderLabels({QString::fromUtf8("\xe5\xb1\x9e\xe6\x80\xa7"),
        QString::fromUtf8("\xe5\x8c\xb9\xe9\x85\x8d"), QString::fromUtf8("\xe5\x80\xbc")});
    m_condTable->horizontalHeader()->setStretchLastSection(false);
    m_condTable->setColumnWidth(0, 180);
    m_condTable->setColumnWidth(1, 120);
    m_condTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_condTable->horizontalHeader()->setDefaultSectionSize(32);
    m_condTable->horizontalHeader()->setStyleSheet("QHeaderView::section{font-size:13px;padding:4px 8px;min-height:28px}");
    m_condTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_condTable->setFocusPolicy(Qt::NoFocus);
    m_condTable->setStyleSheet("QTableWidget::item{padding:3px 8px;font-size:13px}"
                               " QComboBox{font-size:13px;min-height:22px;padding:1px 4px}"
                               " QLineEdit{font-size:13px;min-height:20px}");
    m_condTable->verticalHeader()->setDefaultSectionSize(34);
    m_condTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    rightLay->addWidget(m_condTable, 1);

    auto* crBtns = new QHBoxLayout;
    m_btnAddCond    = new QPushButton(QString::fromUtf8("+ \xe6\xb7\xbb\xe5\x8a\xa0\xe6\x9d\xa1\xe4\xbb\xb6"));
    m_btnRemoveCond = new QPushButton(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4\xe9\x80\x89\xe4\xb8\xad"));
    m_btnAddCond->setStyleSheet(gbStyle); m_btnRemoveCond->setStyleSheet(gbStyle);
    crBtns->addWidget(m_btnAddCond); crBtns->addWidget(m_btnRemoveCond);
    crBtns->addStretch();
    rightLay->addLayout(crBtns);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    mainLayout->addWidget(splitter, 1);

    // ── 底部按钮 ──
    auto* bottom = new QHBoxLayout;
    bottom->addStretch();
    auto* btnOk = new QPushButton(QString::fromUtf8("\xe7\xa1\xae\xe5\xae\x9a"));
    auto* btnCancel = new QPushButton(QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88"));
    btnOk->setStyleSheet("QPushButton{background:#6366f1;color:#fff;border:none;border-radius:6px;padding:6px 20px;font-size:13px}"
                         "QPushButton:hover{background:#4f46e5}");
    btnCancel->setStyleSheet("QPushButton{background:#fff;border:1px solid #e2e8f0;border-radius:6px;padding:6px 20px;font-size:13px}"
                             "QPushButton:hover{background:#f1f5f9}");
    bottom->addWidget(btnCancel); bottom->addWidget(btnOk);
    mainLayout->addLayout(bottom);

    // ── 连接 ──
    connect(m_groupList, &QListWidget::currentRowChanged, this, &FilterEditDialog::onGroupSelected);
    connect(m_btnNew, &QPushButton::clicked, this, &FilterEditDialog::onNewGroup);
    connect(m_btnDelete, &QPushButton::clicked, this, &FilterEditDialog::onDeleteGroup);
    connect(m_btnRename, &QPushButton::clicked, this, &FilterEditDialog::onRenameGroup);
    connect(m_btnAddCond, &QPushButton::clicked, this, &FilterEditDialog::onAddCondition);
    connect(m_btnRemoveCond, &QPushButton::clicked, this, [this]() {
        int row = m_condTable->currentRow();
        if (row >= 0) onRemoveCondition(row);
    });
    connect(btnOk, &QPushButton::clicked, this, &FilterEditDialog::onAccept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    refreshGroupList();
    if (m_groupList->count() > 0)
        m_groupList->setCurrentRow(0);
}

void FilterEditDialog::refreshGroupList() {
    m_groupList->clear();
    for (const auto& fs : m_filterSets)
        m_groupList->addItem(fs.name.isEmpty() ? QString::fromUtf8("\xe6\x9c\xaa\xe5\x91\xbd\xe5\x90\x8d") : fs.name);
}

void FilterEditDialog::refreshConditionTable() {
    m_condTable->setRowCount(0);
    if (m_currentGroup < 0 || m_currentGroup >= m_filterSets.size()) return;

    const auto& conds = m_filterSets[m_currentGroup].conditions;
    m_condTable->setRowCount(conds.size());
    QStringList ops = {QString::fromUtf8("\xe7\xad\x89\xe4\xba\x8e"), QString::fromUtf8("\xe4\xb8\x8d\xe7\xad\x89\xe4\xba\x8e"),
                       QString::fromUtf8("\xe5\x8c\x85\xe5\x90\xab"), QString::fromUtf8("\xe4\xb8\x8d\xe5\x8c\x85\xe5\x90\xab")};
    static const QStringList opVals = {"eq", "ne", "in", "notin"};

    for (int i = 0; i < conds.size(); i++) {
        // 属性名：可编辑下拉框
        auto* keyCombo = new QComboBox;
        keyCombo->setEditable(true);
        keyCombo->addItems(m_propertyKeys);
        if (!conds[i].key.isEmpty()) {
            int idx = keyCombo->findText(conds[i].key);
            if (idx >= 0) keyCombo->setCurrentIndex(idx);
            else keyCombo->setCurrentText(conds[i].key);
        }
        m_condTable->setCellWidget(i, 0, keyCombo);

        // 匹配方式
        auto* opCombo = new QComboBox;
        opCombo->addItems(ops);
        int opIdx = opVals.indexOf(conds[i].op);
        if (opIdx < 0) opIdx = 2;
        opCombo->setCurrentIndex(opIdx);
        m_condTable->setCellWidget(i, 1, opCombo);

        // 值：可编辑下拉框，选项取决于属性名
        auto* valCombo = new QComboBox;
        valCombo->setEditable(true);
        auto updateValues = [this, keyCombo, valCombo]() {
            QString key = keyCombo->currentText();
            valCombo->clear();
            if (m_propertyValues.contains(key))
                valCombo->addItems(m_propertyValues[key]);
        };
        updateValues();
        connect(keyCombo, &QComboBox::currentTextChanged, this, updateValues);
        if (!conds[i].value.isEmpty())
            valCombo->setCurrentText(conds[i].value);
        m_condTable->setCellWidget(i, 2, valCombo);

        // 禁用 ComboBox 滚轮
        keyCombo->setFocusPolicy(Qt::StrongFocus);
        keyCombo->installEventFilter(this);
        opCombo->setFocusPolicy(Qt::StrongFocus);
        opCombo->installEventFilter(this);
        valCombo->setFocusPolicy(Qt::StrongFocus);
        valCombo->installEventFilter(this);
    }
    m_condTable->resizeRowsToContents();
}

void FilterEditDialog::onGroupSelected(int row) {
    // 先保存当前组的编辑状态
    flushCurrentGroup();
    if (row < 0 || row >= m_filterSets.size()) return;
    m_currentGroup = row;
    refreshConditionTable();
    // 恢复 AND/OR 模式
    if (m_modeCombo) m_modeCombo->setCurrentIndex(m_filterSets[row].mode == "or" ? 1 : 0);
}

void FilterEditDialog::onNewGroup() {
    bool ok;
    QString name = QInputDialog::getText(this, QString::fromUtf8("\xe6\x96\xb0\xe5\xbb\xba\xe7\xad\x9b\xe9\x80\x89\xe7\xbb\x84"),
        QString::fromUtf8("\xe5\x90\x8d\xe7\xa7\xb0:"), QLineEdit::Normal,
        QString::fromUtf8("\xe7\xad\x9b\xe9\x80\x89\xe6\x9d\xa1\xe4\xbb\xb6 %1").arg(m_filterSets.size() + 1), &ok);
    if (!ok || name.isEmpty()) return;
    FilterSet fs; fs.name = name;
    m_filterSets.append(fs);
    refreshGroupList();
    m_groupList->setCurrentRow(m_groupList->count() - 1);
}

void FilterEditDialog::onDeleteGroup() {
    int row = m_groupList->currentRow();
    if (row < 0) return;
    if (QMessageBox::question(this, QString::fromUtf8("\xe7\xa1\xae\xe8\xae\xa4"),
        QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4\xe7\xad\x9b\xe9\x80\x89\xe7\xbb\x84 \"%1\"?").arg(m_filterSets[row].name)) != QMessageBox::Yes)
        return;
    m_filterSets.remove(row);
    refreshGroupList();
    if (m_groupList->count() > 0)
        m_groupList->setCurrentRow(0);
    else
        refreshConditionTable();
}

void FilterEditDialog::onRenameGroup() {
    int row = m_groupList->currentRow();
    if (row < 0) return;
    bool ok;
    QString name = QInputDialog::getText(this, QString::fromUtf8("\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d"),
        QString::fromUtf8("\xe5\x90\x8d\xe7\xa7\xb0:"), QLineEdit::Normal, m_filterSets[row].name, &ok);
    if (!ok || name.isEmpty()) return;
    m_filterSets[row].name = name;
    refreshGroupList();
    m_groupList->setCurrentRow(row);
}

void FilterEditDialog::onAddCondition() {
    if (m_currentGroup < 0 || m_currentGroup >= m_filterSets.size()) {
        // 没有选中组时自动新建
        FilterSet fs; fs.name = QString::fromUtf8("\xe7\xad\x9b\xe9\x80\x89\xe6\x9d\xa1\xe4\xbb\xb6 %1").arg(m_filterSets.size() + 1);
        m_filterSets.append(fs);
        refreshGroupList();
        m_groupList->setCurrentRow(m_groupList->count() - 1);
        // setCurrentRow 后 m_currentGroup 已更新
    }
    if (m_currentGroup < 0 || m_currentGroup >= m_filterSets.size()) return;
    FilterCondition c; c.op = "in";
    m_filterSets[m_currentGroup].conditions.append(c);
    refreshConditionTable();
}

void FilterEditDialog::onRemoveCondition(int row) {
    if (m_currentGroup < 0 || m_currentGroup >= m_filterSets.size()) return;
    auto& conds = m_filterSets[m_currentGroup].conditions;
    if (row < 0 || row >= conds.size()) return;
    conds.remove(row);
    refreshConditionTable();
}

void FilterEditDialog::onAccept() {
    flushCurrentGroup();
    accept();
}

void FilterEditDialog::flushCurrentGroup() {
    if (m_currentGroup < 0 || m_currentGroup >= m_filterSets.size()) return;
    auto& fs = m_filterSets[m_currentGroup];
    fs.mode = m_modeCombo && m_modeCombo->currentIndex() == 1 ? "or" : "and";
    auto& conds = fs.conditions;
    conds.clear();
    static const QStringList opVals = {"eq", "ne", "in", "notin"};
    for (int i = 0; i < m_condTable->rowCount(); i++) {
        auto* keyW = qobject_cast<QComboBox*>(m_condTable->cellWidget(i, 0));
        auto* opW  = qobject_cast<QComboBox*>(m_condTable->cellWidget(i, 1));
        auto* valW = qobject_cast<QComboBox*>(m_condTable->cellWidget(i, 2));
        if (!keyW || !opW || !valW) continue;
        QString key = keyW->currentText().trimmed();
        if (key.isEmpty()) continue;
        FilterCondition c;
        c.key = key;
        c.value = valW->currentText().trimmed();
        int opIdx = opW->currentIndex();
        c.op = (opIdx >= 0 && opIdx < opVals.size()) ? opVals[opIdx] : "in";
        conds.append(c);
    }
}

bool FilterEditDialog::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::Wheel) {
        auto* combo = qobject_cast<QComboBox*>(obj);
        if (combo && !combo->hasFocus()) return true;
    }
    return QDialog::eventFilter(obj, ev);
}
