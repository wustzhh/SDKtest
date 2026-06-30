#include "TestProgressPanel.h"
#include <QScrollBar>

TestProgressPanel::TestProgressPanel(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 4);

    // ── 进度条 ──
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setStyleSheet(
        "QProgressBar { background:#f1f5f9; border:none; border-radius:4px; text-align:center; height:22px; font-size:11px; color:#64748b; }"
        "QProgressBar::chunk { background:#6366f1; border-radius:4px; }");

    m_lblProgress = new QLabel("就绪", this);
    m_lblProgress->setStyleSheet("font-size: 13px; color: #64748b;");

    // ── 日志 ──
    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Consolas", 10));
    m_logView->setStyleSheet("background: #1e1e1e; color: #d4d4d4;");
    m_logView->setMinimumHeight(120);

    // ── 取消按钮 ──
    m_btnCancel = new QPushButton("取消运行", this);
    m_btnCancel->setEnabled(false);
    m_btnCancel->setStyleSheet(
        "QPushButton { background: #fef2f2; color: #ef4444; border: 1px solid #fecaca; "
        "border-radius: 6px; padding: 5px 14px; font-weight:500; }"
        "QPushButton:hover { background: #fee2e2; border-color:#fca5a5; }"
        "QPushButton:disabled { background: #f8f9fb; color: #94a3b8; border-color: #e2e8f0; }");
    connect(m_btnCancel, &QPushButton::clicked, this, &TestProgressPanel::cancelRequested);

    m_layout->addWidget(m_lblProgress);
    m_layout->addWidget(m_progressBar);
    m_layout->addWidget(m_logView);
    m_layout->addWidget(m_btnCancel);
}

void TestProgressPanel::startRun(int totalTests) {
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(totalTests);
    m_lblProgress->setText(QString("运行中... 0 / %1").arg(totalTests));
    m_logView->clear();
    m_btnCancel->setEnabled(true);
}

void TestProgressPanel::updateProgress(int done, int total) {
    m_progressBar->setValue(done);
    m_progressBar->setMaximum(total);
    m_lblProgress->setText(QString("运行中... %1 / %2").arg(done).arg(total));
}

void TestProgressPanel::appendLog(const QString& line) {
    m_logView->append(line);
    // 自动滚动到底部
    auto sb = m_logView->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

void TestProgressPanel::finishRun() {
    m_btnCancel->setEnabled(false);
    m_progressBar->setValue(m_progressBar->maximum());
    m_lblProgress->setText("运行完成 ✓");
    appendLog("\n══════ 运行完成 ══════");
}

void TestProgressPanel::reset() {
    m_progressBar->setValue(0);
    m_lblProgress->setText("就绪");
    m_logView->clear();
    m_btnCancel->setEnabled(false);
}
