#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileInfo>

#include "models/TestResult.h"

// ────────────────────────────────────────────────────────────
//  模型信息面板：显示输入/输出模型路径，检查是否存在
// ────────────────────────────────────────────────────────────
class ModelInfoPanel : public QWidget {
    Q_OBJECT
public:
    explicit ModelInfoPanel(QWidget* parent = nullptr);

    // 更新显示（传入当前选中的测试结果）
    void showModelInfo(const TestRunResult* result);

    // 清空
    void clear();

signals:
    void openFileRequested(const QString& path);  // 用户想看模型文件

private slots:
    void onToggleCollapse();

private:
    void setPathLabel(QLabel* label, const QString& title, const QString& path);

    QVBoxLayout*    m_layout;
    QPushButton*    m_btnToggle;     // 折叠/展开按钮
    QWidget*        m_content;       // 可折叠的内容区

    QLabel*         m_lblInterface;  // 接口名
    QLabel*         m_lblModelIn;    // 输入模型
    QLabel*         m_lblModelOut;   // 输出模型
    QLabel*         m_lblExtra;      // 其他属性

    bool            m_collapsed = false;
};
