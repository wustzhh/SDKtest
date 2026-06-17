#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileInfo>

// ────────────────────────────────────────────────────────────
//  3D 模型查看器占位（后续接入 Qt3D / OpenCASCADE）
//  当前功能：显示模型文件信息 + 鼠标操作提示
// ────────────────────────────────────────────────────────────
class Model3DViewer : public QWidget {
    Q_OBJECT
public:
    explicit Model3DViewer(QWidget* parent = nullptr);

    // 加载模型文件（显示文件信息）
    void loadFile(const QString& filePath);
    void clear();

signals:
    void openFileRequested();

private:
    QVBoxLayout* m_layout;
    QLabel*      m_display;   // 显示模型信息或提示
};
