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
        "QPushButton { background:#282a34; border:1px solid #2a2d38; border-radius:4px; "
        "padding:4px 8px; font-weight:600; text-align:left; color:#8892a6; }"
        "QPushButton:hover { background:#323540; color:#e2e8f0; }");
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
    m_lblExtra->setStyleSheet("font-size:11px; color:#5a6278;");
    cl->addWidget(m_lblExtra);

    m_content->setStyleSheet("background:#1e2028; border:1px solid #2a2d38; border-radius:6px;");
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
    lbl->setStyleSheet("font-weight:600; color:#8892a6; font-size:12px; min-width:70px;");
    rl->addWidget(lbl);

    row.label = new QLabel("—", m_content);
    row.label->setStyleSheet("font-size:12px; color:#e2e8f0;");
    row.label->setWordWrap(true);
    rl->addWidget(row.label, 1);

    row.btnOpen = new QPushButton("打开", m_content);
    row.btnOpen->setFixedSize(40, 20);
    row.btnOpen->setStyleSheet(
        "QPushButton { background:#282a34; color:#818cf8; border:1px solid #2a2d38; "
        "border-radius:4px; font-size:10px; }"
        "QPushButton:hover { background:#323540; border-color:#818cf8; }");
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
