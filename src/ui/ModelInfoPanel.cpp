#include "ModelInfoPanel.h"
#include <QHBoxLayout>

ModelInfoPanel::ModelInfoPanel(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(2);

    // Header
    auto* header = new QWidget(this);
    auto* hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(0, 0, 0, 0);
    m_btnToggle = new QPushButton("▼  模型信息", this);
    m_btnToggle->setStyleSheet(
        "QPushButton { background:#e8e8e8; border:none; border-radius:3px; "
        "padding:4px 8px; font-weight:bold; text-align:left; }"
        "QPushButton:hover { background:#d0d0d0; }");
    m_btnToggle->setFixedHeight(26);
    connect(m_btnToggle, &QPushButton::clicked, this, &ModelInfoPanel::onToggleCollapse);
    hdrLayout->addWidget(m_btnToggle, 1);
    m_layout->addWidget(header);

    // Content
    m_content = new QWidget(this);
    auto* cl = new QVBoxLayout(m_content);
    cl->setContentsMargins(8, 4, 8, 4);
    cl->setSpacing(3);

    m_interface = makeRow("Interface:");
    m_modelIn   = makeRow("Model In:");
    m_modelOut  = makeRow("Model Out:");

    m_lblExtra = new QLabel("—", m_content);
    m_lblExtra->setWordWrap(true);
    m_lblExtra->setMaximumHeight(60);
    m_lblExtra->setStyleSheet("font-size:11px; color:#888;");
    cl->addWidget(m_lblExtra);

    m_content->setStyleSheet("background:#fafafa; border:1px solid #e0e0e0; border-radius:3px;");
    m_layout->addWidget(m_content);
    clear();
}

ModelInfoPanel::ModelRow ModelInfoPanel::makeRow(const QString& title) {
    ModelRow row;
    row.container = new QWidget(m_content);
    auto* rl = new QHBoxLayout(row.container);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(4);

    auto* lbl = new QLabel(title, m_content);
    lbl->setStyleSheet("font-weight:bold; color:#555; font-size:12px; min-width:70px;");
    rl->addWidget(lbl);

    row.label = new QLabel("—", m_content);
    row.label->setStyleSheet("font-size:12px; color:#333;");
    row.label->setWordWrap(true);
    rl->addWidget(row.label, 1);

    row.btnOpen = new QPushButton("打开", m_content);
    row.btnOpen->setFixedSize(40, 20);
    row.btnOpen->setStyleSheet(
        "QPushButton { background:#2196F3; color:white; border:none; "
        "border-radius:3px; font-size:10px; }"
        "QPushButton:hover { background:#1976D2; }");
    row.btnOpen->setVisible(false);
    connect(row.btnOpen, &QPushButton::clicked, this, [this, row]() {
        if (row.label->toolTip().startsWith("file:"))
            emit openFileRequested(row.label->toolTip().mid(5));
    });
    rl->addWidget(row.btnOpen);

    m_content->layout()->addWidget(row.container);
    return row;
}

void ModelInfoPanel::showModelInfo(const TestRunResult* result) {
    if (!result || result->properties.isEmpty()) {
        clear();
        return;
    }
    const auto& p = result->properties;

    auto setRow = [&](ModelRow& row, const QString& key) {
        QString val = p.value(key);
        if (val.isEmpty()) {
            row.label->setText("—");
            row.btnOpen->setVisible(false);
        } else {
            QFileInfo fi(val);
            bool isFile = fi.exists() || val.contains('/') || val.contains('\\');
            if (isFile) {
                bool exists = fi.exists();
                QString icon = exists ? "✓" : "✗";
                QString color = exists ? "#2e7d32" : "#c62828";
                row.label->setText(QString("<span style='color:%1'>%2</span> %3")
                    .arg(color, icon, val.toHtmlEscaped()));
                row.label->setToolTip("file:" + val);
                row.btnOpen->setVisible(exists);
            } else {
                row.label->setText(val.toHtmlEscaped());
                row.label->setToolTip("");
                row.btnOpen->setVisible(false);
            }
        }
    };

    setRow(m_interface, "interface");
    setRow(m_modelIn,   "model");
    setRow(m_modelOut,  "resultModel");

    // Extra
    QStringList extras;
    for (auto it = p.begin(); it != p.end(); ++it) {
        if (it.key() != "interface" && it.key() != "model" && it.key() != "resultModel")
            extras << it.key() + "=" + it.value();
    }
    m_lblExtra->setText(extras.isEmpty() ? "—" : extras.join("  |  "));

    if (m_collapsed) onToggleCollapse();
}

void ModelInfoPanel::clear() {
    m_interface.label->setText("—"); m_interface.btnOpen->setVisible(false);
    m_modelIn.label->setText("—");   m_modelIn.btnOpen->setVisible(false);
    m_modelOut.label->setText("—");  m_modelOut.btnOpen->setVisible(false);
    m_lblExtra->setText("—");
}

void ModelInfoPanel::onToggleCollapse() {
    m_collapsed = !m_collapsed;
    m_content->setVisible(!m_collapsed);
    if (m_collapsed) {
        setMaximumHeight(26);
        m_btnToggle->setText("▶  模型信息");
    } else {
        setMaximumHeight(200);
        m_btnToggle->setText("▼  模型信息");
    }
}
