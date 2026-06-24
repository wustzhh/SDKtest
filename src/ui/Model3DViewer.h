#pragma once

#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector>
#include <QVector3D>
#include <QQuaternion>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QThread>
#include <QTimer>

struct EdgeLine {
    int v0, v1;
    QVector3D color;
};

#ifdef HAS_OCC
struct StepLoadResult {
    bool ok = false;
    QString error;
    QVector<QVector3D> verts;
    QVector<int> tris;
    QVector<QVector3D> normals;
    QVector<EdgeLine> edges;
    QVector<int> faceIds;    // 三角形的面索引（与 tris 一一对应，每 3 个一组）
    QVector<QVector3D> faceCenters; // 每个面的中心点
    int elapsedMs = 0;
};

class StepWorker : public QObject {
    Q_OBJECT
public:
    StepWorker(const QString& path) : m_path(path) {}
    void doWork();
signals:
    void progress(const QString& text);
    void finished(const StepLoadResult& result);
private:
    QString m_path;
};
#endif

class GLViewer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit GLViewer(QWidget* parent = nullptr);
    void loadMesh(const QVector<QVector3D>& verts, const QVector<int>& tris,
                  const QVector<QVector3D>& normals = {}, const QVector<EdgeLine>& edges = {},
                  const QVector<int>& faceIds = {}, const QVector<QVector3D>& faceCenters = {});
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
    QVector<QVector3D> m_normals;
    QVector<EdgeLine> m_edges;
    QVector<int> m_faceIds;
    QVector<QVector3D> m_faceCenters;
    QQuaternion m_rot;
    float m_zoom=1,m_modelSize=1;
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
    ~Model3DViewer() override;
    void loadFile(const QString& filePath);
    void clear();

private:
    void cancelLoad();
    GLViewer* m_gl;
    QLabel* m_status;
    QPushButton* m_btnReset;
    void updateCountdown();
    QThread* m_workerThread = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    QTimer* m_countdownTimer = nullptr;
    int m_remainingSeconds = 0;
    StepWorker* m_worker = nullptr;
};
