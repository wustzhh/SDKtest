#include "ModelInfoPanel.h"

#include <QHBoxLayout>

ModelInfoPanel::ModelInfoPanel(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(2);

    // ── 折叠切换按钮（标题栏）──
    auto* header = new QWidget(this);
    auto* hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(0, 0, 0, 0);

    m_btnToggle = new QPushButton("▼  Model Info", this);
    m_btnToggle->setStyleSheet(
        "QPushButton { background:#e8e8e8; border:none; border-radius:3px; "
        "padding:4px 8px; font-weight:bold; text-align:left; }"
        "QPushButton:hover { background:#d0d0d0; }");
    m_btnToggle->setFixedHeight(26);
    connect(m_btnToggle, &QPushButton::clicked, this, &ModelInfoPanel::onToggleCollapse);

    hdrLayout->addWidget(m_btnToggle, 1);
    m_layout->addWidget(header);

    // ── 可折叠内容区 ──
    m_content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(8, 4, 8, 4);
    contentLayout->setSpacing(3);

    auto makeRow = [&](const QString& title) -> QLabel* {
        auto* row = new QWidget(m_content);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(title, m_content);
        lbl->setStyleSheet("font-weight:bold; color:#555; font-size:12px; min-width:80px;");
        auto* val = new QLabel("—", m_content);
        val->setStyleSheet("font-size:12px; color:#333;");
        val->setWordWrap(true);
        rowLay->addWidget(lbl);
        rowLay->addWidget(val, 1);
        contentLayout->addWidget(row);
        return val;
    };

    m_lblInterface = makeRow("Interface:");
    m_lblModelIn   = makeRow("Model In:");
    m_lblModelOut  = makeRow("Model Out:");
    m_lblExtra     = makeRow("Extra:");
    m_lblExtra->setMaximumHeight(60);

    m_content->setStyleSheet("background:#fafafa; border:1px solid #e0e0e0; border-radius:3px;");
    m_layout->addWidget(m_content);

    clear();
}

void ModelInfoPanel::showModelInfo(const TestRunResult* result) {
    if (!result || result->properties.isEmpty()) {
        clear();
        return;
    }

    const auto& props = result->properties;

    // Interface
    QString iface = props.value("interface");
    m_lblInterface->setText(iface.isEmpty() ? "—" : iface);

    // Model In
    setPathLabel(m_lblModelIn, "Input Model", props.value("model"));

    // Model Out
    setPathLabel(m_lblModelOut, "Output Model", props.value("resultModel"));

    // Extra (除 interface/model/resultModel 之外的)
    QStringList extras;
    for (auto it = props.begin(); it != props.end(); ++it) {
        if (it.key() != "interface" && it.key() != "model" && it.key() != "resultModel") {
            extras << QString("%1=%2").arg(it.key(), it.value());
        }
    }
    m_lblExtra->setText(extras.isEmpty() ? "—" : extras.join("  |  "));

    if (m_collapsed) onToggleCollapse();  // 有数据时自动展开
}

void ModelInfoPanel::clear() {
    m_lblInterface->setText("—");
    m_lblModelIn->setText("—");
    m_lblModelOut->setText("—");
    m_lblExtra->setText("—");
}

void ModelInfoPanel::setPathLabel(QLabel* label, const QString& title, const QString& path) {
    if (path.isEmpty()) {
        label->setText("—");
        return;
    }

    QFileInfo fi(path);
    bool exists = fi.exists();
    QString icon = exists ? "✓" : "✗";
    QString color = exists ? "#2e7d32" : "#c62828";
    QString sizeStr = exists ? QString("  [%1 KB]").arg(fi.size() / 1024) : "";

    label->setText(
        QString("<span style='color:%1'>%2</span>  %3%4")
            .arg(color, icon, path.toHtmlEscaped(), sizeStr));
    label->setToolTip(exists
        ? QString("Path: %1\nSize: %2 bytes\nModified: %3")
              .arg(fi.absoluteFilePath())
              .arg(fi.size())
              .arg(fi.lastModified().toString("yyyy-MM-dd hh:mm:ss"))
        : "File does not exist");
}

void ModelInfoPanel::onToggleCollapse() {
    m_collapsed = !m_collapsed;
    if (m_collapsed) {
        // 完全隐藏（高度归零）
        m_content->setVisible(false);
        setMaximumHeight(24);  // 只剩标题栏高度
        m_btnToggle->setText("▶  Model Info");
    } else {
        // 恢复显示
        m_content->setVisible(true);
        setMaximumHeight(200);
        m_btnToggle->setText("▼  Model Info");
    }
}
