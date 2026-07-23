#include "ModelRenderView.h"

#include <QHeaderView>
#include <QScrollBar>
#include <QFileInfo>
#include <QDialog>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QApplication>
#include <QClipboard>
#include <QProcess>


// ── 颜色 ──
static const QColor COLOR_PASS(0x4C, 0xAF, 0x50);   // 绿色
static const QColor COLOR_FAIL(0xF4, 0x43, 0x36);    // 红色
static const QColor COLOR_SKIP(0xFF, 0x98, 0x00);     // 橙色
static const QColor COLOR_CRASHED(0x9C, 0x27, 0xB0);  // 紫色（崩溃）
static const QColor COLOR_HIGHLIGHT(0xFF, 0xFF, 0x99); // 黄色高亮
static const QColor COLOR_SECTION(0x21, 0x96, 0xF3);  // 蓝色标题
static const QColor COLOR_NORMAL(0x33, 0x33, 0x33);    // 深灰正文

ModelRenderView::ModelRenderView(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);

    // Page 0: placeholder
    m_placeholder = new QLabel(m_stack);
    m_placeholder->setText(
        "\n\n  === Model Render Area ===\n\n"
        "Run a test, then click a result\n"
        "to see model data here.\n\n"
        "The model tree shows:\n"
        "  - Topology info (bodies/faces/edges)\n"
        "  - Feature recognition results\n"
        "  - Timing breakdown\n"
        "  - Custom [PROP] attributes\n\n");
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setStyleSheet(
        "color: #5a6278; font-size: 13px; padding: 20px; "
        "background: #16181e; border: 1px solid #2a2d38; border-radius: 6px;");
    m_stack->addWidget(m_placeholder);  // page 0

    // Page 1: actual content
    m_content = new QWidget(m_stack);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(4, 4, 4, 4);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    m_btnCollapsePanel = new QPushButton("\u25BC", m_content);  // ▼
    m_btnCollapsePanel->setFixedSize(20, 22);
    m_btnCollapsePanel->setToolTip("隐藏右侧面板");
    m_btnCollapsePanel->setStyleSheet(
        "QPushButton { background:#f1f5f9; border:none; border-radius:6px; font-weight:bold; color:#64748b; }"
        "QPushButton:hover { background:#e2e8f0; color:#1e293b; }");
    connect(m_btnCollapsePanel, &QPushButton::clicked,
            this, &ModelRenderView::collapseRequested);

    m_searchEdit = new QLineEdit(m_content);
    m_searchEdit->setPlaceholderText("搜索模型数据...");
    m_searchEdit->setStyleSheet("padding:2px 4px; font-size:12px;");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ModelRenderView::onSearchChanged);
    QString tbBtn = "QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;"
                    "padding:3px 10px;font-size:12px}QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}";
    m_btnExpand = new QPushButton("展开", m_content);
    m_btnExpand->setFixedHeight(28);m_btnExpand->setStyleSheet(tbBtn);
    connect(m_btnExpand, &QPushButton::clicked, this, &ModelRenderView::onExpandAll);
    m_btnCollapse = new QPushButton("折叠", m_content);
    m_btnCollapse->setFixedHeight(28);m_btnCollapse->setStyleSheet(tbBtn);
    connect(m_btnCollapse, &QPushButton::clicked, this, &ModelRenderView::onCollapseAll);
    m_btnLocate = new QPushButton("\xE2\x97\x8A", m_content);  // ◊
    m_btnLocate->setFixedSize(28,28);
    m_btnLocate->setStyleSheet(
        "QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;font-size:16px;padding:0;}"
        "QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}");
    m_btnLocate->setToolTip("定位到当前选中结果");
    connect(m_btnLocate, &QPushButton::clicked, this, [this]() {
        if (m_lastHighlighted)
            m_tree->scrollToItem(m_lastHighlighted, QAbstractItemView::EnsureVisible);
    });
    m_lblStats = new QLabel("", m_content);
    m_lblStats->setStyleSheet("color:#8892a6; font-size:11px;");
    toolbar->addWidget(m_btnCollapsePanel);
    toolbar->addWidget(m_searchEdit, 2);
    toolbar->addWidget(m_btnExpand);
    toolbar->addWidget(m_btnCollapse);
    toolbar->addWidget(m_btnLocate);
    toolbar->addWidget(m_lblStats);
    contentLayout->addLayout(toolbar);

    // Result tree
    m_tree = new QTreeWidget(m_content);
    m_tree->setHeaderLabels({"", "Node"});
    m_tree->setMinimumHeight(100);
    m_tree->setColumnWidth(0, 24);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setAlternatingRowColors(true);
    m_tree->setWordWrap(true);
    m_tree->setTextElideMode(Qt::ElideNone);
    m_tree->setStyleSheet(
        "QTreeWidget { font-size:13px; border:1px solid #e2e8f0; border-radius:6px; background:#ffffff; }"
        "QTreeWidget::item { padding:6px 10px; min-height:28px; border-bottom:1px solid #f1f5f9; }"
        "QTreeWidget::item:selected { background:#eef2ff; color:#1e293b; }"
        "QTreeWidget::item:hover { background:#f8f9fb; }"
        "QTreeWidget{outline:none;}");
    connect(m_tree, &QTreeWidget::itemClicked, this, &ModelRenderView::onTreeItemClicked);
    connect(m_btnLocate, &QPushButton::clicked, this, [this]() {
        if (m_lastHighlighted)
            m_tree->scrollToItem(m_lastHighlighted, QAbstractItemView::EnsureVisible);
    });

    // Splitter: result tree + property tree
    m_bottomSplit = new QSplitter(Qt::Vertical, m_content);
    m_bottomSplit->setHandleWidth(5);
    m_bottomSplit->setStyleSheet("QSplitter::handle{background:rgba(108,92,231,0.15);margin:2px 0}");
    m_bottomSplit->addWidget(m_tree);

    // Property tree
    m_propTree = new QTreeWidget(m_bottomSplit);
    m_propTree->setHeaderLabels({"Key", "Value", ""});
    m_propTree->setColumnWidth(0, 110);
    m_propTree->setColumnWidth(2, 56);
    m_propTree->header()->setStretchLastSection(false);
    m_propTree->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_propTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_propTree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_propTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_propTree->setWordWrap(true);
    m_propTree->setMinimumHeight(40);
    m_propTree->setRootIsDecorated(false);
    m_propTree->setAlternatingRowColors(true);
    m_propTree->setStyleSheet(
        "QTreeWidget { background:#ffffff; border-radius:6px; border:1px solid #e2e8f0; "
        "font-size:13px; } "
        "QTreeWidget::item { padding:8px 12px; min-height:34px; color:#1e293b; } "
        "QTreeWidget::item:selected { background:#eef2ff; color:#1e293b; } "
        "QHeaderView::section { background:#f8f9fb; color:#6366f1; padding:6px 12px; "
        "font-size:12px; font-weight:600; border:none; border-bottom:1px solid #e2e8f0; }");
    m_bottomSplit->setStretchFactor(0, 4);
    m_bottomSplit->setStretchFactor(1, 1);
    m_bottomSplit->setSizes({400, 80});
    // 属性树右键菜单
    m_propTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_propTree, &QTreeWidget::customContextMenuRequested,
            this, &ModelRenderView::onPropTreeContextMenu);
    contentLayout->addWidget(m_bottomSplit, 1);

    m_stack->addWidget(m_content);  // page 1
    m_stack->setCurrentIndex(1);  // 默认显示内容页
    m_layout->addWidget(m_stack, 1);
}

void ModelRenderView::showResults(const QVector<TestRunResult>& results) {
    m_results = results;
    m_resultMap.clear();
    for (auto& r : m_results)
        m_resultMap[r.testCase.fullName()] = &r;

    m_stack->setCurrentIndex(1);  // 切换到内容页
    buildResultTree(results);

    int passed = 0, failed = 0, skipped = 0;
    for (const auto& r : results) {
        if (r.status == "PASSED") passed++;
        else if (r.status == "FAILED") failed++;
        else skipped++;
    }
    m_lblStats->setText(
        QString("✅ %1  ❌ %2  ⏭ %3  | 共 %4")
            .arg(passed).arg(failed).arg(skipped).arg(results.size()));
    m_lblStats->setStyleSheet(
        QString("font-size: 12px; font-weight: bold; padding: 4px; "
                "color: %1;")
            .arg(failed > 0 ? "#f44336" : "#4CAF50"));

    m_tree->expandAll();
}

void ModelRenderView::clear() {
    m_tree->clear();
    m_propTree->clear();
    m_results.clear();
    m_resultMap.clear();
    m_lastHighlighted = nullptr;
    m_lblStats->setText("");
    m_searchEdit->clear();  // 留在内容页，只是清空数据
}

void ModelRenderView::buildResultTree(const QVector<TestRunResult>& results) {
    m_lastHighlighted = nullptr;
    m_tree->clear();

    for (const auto& result : results) {
        // ── 顶层：测试用例 ──
        auto* caseItem = new QTreeWidgetItem(m_tree);
        caseItem->setText(0, "");

        // 状态图标
        QString icon = result.status == "PASSED" ? "✅" :
                       result.status == "FAILED" ? "❌" :
                       result.status == "CRASHED" ? "💥" : "⏭";
        caseItem->setText(1, QString("%1 %2").arg(icon, result.testCase.fullName()));
        caseItem->setToolTip(1, QString("%1 ms").arg(result.durationMs, 0, 'f', 1));
        caseItem->setData(1, Qt::UserRole, result.testCase.fullName());

        // 颜色
        QColor statusColor = COLOR_NORMAL;
        caseItem->setForeground(1, statusColor);
        QFont f = caseItem->font(1);
        f.setBold(true);
        caseItem->setFont(1, f);

        // ── 子节点：模型树 ──
        addNodeToTree(caseItem, result.modelTree);
    }
}

void ModelRenderView::addNodeToTree(QTreeWidgetItem* parent, const ResultNode& node) {
    for (const auto& child : node.children) {
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, "");

        switch (child.type) {
        case NodeType::Section:
            item->setText(1, child.name);
            item->setForeground(1, COLOR_SECTION);
            { QFont f = item->font(1); f.setBold(true); item->setFont(1, f); }
            break;
        case NodeType::Status: {
            QString icon = child.value == "PASSED" ? "✅" :
                           child.value == "FAILED" ? "❌" : "⏭";
            item->setText(1, icon + " " + child.name);
            QColor c = child.value == "PASSED" ? COLOR_PASS :
                       child.value == "FAILED" ? COLOR_FAIL : COLOR_SKIP;
            item->setForeground(1, c);
            break;
        }
        case NodeType::KeyValue:
            item->setText(1, child.name + ": " + child.value);
            item->setForeground(1, COLOR_NORMAL);
            break;
        case NodeType::Array:
            item->setText(1, QString("📋 %1  [%2 项]").arg(child.name).arg(child.children.size()));
            item->setForeground(1, COLOR_NORMAL);
            for (const auto& elem : child.children) {
                auto* elemItem = new QTreeWidgetItem(item);
                elemItem->setText(1, elem.name);
                elemItem->setForeground(1, COLOR_NORMAL);
            }
            break;
        case NodeType::Scalar:
        default:
            item->setText(1, child.value.isEmpty() ? child.name : child.name + ": " + child.value);
            item->setForeground(1, COLOR_NORMAL);
            if (child.name == "stdout" && child.rawData.isValid())
                item->setToolTip(1, child.rawData.toString());
            break;
        }

        // 递归子节点
        if (!child.children.isEmpty() && child.type != NodeType::Array) {
            addNodeToTree(item, child);
        }

        // 搜索高亮
        if (child.matchScore > 0) {
            for (int c = 0; c < 2; c++)
                item->setBackground(c, COLOR_HIGHLIGHT);
        }
    }
}

void ModelRenderView::onSearchChanged(const QString& text) {
    // 遍历所有项，匹配的高亮+展开，不匹配的隐藏
    std::function<bool(QTreeWidgetItem*)> filter;
    filter = [&](QTreeWidgetItem* item) -> bool {
        bool selfMatch = text.isEmpty() ||
                         item->text(1).contains(text, Qt::CaseInsensitive) ||
                         item->toolTip(1).contains(text, Qt::CaseInsensitive);
        bool childMatch = false;
        for (int i = 0; i < item->childCount(); i++) {
            if (filter(item->child(i))) childMatch = true;
        }
        bool visible = selfMatch || childMatch;
        item->setHidden(!visible);
        if (!text.isEmpty() && selfMatch) {
            item->setBackground(0, COLOR_HIGHLIGHT);
            item->setBackground(1, COLOR_HIGHLIGHT);
            // 展开父链
            QTreeWidgetItem* p = item->parent();
            while (p) { p->setExpanded(true); p = p->parent(); }
        } else {
            item->setBackground(0, QColor());
            item->setBackground(1, QColor());
        }
        return visible;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
        filter(m_tree->topLevelItem(i));
    m_tree->viewport()->update();
}

void ModelRenderView::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item) return;

    // 取消上次高亮（恢复原前景色 + 清背景）
    if (m_lastHighlighted && m_lastHighlighted != item) {
        for (int c = 0; c < 2; c++) {
            m_lastHighlighted->setBackground(c, QBrush());
            QVariant saved = m_lastHighlighted->data(c, Qt::UserRole + 2);
            if (saved.isValid()) m_lastHighlighted->setForeground(c, saved.value<QColor>());
        }
    }
    // 保存当前项原前景色，然后高亮
    for (int c = 0; c < 2; c++) {
        item->setData(c, Qt::UserRole + 2, item->foreground(c).color());
    }
    m_lastHighlighted = item;
    for (int c = 0; c < 2; c++) {
        item->setBackground(c, QColor(0xe8,0xf5,0xe9));   // 淡绿背景
        item->setForeground(c, QColor(0x4C,0xAF,0x50));   // 绿色字
    }
    QFont bf = item->font(1); bf.setBold(true); item->setFont(1, bf);
    m_tree->scrollToItem(item, QAbstractItemView::EnsureVisible);

    // 点击 stdout 节点弹出完整内容
    if (item->text(1) == "stdout" && item->toolTip(1).length() > 300) {
        showFullOutput("stdout", item->toolTip(1));
        return;
    }

    // 如果是用例级别的节点，显示详情
    QString fullName = item->data(1, Qt::UserRole).toString();
    if (!fullName.isEmpty() && m_resultMap.contains(fullName)) {
        updateDetailPanel(m_resultMap[fullName]);
        emit resultSelected(*m_resultMap[fullName]);
    }
}

void ModelRenderView::updateDetailPanel(const TestRunResult* result) {
    m_propTree->clear();
    if (!result || result->properties.isEmpty()) return;
    m_propTree->expandAll();
    QStringList priority = {"interface", "model", "resultModel"};
    for (const auto& pk : priority) {
        if (result->properties.contains(pk)) {
            auto* item = new QTreeWidgetItem(m_propTree);
            item->setText(0, pk);
            QString v = result->properties[pk];
            QFileInfo fi(v);
            bool isFilePath = fi.exists() || v.contains('/') || v.contains('\\');
            if (isFilePath) {
                item->setText(1, QString(fi.exists() ? "\xe2\x9c\x93 " : "\xe2\x9c\x97 ") + v);
                item->setForeground(1, fi.exists() ? QColor(0x2e,0x7d,0x32) : QColor(0xc6,0x28,0x28));
                if (fi.exists()) {
                    auto* btn = new QPushButton(QString::fromUtf8("\xe6\x89\x93\xe5\xbc\x80"));
                    btn->setFixedSize(40, 20);
                    btn->setStyleSheet("QPushButton{font-size:10px;padding:0 4px;border-radius:4px;background:#6c5ce7;color:white;border:none;}QPushButton:hover{background:#5a4bd1;}");
                    connect(btn, &QPushButton::clicked, this, [this, v]() { emit openModelFile(v); });
                    m_propTree->setItemWidget(item, 2, btn);
                }
            } else {
                item->setText(1, v);
                item->setToolTip(1, v);
            }
        }
    }
    bool hasResultModel = result->properties.contains("resultModel");
    for (auto it = result->properties.begin(); it != result->properties.end(); ++it) {
        if (priority.contains(it.key())) continue;
        auto* item = new QTreeWidgetItem(m_propTree);
        item->setText(0, it.key());
        QString v = it.value();

        // ── 检测包围盒格式 [minX,minY,minZ,maxX,maxY,maxZ],[...] ──
        QVector<QVector<double>> boxes;
        bool isBoxProp = false;
        // 始终显示高亮按钮
        if (v.startsWith('[')) {
            static QRegularExpression boxRe(R"(\[([^\]]+)\])");
            auto boxIt = boxRe.globalMatch(v);
            while (boxIt.hasNext()) {
                auto m = boxIt.next();
                QStringList nums = m.captured(1).split(',');
                if (nums.size() >= 6) {
                    QVector<double> box;
                    for (int i = 0; i < 6; i++) {
                        bool ok;
                        double val = nums[i].trimmed().toDouble(&ok);
                        if (!ok) { box.clear(); break; }
                        box.append(val);
                    }
                    if (box.size() == 6) boxes.append(box);
                }
            }
            isBoxProp = !boxes.isEmpty();
        }

        if (isBoxProp) {
            item->setText(1, QString::fromUtf8("\xE8\xA7\xA3\xE6\x9E\x90\xE4\xB8\xAD..."));
            item->setToolTip(1, v);
            auto* btn = new QPushButton(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"));
                btn->setFixedSize(44, 22);
                btn->setStyleSheet("QPushButton{font-size:11px;padding:0 4px;border-radius:4px;background:#6366f1;color:white;border:none;}QPushButton:hover{background:#4f46e5;}");
                btn->setCheckable(true);
                QString propKey = it.key();
                connect(btn, &QPushButton::toggled, this, [this, propKey, boxes, btn](bool on){
                    btn->setText(on?QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"):QString::fromUtf8("\xe9\x9a\x90\xe8\x97\x8f"));
                    emit toggleHighlightBoxes(propKey, boxes, on);
                });
                btn->setChecked(true);
                m_propTree->setItemWidget(item, 2, btn);
        }
        // ── 检测逗号分隔的整数数组 ──
        else {
            QStringList parts = v.split(',', Qt::SkipEmptyParts);
            bool isArray = parts.size() > 1;
            for (auto& p : parts) { p = p.trimmed(); bool ok; p.toInt(&ok); if (!ok) { isArray = false; break; } }
            if (isArray) {
                item->setText(1, v);
                item->setToolTip(1, v);
                if (it.key() == "searchResult" || it.key() == "removeResult") {
                    auto* btn = new QPushButton(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"));
                    btn->setFixedSize(40, 18);
                    btn->setStyleSheet("QPushButton{font-size:10px;padding:0 2px;border-radius:4px;background:#6c5ce7;color:white;border:none;}QPushButton:hover{background:#5a4bd1;}");
                    btn->setCheckable(true);
                    QVector<int> ids; for(const auto& p:parts){bool ok;int id=p.toInt(&ok);if(ok)ids.append(id);}
                    connect(btn, &QPushButton::toggled, this, [this, ids, btn](bool on){
                        btn->setText(on?QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"):QString::fromUtf8("\xe9\x9a\x90\xe8\x97\x8f"));
                        emit toggleHighlight(ids, on);
                    });
                    btn->setChecked(true);
                    m_propTree->setItemWidget(item, 2, btn);
                }
            } else {
                item->setText(1, v);
                item->setToolTip(1, v);
            }
        }
    }
}

void ModelRenderView::onExpandAll() {
    m_tree->expandAll();
}

void ModelRenderView::updatePropertyText(const QString& key, const QString& newText) {
    // 递归搜索所有层级的 item 匹配 key
    std::function<bool(QTreeWidgetItem*)> search = [&](QTreeWidgetItem* parent) -> bool {
        for (int i = 0; i < parent->childCount(); ++i) {
            auto* item = parent->child(i);
            if (item->text(0) == key) { item->setText(1, newText); return true; }
            if (search(item)) return true;
        }
        return false;
    };
    for (int i = 0; i < m_propTree->topLevelItemCount(); ++i) {
        auto* item = m_propTree->topLevelItem(i);
        if (item->text(0) == key) { item->setText(1, newText); return; }
        if (search(item)) return;
    }
}

void ModelRenderView::showFullOutput(const QString& title, const QString& text) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(title);
    dlg->resize(800, 600);
    auto* lay = new QVBoxLayout(dlg);
    auto* te = new QPlainTextEdit(text, dlg);
    te->setReadOnly(true);
    te->setFont(QFont("Consolas", 10));
    te->setStyleSheet("background:#1e1e1e;color:#d4d4d4;");
    lay->addWidget(te);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

int ModelRenderView::saveBottomSplitPos() const {
    auto s = m_bottomSplit->sizes();
    return s.size() >= 2 ? s[0] : 400;
}

void ModelRenderView::restoreBottomSplitPos(int pos) {
    if (pos <= 0) return;
    int total = m_bottomSplit->height();
    if (total < 50) total = pos + 80;
    int treeH = qBound(60, pos, total - 60);
    m_bottomSplit->setSizes({treeH, total - treeH});
}

void ModelRenderView::onCollapseAll() {
    m_tree->collapseAll();
}

void ModelRenderView::onPropTreeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_propTree->itemAt(pos);
    if (!item) return;

    QString value = item->text(1);
    if (value.isEmpty()) return;

    // 去掉可能的前缀图标（✓ / ✗）
    QString path = value;
    if (path.startsWith("\xe2\x9c\x93 ") || path.startsWith("\xe2\x9c\x97 "))
        path = path.mid(2);

    QMenu menu(this);
    QFileInfo fi(path);
    if (fi.exists()) {
        QAction* actOpenFolder = menu.addAction(QString::fromUtf8("\xe6\x89\x93\xe5\xbc\x80\xe6\x96\x87\xe4\xbb\xb6\xe5\xa4\xb9"));
        QAction* actOpenFile = menu.addAction(QString::fromUtf8("\xe6\x89\x93\xe5\xbc\x80\xe6\x96\x87\xe4\xbb\xb6"));
        menu.addSeparator();
    }
    QAction* actCopy = menu.addAction(QString::fromUtf8("\xe5\xa4\x8d\xe5\x88\xb6"));

    QAction* chosen = menu.exec(m_propTree->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen->text().contains(QString::fromUtf8("\xe5\xa4\x8d\xe5\x88\xb6"))) {
        QApplication::clipboard()->setText(path);
    } else if (chosen->text().contains(QString::fromUtf8("\xe6\x96\x87\xe4\xbb\xb6\xe5\xa4\xb9"))) {
        // 打开文件夹并选中文件
        QString nativePath = QDir::toNativeSeparators(fi.absoluteFilePath());
        QProcess::startDetached("explorer", {"/select,", nativePath});
    } else if (chosen->text().contains(QString::fromUtf8("\xe6\x89\x93\xe5\xbc\x80\xe6\x96\x87\xe4\xbb\xb6"))) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
    }
}
