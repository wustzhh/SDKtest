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
        "QProgressBar { background:#f1f5f9; border:none; border-radius:4px; text-align:center; height:22px; font-size:11px; color:#0f172a; font-weight:700; }"
        "QProgressBar::chunk { background:#6366f1; border-radius:4px; }");

    m_lblProgress = new QLabel("就绪", this);
    m_lblProgress->setStyleSheet("font-size: 13px; color: #64748b;");

    // ── 计时 ──
    m_lblElapsed = new QLabel("", this);
    m_lblElapsed->setStyleSheet("font-size: 12px; color: #94a3b8; font-family: Consolas, monospace;");
    m_lblElapsed->setVisible(false);
    m_elapsedTimer = new QTimer(this);
    connect(m_elapsedTimer, &QTimer::timeout, this, &TestProgressPanel::updateElapsed);

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
    m_layout->addWidget(m_lblElapsed);
    m_layout->addWidget(m_progressBar);
    m_layout->addWidget(m_logView);
    m_layout->addWidget(m_btnCancel);
}

void TestProgressPanel::startRun(int totalTests) {
    m_totalTests = totalTests;
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(totalTests);
    m_lblProgress->setText(QString("运行中... 0 / %1").arg(totalTests));
    m_lblElapsed->setVisible(true);
    m_startTime.start();
    m_elapsedTimer->start(1000);
    updateElapsed();
    m_logView->clear();
    m_btnCancel->setEnabled(true);
}

void TestProgressPanel::updateElapsed() {
    qint64 ms = m_startTime.elapsed();
    int secs = ms / 1000;
    int mins = secs / 60;
    int hrs  = mins / 60;
    int days = hrs / 24;
    secs %= 60; mins %= 60; hrs %= 24;
    QString text;
    if (days > 0)
        text = QString("%1:%2:%3:%4").arg(days, 2, 10, QChar('0'))
               .arg(hrs, 2, 10, QChar('0')).arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    else if (hrs > 0)
        text = QString("%1:%2:%3").arg(hrs, 2, 10, QChar('0'))
               .arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    else
        text = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    m_lblElapsed->setText(QString::fromUtf8("\xe2\x96\xb7 %1").arg(text));
}

void TestProgressPanel::updateProgress(int done, int total) {
    m_progressBar->setValue(done);
    double pct = total > 0 ? qMin(done * 100.0 / total, 100.0) : 0.0;
    m_progressBar->setFormat(QString("%1%")
        .arg(QString::number(pct, 'f', 1)));
    // 不修改 maximum（startRun 已设好，防止分母变化）
    m_lblProgress->setText(QString("运行中... %1 / %2").arg(done).arg(total));
}

void TestProgressPanel::appendLog(const QString& line) {
    m_logView->append(line);
    // 自动滚动到底部
    auto sb = m_logView->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

void TestProgressPanel::finishRun() {
    m_elapsedTimer->stop();
    m_btnCancel->setEnabled(false);
    m_progressBar->setValue(m_progressBar->maximum());
    updateElapsed();
    m_lblProgress->setText(QString("运行完成 \u2713  (\u5171 %1 \u4e2a\u7528\u4f8b)").arg(m_totalTests));
    appendLog("\n══════ 运行完成 ══════");
}

void TestProgressPanel::reset() {
    m_elapsedTimer->stop();
    m_lblElapsed->setVisible(false);
    m_progressBar->setValue(0);
    m_lblProgress->setText("就绪");
    m_logView->clear();
    m_btnCancel->setEnabled(false);
}
