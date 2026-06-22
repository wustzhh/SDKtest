#pragma once

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QMap>
#include <QSet>
#include <QPair>

// ────────────────────────────────────────────────────────────
//  轻量 STEP 文件解析器（无外部依赖）
//  只解析 B-Rep 几何的顶点坐标用于可视化
// ────────────────────────────────────────────────────────────
struct StepMesh {
    QVector<QVector3D> vertices;
    QVector<int> indices;     // 线框（每2个一组 = 一条边）
    QVector<int> triangles;   // 三角面（每3个一组 = 一个三角）
    bool success = false;
    QString error;
};

class StepReader {
public:
    static StepMesh parse(const QString& filePath);
};
