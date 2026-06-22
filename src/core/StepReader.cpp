#include "StepReader.h"
#include "Logger.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <cmath>

// ────────────────────────────────────────────────────────────
//  STEP 实体解析
// ────────────────────────────────────────────────────────────

// 收集所有 #N = value 格式的实体
struct Entity {
    int id;
    QString type;
    QString raw;  // 完整原始行（含续行）
};

static QVector<Entity> parseEntities(const QString& text) {
    QVector<Entity> ents;
    // 匹配 #数字 = 类型 ( ... );
    static QRegularExpression re(R"(#(\d+)\s*=\s*(\w+)\s*\(((?:[^;]|\n)*?)\)\s*;)");
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        Entity e;
        e.id = m.captured(1).toInt();
        e.type = m.captured(2);
        e.raw = m.captured(3).simplified();
        ents.append(e);
    }
    return ents;
}

// ────────────────────────────────────────────────────────────
//  主要解析逻辑
// ────────────────────────────────────────────────────────────

StepMesh StepReader::parse(const QString& filePath) {
    StepMesh mesh;
    LOG("STEP", "Parsing: " + filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        mesh.error = "Cannot open file";
        return mesh;
    }

    QString text = QString::fromUtf8(file.readAll());
    file.close();
    LOG("STEP", "File size: " + QString::number(text.size()) + " bytes");

    auto entities = parseEntities(text);

    // 建立实体索引
    QMap<int, const Entity*> byId;
    QMap<QString, QVector<const Entity*>> byType;
    for (const auto& e : entities) {
        byId[e.id] = &e;
        byType[e.type].append(&e);
    }

    LOG("STEP", "Entities: " + QString::number(entities.size()));

    // 收集所有 CARTESIAN_POINT
    QMap<int, QVector3D> points;  // entity id → 3D point
    for (const auto* e : byType.value("CARTESIAN_POINT")) {
        // (x, y, z)
        static QRegularExpression re(R"([-]?\d+\.?\d*(?:[eE][-+]?\d+)?)");
        auto it = re.globalMatch(e->raw);
        double vals[3] = {0,0,0};
        int idx = 0;
        while (it.hasNext() && idx < 3) {
            vals[idx++] = it.next().captured().toDouble();
        }
        points[e->id] = QVector3D(vals[0], vals[1], vals[2]);
    }

    LOG("STEP", "Points: " + QString::number(points.size()));

    if (points.isEmpty()) {
        mesh.error = "No CARTESIAN_POINT found";
        return mesh;
    }

    // 收集 VERTEX_POINT → CARTESIAN_POINT
    QMap<int, int> vtxToPt;
    for (const auto* e : byType.value("VERTEX_POINT")) {
        static QRegularExpression re(R"(#(\d+))");
        auto m = re.match(e->raw);
        if (m.hasMatch()) vtxToPt[e->id] = m.captured(1).toInt();
    }
    LOG("STEP", "Vertices: " + QString::number(vtxToPt.size()));

    // 收集 EDGE_CURVE → (start_vertex, end_vertex)
    // 并直接生成线框
    QSet<QPair<int,int>> edgeSet;
    QRegularExpression numRe(R"(#(\d+))");

    for (const auto* e : byType.value("EDGE_CURVE")) {
        auto it = numRe.globalMatch(e->raw);
        int ids[2] = {-1, -1}, idx = 0;
        while (it.hasNext() && idx < 2) {
            int vid = it.next().captured(1).toInt();
            if (vtxToPt.contains(vid))
                ids[idx++] = vtxToPt[vid];
        }
        if (ids[0] >= 0 && ids[1] >= 0 && ids[0] != ids[1]) {
            if (ids[0] > ids[1]) qSwap(ids[0], ids[1]);
            edgeSet.insert({ids[0], ids[1]});
        }
    }

    LOG("STEP", "Edges: " + QString::number(edgeSet.size()));

    if (edgeSet.isEmpty()) {
        mesh.error = "No edges found. Showing point cloud.";
        // 点云模式：所有点画成小点
        for (auto it = points.constBegin(); it != points.constEnd(); ++it)
            mesh.vertices.append(it.value());
        mesh.success = true;
        return mesh;
    }

    // 建立点id → 全局索引
    QMap<int, int> ptToGlobal;
    for (const auto& edge : edgeSet) {
        if (!ptToGlobal.contains(edge.first)) {
            ptToGlobal[edge.first] = mesh.vertices.size();
            mesh.vertices.append(points[edge.first]);
        }
        if (!ptToGlobal.contains(edge.second)) {
            ptToGlobal[edge.second] = mesh.vertices.size();
            mesh.vertices.append(points[edge.second]);
        }
    }

    // 生成线段（每两个点形成一条线）
    for (const auto& edge : edgeSet) {
        mesh.indices.append(ptToGlobal[edge.first]);
        mesh.indices.append(ptToGlobal[edge.second]);
    }

    mesh.success = !mesh.vertices.isEmpty() && !mesh.indices.isEmpty();
    if (!mesh.success) mesh.error = "No output";

    LOG("STEP", "Wireframe: " + QString::number(mesh.vertices.size()) + " verts, "
        + QString::number(mesh.indices.size() / 2) + " edges");

    return mesh;
}
