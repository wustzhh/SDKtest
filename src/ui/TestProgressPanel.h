#pragma once

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

// ────────────────────────────────────────────────────────────
//  进度 + 实时日志面板
// ────────────────────────────────────────────────────────────
class TestProgressPanel : public QWidget {
    Q_OBJECT
public:
    explicit TestProgressPanel(QWidget* parent = nullptr);

    // 初始化运行
    void startRun(int totalTests);

    // 更新进度
    void updateProgress(int done, int total);

    // 追加日志
    void appendLog(const QString& line);

    // 运行完成
    void finishRun();

    // 重置
    void reset();

signals:
    void cancelRequested();

private:
    QVBoxLayout*    m_layout;
    QProgressBar*   m_progressBar;
    QLabel*         m_lblProgress;
    QTextEdit*      m_logView;
    QPushButton*    m_btnCancel;
};
