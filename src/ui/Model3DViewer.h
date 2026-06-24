#pragma once

#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector>
#include <QVector3D>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

class GLViewer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit GLViewer(QWidget* parent = nullptr);
    void loadMesh(const QVector<QVector3D>& verts, const QVector<int>& tris);
    void resetView();
    void clear();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    QVector<QVector3D> m_verts;
    QVector<int> m_tri;
    float m_rotX=25,m_rotY=-35,m_zoom=1,m_modelSize=1;
    float m_panX=0,m_panY=0;
    QVector3D m_anchor;
    bool m_hasAnchor=false,m_pendingPick=false;
    QPointF m_pickPos;
    QPoint m_lastPos;
    bool m_dragging=false;
};

class Model3DViewer : public QWidget {
    Q_OBJECT
public:
    explicit Model3DViewer(QWidget* parent = nullptr);
    void loadFile(const QString& filePath);
    void clear();

private:
    GLViewer* m_gl;
    QLabel* m_status;
    QPushButton* m_btnReset;
};
