#pragma once
// ═══════════════════════════════════════════════════════════════
//  StepLoader: STEP/NAS 模型加载 + BRepMesh 剖分（独立模块，供 UI 与测试共用）
//  从 Model3DViewer.cpp 拆出，不依赖 Qt Widgets / OpenGL
// ═══════════════════════════════════════════════════════════════

#include <QObject>
#include <QString>
#include <QVector>
#include <QVector3D>

#ifdef HAS_OCC
#include <TopoDS_Shape.hxx>
#endif

// 面包围盒（与 faceCenterIds 一一对应）
struct FaceBBox {
    double minX, minY, minZ;
    double maxX, maxY, maxZ;
};

// 边线（3D 渲染用）
struct EdgeLine {
    int v0, v1;
    QVector3D color;
};

// 模型加载结果（顶点/三角形/法线/边线/面信息）
struct StepLoadResult {
    bool ok = false;
    QString error;
    QVector<QVector3D> verts;
    QVector<int> tris;
    QVector<QVector3D> normals;
    QVector<EdgeLine> edges;
    QVector<int> faceIds;        // 每个三角形的面 ID（与 tris 一一对应）
    QVector<QVector3D> faceCenters;   // 每个面的中心点
    QVector<int> faceCenterIds;       // 与 faceCenters 一一对应的面 ID
    QVector<FaceBBox> faceBBoxes;     // 每个面的包围盒（与 faceCenterIds 一一对应）
    int elapsedMs = 0;
#ifdef HAS_OCC
    TopoDS_Shape shape;
#endif
};

// STEP/NAS 加载 + BRepMesh 剖分工人
class StepWorker : public QObject {
    Q_OBJECT
public:
    explicit StepWorker(const QString& path);
    void doWork();
signals:
    void progress(const QString& text);
    void finished(const StepLoadResult& result);
private:
    QString m_path;
};
