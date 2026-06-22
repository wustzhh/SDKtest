#include "StepReader.h"
#include "Logger.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <cmath>

struct Entity { int id; QString type; QString raw; };

static QVector<Entity> parseEntities(const QString& text) {
    QVector<Entity> ents;
    QRegularExpression re(R"(#(\d+)\s*=\s*(\w+)\s*\(((?:[^;]|\n)*?)\)\s*;)");
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        ents.append({m.captured(1).toInt(), m.captured(2), m.captured(3).simplified()});
    }
    return ents;
}

static QRegularExpression numRe(R"(#(\d+))");

StepMesh StepReader::parse(const QString& filePath) {
    StepMesh mesh;
    LOG("STEP", "Parsing: " + filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        mesh.error = "Cannot open file"; return mesh;
    }

    QString text = QString::fromUtf8(file.readAll());
    file.close();

    auto entities = parseEntities(text);
    QMap<int, const Entity*> byId;
    QMap<QString, QVector<const Entity*>> byType;
    for (const auto& e : entities) { byId[e.id] = &e; byType[e.type].append(&e); }
    LOG("STEP", "Entities: " + QString::number(entities.size()));

    // CARTESIAN_POINT → 3D point
    QMap<int, QVector3D> points;
    QRegularExpression numRe2(R"([-]?\d+\.?\d*(?:[eE][-+]?\d+)?)");
    for (const auto* e : byType.value("CARTESIAN_POINT")) {
        auto it = numRe2.globalMatch(e->raw);
        double v[3] = {0,0,0}; int i=0;
        while (it.hasNext() && i<3) v[i++] = it.next().captured().toDouble();
        points[e->id] = QVector3D(v[0], v[1], v[2]);
    }
    if (points.isEmpty()) { mesh.error = "No points"; return mesh; }
    LOG("STEP", "Points: " + QString::number(points.size()));

    // VERTEX_POINT → point id
    QMap<int, int> vtxToPt;
    for (const auto* e : byType.value("VERTEX_POINT")) {
        auto m = numRe.match(e->raw);
        if (m.hasMatch()) vtxToPt[e->id] = m.captured(1).toInt();
    }
    LOG("STEP", "Vertices: " + QString::number(vtxToPt.size()));

    // EDGE_CURVE → edges (unique point pairs)
    // 对 CIRCLE 曲线做细分，生成多个线段
    struct Edge { int p1, p2; };
    QMap<int, Edge> edgeById;
    QSet<QPair<int,int>> edgeSet;
    // 新增顶点集（曲线细分产生的中间点）
    int nextPtId = points.isEmpty() ? 1 : (std::prev(points.end()).key()) + 1000;
    for (const auto* e : byType.value("EDGE_CURVE")) {
        // EDGE_CURVE('',#startV,#endV,#curve,.T.)
        auto it = numRe.globalMatch(e->raw);
        int ids[3] = {-1,-1,-1}, i=0;
        while (it.hasNext() && i<3) {
            int vid = it.next().captured(1).toInt();
            if (i<2 && vtxToPt.contains(vid)) ids[i++] = vtxToPt[vid];
            else if (i==2) { ids[i++] = vid; } // 第三个是曲线实体ID
        }
        if (ids[0]<0 || ids[1]<0) continue;
        if (ids[0]==ids[1]) continue;

        // 检查曲线类型，对 CIRCLE 做细分
        int curveId = ids[2];
        bool isCircle = (curveId>=0 && byId.contains(curveId) && byId[curveId]->type == "CIRCLE");

        if (isCircle) {
            // 从 CIRCLE 提取半径（第3个参数，跳过 '# 号引用）
            // CIRCLE('',#axisPlacement,radius)
            double radius = 0;
            bool gotRadius = false;
            {
                // 找原始文本中的数字（跳过第一个 # 引用）
                QString cirRaw = byId[curveId]->raw;
                int pos = cirRaw.lastIndexOf(',');
                if (pos >= 0) {
                    QString lastParam = cirRaw.mid(pos+1).trimmed();
                    if (lastParam.endsWith(')')) lastParam.chop(1);
                    bool ok;
                    double r = lastParam.toDouble(&ok);
                    if (ok) { radius = r; gotRadius = true; }
                }
            }

            if (gotRadius) {
                // 解析 AXIS2_PLACEMENT_3D
                auto apIt2 = numRe.globalMatch(byId[curveId]->raw);
                if (apIt2.hasNext()) {
                    int apId2 = apIt2.next().captured(1).toInt();
                    if (byId.contains(apId2) && byId[apId2]->type == "AXIS2_PLACEMENT_3D") {
                        // 提取 (center, zDir, xDir)
                        QVector3D center, zDir(0,0,1), xDir(1,0,0);
                        auto refs = numRe.globalMatch(byId[apId2]->raw);
                        for (int ri = 0; ri < 3 && refs.hasNext(); ri++) {
                            int rid = refs.next().captured(1).toInt();
                            if (!byId.contains(rid)) continue;
                            if (byId[rid]->type == "CARTESIAN_POINT" && points.contains(rid))
                                center = points[rid];
                            else if (byId[rid]->type == "DIRECTION") {
                                // DIRECTION('',(x,y,z))
                                QRegularExpression fr(R"([-]?\d+\.?\d*(?:[eE][-+]?\d+)?)");
                                auto fi = fr.globalMatch(byId[rid]->raw);
                                double dv[3] = {0,0,0}; int di = 0;
                                while (fi.hasNext() && di<3) dv[di++] = fi.next().captured().toDouble();
                                QVector3D dir(dv[0], dv[1], dv[2]);
                                dir.normalize();
                                if (ri == 1) zDir = dir;
                                else if (ri == 2) xDir = dir;
                            }
                        }
                        // Y = Z × X
                        QVector3D yDir = QVector3D::crossProduct(zDir, xDir);
                        yDir.normalize();

                        // 细分圆弧
                        int N = 14;
                        QVector3D startP = points[ids[0]];
                        QVector3D endP   = points[ids[1]];
                        float ang1 = 0, ang2 = 0;
                        // 在局部坐标系中计算角度
                        QVector3D lv1 = startP - center, lv2 = endP - center;
                        ang1 = atan2(QVector3D::dotProduct(lv1, yDir),
                                     QVector3D::dotProduct(lv1, xDir));
                        ang2 = atan2(QVector3D::dotProduct(lv2, yDir),
                                     QVector3D::dotProduct(lv2, xDir));
                        float da = ang2 - ang1;
                        while (da > M_PI) da -= 2*M_PI;
                        while (da < -M_PI) da += 2*M_PI;

                        int prevPt = ids[0];
                        for (int k = 1; k <= N; k++) {
                            float t = float(k) / N;
                            float a = ang1 + da * t;
                            int ptId = nextPtId++;
                            points[ptId] = center + xDir * (radius*cos(a)) + yDir * (radius*sin(a));
                            if (k == N) ptId = ids[1];
                            if (prevPt != ptId) {
                                int a2 = prevPt, b2 = ptId;
                                if (a2 > b2) qSwap(a2, b2);
                                edgeSet.insert({a2, b2});
                            }
                            prevPt = ptId;
                        }
                        edgeById[e->id] = {ids[0], ids[1]};
                        continue;
                    }
                }
            }
            // fall through to straight line
        }

        // 直线段（LINE 或 曲线细分失败）
        edgeById[e->id] = {ids[0], ids[1]};
        int a2 = ids[0], b2 = ids[1];
        if (a2 > b2) qSwap(a2, b2);
        edgeSet.insert({a2, b2});
    }
    LOG("STEP", "Edges: " + QString::number(edgeById.size()));

    if (edgeById.isEmpty()) {
        for (auto it = points.constBegin(); it != points.constEnd(); ++it)
            mesh.vertices.append(it.value());
        mesh.error = "No edges";
        return mesh;
    }

    // ── 面解析 + 三角剖分 ──
    QVector<QVector<int>> facePointIds;  // 每个面：点id列表（按环绕顺序）
    for (const auto* af : byType.value("ADVANCED_FACE")) {
        // ADVANCED_FACE('',(#36,#155),#50,.T.)
        auto it = numRe.globalMatch(af->raw);
        it.next(); // skip face name
        QVector<int> faceVerts;
        QSet<int> visited;

        while (it.hasNext()) {
            int fbId = it.next().captured(1).toInt();
            if (!byId.contains(fbId)) continue;
            // FACE_BOUND → EDGE_LOOP → ORIENTED_EDGE → EDGE_CURVE
            auto fbIt = numRe.globalMatch(byId[fbId]->raw);
            while (fbIt.hasNext()) {
                int elId = fbIt.next().captured(1).toInt();
                if (!byId.contains(elId)) continue;
                auto elIt = numRe.globalMatch(byId[elId]->raw);
                while (elIt.hasNext()) {
                    int oeId = elIt.next().captured(1).toInt();
                    if (!byId.contains(oeId)) continue;
                    // ORIENTED_EDGE('',*,*,#edgeId,.T.) → 第三个#是edge
                    int ecId = -1;
                    auto oeIt = numRe.globalMatch(byId[oeId]->raw);
                    for (int k=0; oeIt.hasNext(); k++) {
                        auto em = oeIt.next();
                        if (k == 2) { ecId = em.captured(1).toInt(); break; }
                    }
                    if (edgeById.contains(ecId)) {
                        auto& ed = edgeById[ecId];
                        if (!visited.contains(ed.p1)) { visited.insert(ed.p1); faceVerts.append(ed.p1); }
                        if (!visited.contains(ed.p2)) { visited.insert(ed.p2); faceVerts.append(ed.p2); }
                    }
                }
            }
        }
        if (faceVerts.size() >= 3)
            facePointIds.append(faceVerts);
    }
    LOG("STEP", "Faces: " + QString::number(facePointIds.size()));

    // ── 建立全局顶点索引 ──
    QMap<int, int> ptToGlobal;
    auto getGlobal = [&](int ptId) {
        if (!ptToGlobal.contains(ptId)) {
            ptToGlobal[ptId] = mesh.vertices.size();
            mesh.vertices.append(points[ptId]);
        }
        return ptToGlobal[ptId];
    };

    // 线框索引
    for (const auto& edge : edgeSet) {
        mesh.indices.append(getGlobal(edge.first));
        mesh.indices.append(getGlobal(edge.second));
    }

    // 三角面索引（扇型剖分）
    for (const auto& face : facePointIds) {
        if (face.size() < 3) continue;
        int v0 = getGlobal(face[0]);
        for (int i = 1; i < face.size()-1; i++) {
            mesh.triangles.append(v0);
            mesh.triangles.append(getGlobal(face[i]));
            mesh.triangles.append(getGlobal(face[i+1]));
        }
    }

    mesh.success = !mesh.vertices.isEmpty() && !mesh.indices.isEmpty();
    LOG("STEP", "Verts=" + QString::number(mesh.vertices.size())
        + " Edges=" + QString::number(mesh.indices.size()/2)
        + " Tris=" + QString::number(mesh.triangles.size()/3));
    return mesh;
}
