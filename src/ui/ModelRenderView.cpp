#include "ModelRenderView.h"

#include <QHeaderView>
#include <QScrollBar>

// ── 颜色 ──
static const QColor COLOR_PASS(0x4C, 0xAF, 0x50);   // 绿色
static const QColor COLOR_FAIL(0xF4, 0x43, 0x36);    // 红色
static const QColor COLOR_SKIP(0xFF, 0x98, 0x00);     // 橙色
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
        "color: #999; font-size: 13px; padding: 20px; "
        "background: #fafafa; border: 1px solid #e0e0e0; border-radius: 6px;");
    m_stack->addWidget(m_placeholder);  // page 0

    // Page 1: actual content
    m_content = new QWidget(m_stack);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(4, 4, 4, 4);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    m_btnCollapsePanel = new QPushButton("\u25BC", m_content);  // ▼
    m_btnCollapsePanel->setFixedSize(20, 22);
    m_btnCollapsePanel->setToolTip("Collapse panel");
    m_btnCollapsePanel->setStyleSheet(
        "QPushButton { background:#e0e0e0; border:none; border-radius:3px; font-weight:bold; }"
        "QPushButton:hover { background:#ccc; }");
    connect(m_btnCollapsePanel, &QPushButton::clicked,
            this, &ModelRenderView::collapseRequested);

    m_searchEdit = new QLineEdit(m_content);
    m_searchEdit->setPlaceholderText("Search model data...");
    m_searchEdit->setStyleSheet("padding:2px 4px; font-size:12px;");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ModelRenderView::onSearchChanged);
    m_filterCombo = new QComboBox(m_content);
    m_filterCombo->addItems({"All", "Passed", "Failed", "Skipped"});
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModelRenderView::onFilterChanged);
    m_btnExpand = new QPushButton("Expand", m_content);
    connect(m_btnExpand, &QPushButton::clicked, this, &ModelRenderView::onExpandAll);
    m_btnCollapse = new QPushButton("Fold", m_content);
    connect(m_btnCollapse, &QPushButton::clicked, this, &ModelRenderView::onCollapseAll);
    m_lblStats = new QLabel("", m_content);
    m_lblStats->setStyleSheet("color:#666; font-size:11px;");
    toolbar->addWidget(m_btnCollapsePanel);
    toolbar->addWidget(m_searchEdit, 2);
    toolbar->addWidget(m_filterCombo);
    toolbar->addWidget(m_btnExpand);
    toolbar->addWidget(m_btnCollapse);
    toolbar->addWidget(m_lblStats);
    contentLayout->addLayout(toolbar);

    // Splitter: result tree + detail panel
    m_splitter = new QSplitter(Qt::Vertical, m_content);
    m_tree = new QTreeWidget(m_splitter);
    m_tree->setHeaderLabels({"", "Node", "Value"});
    m_tree->setColumnWidth(0, 24);
    m_tree->setColumnWidth(1, 280);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setStyleSheet(
        "QTreeWidget { font-size: 12px; border:1px solid #ddd; }"
        "QTreeWidget::item { padding: 2px; }"
        "QTreeWidget::item:selected { background:#e3f2fd; color:#333; }");
    connect(m_tree, &QTreeWidget::itemClicked, this, &ModelRenderView::onTreeItemClicked);

    m_detailPanel = new QTextBrowser(m_splitter);
    m_detailPanel->setOpenExternalLinks(false);
    m_detailPanel->setMinimumHeight(80);
    m_detailPanel->setStyleSheet(
        "QTextBrowser { background:#fafafa; border:1px solid #ddd; "
        "font-family: Consolas; font-size: 11px; padding: 6px; }");

    m_splitter->addWidget(m_tree);
    m_splitter->addWidget(m_detailPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    contentLayout->addWidget(m_splitter, 1);

    m_stack->addWidget(m_content);  // page 1
    m_stack->setCurrentIndex(0);
    m_layout->addWidget(m_stack, 1);
}

void ModelRenderView::showResults(const QVector<TestRunResult>& results) {
    m_results = results;
    m_resultMap.clear();
    for (auto& r : m_results)
        m_resultMap[r.testCase.fullName()] = &r;

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
    if (!results.isEmpty()) {
        updateDetailPanel(&m_results[0]);
    }
}

void ModelRenderView::clear() {
    m_tree->clear();
    m_detailPanel->clear();
    m_results.clear();
    m_resultMap.clear();
    m_lblStats->setText("");
    m_searchEdit->clear();
}

void ModelRenderView::buildResultTree(const QVector<TestRunResult>& results) {
    m_tree->clear();

    for (const auto& result : results) {
        // ── 顶层：测试用例 ──
        auto* caseItem = new QTreeWidgetItem(m_tree);
        caseItem->setText(0, "");

        // 状态图标
        QString icon = result.status == "PASSED" ? "✅" :
                       result.status == "FAILED" ? "❌" : "⏭";
        caseItem->setText(1, QString("%1 %2").arg(icon, result.testCase.fullName()));
        caseItem->setText(2, QString("%1 ms").arg(result.durationMs, 0, 'f', 1));
        caseItem->setData(1, Qt::UserRole, result.testCase.fullName());

        // 颜色
        QColor statusColor = result.status == "PASSED" ? COLOR_PASS :
                             result.status == "FAILED" ? COLOR_FAIL : COLOR_SKIP;
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
            item->setText(2, child.value);
            QColor c = child.value == "PASSED" ? COLOR_PASS :
                       child.value == "FAILED" ? COLOR_FAIL : COLOR_SKIP;
            item->setForeground(1, c);
            item->setForeground(2, c);
            break;
        }
        case NodeType::KeyValue:
            item->setText(1, child.name);
            item->setText(2, child.value);
            item->setForeground(1, COLOR_NORMAL);
            break;
        case NodeType::Array:
            item->setText(1, QString("📋 %1").arg(child.name));
            item->setText(2, QString("[%1 项]").arg(child.children.size()));
            item->setForeground(1, COLOR_NORMAL);
            for (const auto& elem : child.children) {
                auto* elemItem = new QTreeWidgetItem(item);
                elemItem->setText(1, elem.name);
                elemItem->setForeground(1, COLOR_NORMAL);
            }
            break;
        case NodeType::Scalar:
        default:
            item->setText(1, child.name);
            item->setText(2, child.value);
            break;
        }

        // 递归子节点
        if (!child.children.isEmpty() && child.type != NodeType::Array) {
            addNodeToTree(item, child);
        }

        // 搜索高亮
        if (child.matchScore > 0) {
            for (int c = 0; c < 3; c++)
                item->setBackground(c, COLOR_HIGHLIGHT);
        }
    }
}

void ModelRenderView::onSearchChanged(const QString& text) {
    // 遍历所有节点，高亮匹配
    auto allItems = m_tree->findItems("", Qt::MatchContains | Qt::MatchRecursive);
    for (auto* item : allItems) {
        bool match = text.isEmpty() ||
                     item->text(1).contains(text, Qt::CaseInsensitive) ||
                     item->text(2).contains(text, Qt::CaseInsensitive);
        for (int c = 0; c < 3; c++) {
            item->setBackground(c, match && !text.isEmpty() ? COLOR_HIGHLIGHT : QColor());
        }
        if (!text.isEmpty() && match) {
            item->setExpanded(true);
        }
    }
}

void ModelRenderView::onFilterChanged(int index) {
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
        auto* item = m_tree->topLevelItem(i);
        if (index == 0) {
            item->setHidden(false);
        } else {
            QString status = item->text(1);
            bool show = (index == 1 && status.contains("✅")) ||
                        (index == 2 && status.contains("❌")) ||
                        (index == 3 && status.contains("⏭"));
            item->setHidden(!show);
        }
    }
}

void ModelRenderView::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item) return;

    // 如果是用例级别的节点，显示详情
    QString fullName = item->data(1, Qt::UserRole).toString();
    if (!fullName.isEmpty() && m_resultMap.contains(fullName)) {
        updateDetailPanel(m_resultMap[fullName]);
        emit resultSelected(*m_resultMap[fullName]);
    }
}

void ModelRenderView::updateDetailPanel(const TestRunResult* result) {
    if (!result) {
        m_detailPanel->setHtml("<p style='color:#999'>选择测试用例查看详情</p>");
        return;
    }

    QString statusColor = result->status == "PASSED" ? "#4CAF50" :
                          result->status == "FAILED" ? "#F44336" : "#FF9800";
    QString statusIcon = result->status == "PASSED" ? "✅" :
                         result->status == "FAILED" ? "❌" : "⏭";

    QString html;
    html += QString("<h2 style='margin:0'>%1 %2</h2>")
                .arg(statusIcon, result->testCase.fullName());
    html += QString("<p><b>状态:</b> <span style='color:%1'>%2</span> | "
                    "<b>耗时:</b> %3 ms | "
                    "<b>套件:</b> %4</p>")
                .arg(statusColor, result->status)
                .arg(result->durationMs, 0, 'f', 1)
                .arg(result->testCase.suiteName);

    // ── 模型属性（优先展示）──
    if (!result->properties.isEmpty()) {
        html += "<hr/><h3>📋 模型属性</h3>";
        html += "<table style='border-collapse:collapse; width:100%; font-size:13px;'>";
        // 优先字段排前面
        QStringList priority = {"interface", "model", "resultModel"};
        for (const auto& pk : priority) {
            if (result->properties.contains(pk)) {
                QString val = result->properties[pk].toHtmlEscaped();
                html += QString(
                    "<tr><td style='padding:4px 8px; font-weight:bold; "
                    "background:#f0f0f0; border:1px solid #ddd; width:140px;'>%1</td>"
                    "<td style='padding:4px 8px; border:1px solid #ddd;'>%2</td></tr>")
                    .arg(pk, val);
            }
        }
        // 其余字段
        for (auto it = result->properties.begin(); it != result->properties.end(); ++it) {
            if (priority.contains(it.key())) continue;
            html += QString(
                "<tr><td style='padding:4px 8px; font-weight:bold; "
                "background:#f0f0f0; border:1px solid #ddd;'>%1</td>"
                "<td style='padding:4px 8px; border:1px solid #ddd;'>%2</td></tr>")
                .arg(it.key().toHtmlEscaped(), it.value().toHtmlEscaped());
        }
        html += "</table>";
    }

    html += "<hr/><h3>📄 原始输出</h3>";
    html += "<pre style='background:#f5f5f5; padding:8px; border-radius:4px; "
            "max-height:300px; overflow:auto; font-size:11px; white-space:pre-wrap;'>";
    html += result->rawStdout.toHtmlEscaped().left(5000);
    html += "</pre>";

    if (!result->rawStderr.isEmpty()) {
        html += "<hr/><h3>⚠ 错误输出</h3>";
        html += "<pre style='background:#ffebee; padding:8px; border-radius:4px; "
                "color:#c62828; font-size:11px; white-space:pre-wrap;'>";
        html += result->rawStderr.toHtmlEscaped();
        html += "</pre>";
    }

    m_detailPanel->setHtml(html);
}

void ModelRenderView::onExpandAll() {
    m_tree->expandAll();
}

void ModelRenderView::onCollapseAll() {
    m_tree->collapseAll();
}
