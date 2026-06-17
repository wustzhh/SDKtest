#include "Model3DViewer.h"
#include <QFileInfo>
#include <QDateTime>

Model3DViewer::Model3DViewer(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setStyleSheet("Model3DViewer { background:#fff; border:1px solid #ddd; border-radius:3px; }");

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);

    m_display = new QLabel(this);
    m_display->setAlignment(Qt::AlignCenter);
    m_display->setWordWrap(true);
    m_display->setStyleSheet("color:#999; font-size:13px;");
    m_layout->addWidget(m_display, 1);

    clear();
}

void Model3DViewer::loadFile(const QString& filePath) {
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        m_display->setText(QString("File not found:\n%1").arg(filePath));
        m_display->setStyleSheet("color:#c62828; font-size:12px;");
        return;
    }

    QString sizeStr;
    qint64 sz = fi.size();
    if (sz < 1024) sizeStr = QString::number(sz) + " B";
    else if (sz < 1024*1024) sizeStr = QString::number(sz/1024) + " KB";
    else sizeStr = QString::number(sz/(1024*1024)) + " MB";

    QString info;
    info += QString("File: %1\n").arg(fi.fileName());
    info += QString("Path: %1\n").arg(fi.absolutePath());
    info += QString("Size: %1\n").arg(sizeStr);
    info += QString("Modified: %1\n")
                .arg(fi.lastModified().toString("yyyy-MM-dd hh:mm"));

    m_display->setText(info);
    m_display->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_display->setStyleSheet("color:#333; font-size:12px;");
}

void Model3DViewer::clear() {
    m_display->setText(
        "No model loaded.\n\n"
        "Click [Open Model] above\n"
        "to select a .step / .stp file.\n\n"
        "(3D viewport coming in next version)");
    m_display->setAlignment(Qt::AlignCenter);
    m_display->setStyleSheet("color:#999; font-size:13px;");
}
