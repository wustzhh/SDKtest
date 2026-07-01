#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileInfo>
#include <QHBoxLayout>

#include "models/TestResult.h"

class ModelInfoPanel : public QWidget {
    Q_OBJECT
public:
    explicit ModelInfoPanel(QWidget* parent = nullptr);
    void showModelInfo(const TestRunResult* result);
    void clear();

signals:
    void openFileRequested(const QString& path);

private slots:
    void onToggleCollapse();

private:
    struct ModelRow {
        QWidget*   container;
        QLabel*    label;
        QPushButton* btnOpen;
    };
    ModelRow makeRow(const QString& title);

    QVBoxLayout*    m_layout;
    QPushButton*    m_btnToggle;
    QWidget*        m_content;

    ModelRow        m_interface;
    ModelRow        m_modelIn;
    ModelRow        m_modelOut;
    QLabel*         m_lblExtra;

    bool            m_collapsed = false;
public:
    bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool c);
};
