#include "StepLoader.h"
#include "core/Logger.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QtMath>
#include <cmath>
#include <QMap>
#include <QSet>

#ifdef HAS_OCC
#include <STEPControl_Reader.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <Geom_Plane.hxx>
#include <GeomAbs_Shape.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <Poly_Triangulation.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Geom_Curve.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepExtrema_DistShapeShape.hxx>

StepWorker::StepWorker(const QString& path) : m_path(path) {}

void StepWorker::doWork() {
    StepLoadResult r; QElapsedTimer t; t.start();
    emit progress(QString::fromUtf8("\xE8\xAF\xBB\xE5\x8F\x96 STEP..."));
    LOG("3D", QString("stage: read start t=%1ms").arg(t.elapsed()));
    STEPControl_Reader reader;
    if (reader.ReadFile(m_path.toUtf8().constData()) != IFSelect_RetDone) { r.error="ReadFile failed"; emit finished(r); return; }
    LOG("3D", QString("stage: read done t=%1ms").arg(t.elapsed()));
    emit progress(QString::fromUtf8("\xE8\xBD\xAC\xE6\x8D\xA2\xE5\xBD\xA2\xE7\x8A\xB6..."));
    reader.TransferRoots(); TopoDS_Shape shape = reader.OneShape();
    LOG("3D", QString("stage: transfer done t=%1ms").arg(t.elapsed()));
    r.shape = shape;  // 保存用于射线拾取
    if (shape.IsNull()) { r.error="Shape is null"; emit finished(r); return; }
    emit progress(QString::fromUtf8("\xE4\xB8\x89\xE8\xA7\x92\xE5\x8C\x96..."));
    qint64 tMesh=0, tFaces=0, tEdges=0;
    QElapsedTimer stage;
    // 全局粗剖一次（内部并行优化），然后平面单独极粗重剖
    double diag = 1.0;
    int totalFaces = 0;
    { TopExp_Explorer fc(shape, TopAbs_FACE); for (; fc.More(); fc.Next()) totalFaces++; }
    {
        Bnd_Box shapeBox; BRepBndLib::Add(shape, shapeBox);
        double x1,y1,z1,x2,y2,z2;
        if (!shapeBox.IsVoid()) { shapeBox.Get(x1,y1,z1,x2,y2,z2); }
        diag = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) + (z2-z1)*(z2-z1));
    }
    double deflection = qMax(0.01, diag * 0.01);
    double angDefl = 1.5 * M_PI / 180.0;
    LOG("MESH",QString("diag=%1 faces=%2 defl=%3 ang=%4°")
        .arg(diag,0,'f',1).arg(totalFaces).arg(deflection,0,'f',3)
        .arg(angDefl*180.0/M_PI,0,'f',2));
    LOG("3D", QString("stage: mesh start t=%1ms").arg(t.elapsed()));
    BRepMesh_IncrementalMesh(shape, deflection, Standard_False, angDefl, true).Perform();
    LOG("3D", QString("stage: mesh done t=%1ms").arg(t.elapsed()));
    // ==== 平面优化（修改前版本）：GetType()==Plane 即极粗剖 ====
    { TopExp_Explorer fExp(shape, TopAbs_FACE); for (; fExp.More(); fExp.Next()) {
        TopoDS_Face face = TopoDS::Face(fExp.Current());
        BRepAdaptor_Surface ads(face);
        if (ads.GetType() == GeomAbs_Plane)
            BRepMesh_IncrementalMesh(face, 1e6, Standard_False, 30.0*M_PI/180.0, Standard_False).Perform();
    }}
    // ==== END ====
    tMesh = stage.elapsed();
    // 边线采样间距
    double edgeSpacing = qBound(0.001, diag * 0.001, 2.0);
    stage.start();
    emit progress(QString::fromUtf8("\xE6\x8F\x90\xE5\x8F\x96\xE7\xBD\x91\xE6\xA0\xBC..."));
    int skippedFaces = 0;
    int voff=0, faceIdx=0;
    // 预分配内存减少reallocation
    r.verts.reserve(totalFaces * 100);
    r.tris.reserve(totalFaces * 300);
    r.normals.reserve(totalFaces * 100);
    r.faceIds.reserve(totalFaces * 100);
    r.faceCenters.reserve(totalFaces);
    r.faceCenterIds.reserve(totalFaces);
    r.faceBBoxes.reserve(totalFaces);
    TopExp_Explorer fExp(shape, TopAbs_FACE);
    for (; fExp.More(); fExp.Next()) {
        TopoDS_Face face = TopoDS::Face(fExp.Current()); TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) { skippedFaces++; faceIdx++; continue; }
        int base=voff, triStart=r.tris.size()/3;
        r.faceCenterIds.append(faceIdx);
        for (int i=1;i<=tri->NbNodes();i++) { gp_Pnt p=tri->Node(i).Transformed(loc.Transformation()); r.verts.append(QVector3D(p.X(),p.Y(),p.Z())); voff++; }
        for (int i=1;i<=tri->NbTriangles();i++) { int n1,n2,n3; tri->Triangle(i).Get(n1,n2,n3); r.tris.append(base+n1-1); r.tris.append(base+n2-1); r.tris.append(base+n3-1); }
        for (int i=1;i<=tri->NbTriangles();i++) {
            int n1,n2,n3; tri->Triangle(i).Get(n1,n2,n3);
            gp_Pnt p1=tri->Node(n1).Transformed(loc.Transformation()),p2=tri->Node(n2).Transformed(loc.Transformation()),p3=tri->Node(n3).Transformed(loc.Transformation());
            gp_Vec e1(p1,p2),e2(p1,p3),n=e1.Crossed(e2);
            if (n.Magnitude()>1e-10) n.Normalize(); else n.SetCoord(0,0,1);
            if (face.Orientation()==TopAbs_REVERSED) n.Reverse();
            for (int j=0;j<3;j++) { int vi=(j==0?n1:(j==1?n2:n3)); if (base+vi-1>=r.normals.size()) r.normals.resize(base+vi); r.normals[base+vi-1]+=QVector3D(n.X(),n.Y(),n.Z()); }
        }
        int triEnd=r.tris.size()/3;
        // 归一化累加的法线
        for (int ni=base;ni<base+tri->NbNodes();ni++) { if (ni<r.normals.size()) { float mag=r.normals[ni].length(); if (mag>1e-10f) r.normals[ni]/=mag; } }
        // 面中心（取所有顶点平均）
        QVector3D center(0,0,0); int vcnt=0;
        for (int i=1;i<=tri->NbNodes();i++) { gp_Pnt p=tri->Node(i).Transformed(loc.Transformation()); center+=QVector3D(p.X(),p.Y(),p.Z()); vcnt++; }
        if (vcnt>0) center/=vcnt; r.faceCenters.append(center);
        // 面包围盒（从 B-Rep 精确几何算，保证与测试端输出的 AABB 一致）
        {   Bnd_Box bbox;
            BRepBndLib::Add(face, bbox);
            double bx1=0,by1=0,bz1=0,bx2=0,by2=0,bz2=0;
            if (!bbox.IsVoid()) bbox.Get(bx1,by1,bz1,bx2,by2,bz2);
            r.faceBBoxes.append({bx1,by1,bz1,bx2,by2,bz2});
        }
        for (int ti=triStart;ti<triEnd;ti++) r.faceIds.append(faceIdx);
        faceIdx++;
    }
    tMesh = stage.elapsed();
    if (r.verts.isEmpty()||r.tris.isEmpty()) { r.error="No triangles"; emit finished(r); return; }
    emit progress(QString::fromUtf8("\xE7\x94\x9F\xE6\x88\x90\xE8\xBE\xB9\xE7\xBA\xBF..."));
    QVector<TopoDS_Edge> allEdges; QMap<void*,QSet<void*>> edgeFaceMap;
    { TopExp_Explorer eExp(shape, TopAbs_EDGE); for (; eExp.More(); eExp.Next()) { void* p=eExp.Current().TShape().get(); if (!edgeFaceMap.contains(p)) { edgeFaceMap[p]={}; allEdges.append(TopoDS::Edge(eExp.Current())); } } }
    { TopExp_Explorer fExp(shape, TopAbs_FACE); for (; fExp.More(); fExp.Next()) { void* fp=fExp.Current().TShape().get(); TopExp_Explorer eExp(fExp.Current(), TopAbs_EDGE); for (; eExp.More(); eExp.Next()) { void* ep=eExp.Current().TShape().get(); if (edgeFaceMap.contains(ep)) edgeFaceMap[ep].insert(fp); } } }
    // 边线使用独立顶点（不复用面网格顶点），避免哈希碰撞导致错误连线
    int edgeVertBase = r.verts.size();
    int totalEdges = allEdges.size(), renderedEdges = 0, filteredEdges = 0, nonManifoldEdges = 0;
    for (const auto& ed : allEdges) {
        if (BRep_Tool::Degenerated(ed)) { filteredEdges++; continue; }
        int nf=(int)edgeFaceMap.value(ed.TShape().get()).size();
        if (nf == 0) { filteredEdges++; continue; }  // 孤立边不渲染
        if (nf >= 3) nonManifoldEdges++;
        QVector3D col = (nf==1) ? QVector3D(1.0f, 0.15f, 0.15f)  // 自由边红色
                      : (nf==2) ? QVector3D(0.15f, 0.85f, 0.15f)  // 正常绿色
                                : QVector3D(1.0f, 0.85f, 0.1f);   // 非流形黄色
        double f,l; Handle(Geom_Curve) crv=BRep_Tool::Curve(ed,f,l); if (crv.IsNull()) continue;
        GeomAdaptor_Curve acrv(crv, f, l);
        double edgeLen = GCPnts_AbscissaPoint::Length(acrv);
        // 自适应间距和段数
        int ns = qBound(18, (int)(edgeLen / edgeSpacing), (diag < 1.0) ? 500 : 200);
        GCPnts_UniformAbscissa ua(acrv, ns + 1);
        int prev = -1;
        auto sampleAndAdd = [&](const gp_Pnt& pt) {
            int idx = r.verts.size();
            r.verts.append(QVector3D(pt.X(), pt.Y(), pt.Z()));
            if (prev >= 0) r.edges.append({prev, idx, col});
            prev = idx;
        };
        if (ua.IsDone() && ua.NbPoints() >= 2) {
            for (int s = 1; s <= ua.NbPoints(); s++)
                sampleAndAdd(crv->Value(ua.Parameter(s)));
        } else {
            int ns2 = 36; double st = (l-f)/ns2;
            for (int s = 0; s <= ns2; s++)
                sampleAndAdd(crv->Value((s==ns2) ? l : f + s*st));
        }
        renderedEdges++;
    }
    if (r.normals.size()<r.verts.size()) { int o=r.normals.size(); r.normals.resize(r.verts.size()); for (int i=o;i<r.verts.size();i++) r.normals[i]=QVector3D(0,1,0); }
    // 调试输出（仅在开启 debug 宏时启用，避免大模型下大量磁盘I/O拖慢加载）
#if 0
    {   QString logPath = QCoreApplication::applicationDirPath() + "/test_runner_ui_debug.log";
        FILE* df = fopen(logPath.toUtf8().constData(), "a");
        if (df) { fprintf(df, "\n===== %s =====\n", QFileInfo(m_path).fileName().toUtf8().constData()); }
        if (df) {
            fprintf(df, "=== OCCT Debug ===\nFile: %s\n\n", m_path.toUtf8().constData());
            std::map<void*,int> fid; int nf=0;
            { TopExp_Explorer fe(shape,TopAbs_FACE); for(;fe.More();fe.Next()) fid[fe.Current().TShape().get()] = nf++; }
            fprintf(df, "%-8s  %12s %12s %12s  %12s %12s %12s\n",
                    "Face", "minX", "minY", "minZ", "maxX", "maxY", "maxZ");
            fprintf(df, "--------  ------------ ------------ ------------  ------------ ------------ ------------\n");
            {
                TopExp_Explorer feB(shape, TopAbs_FACE);
                int fi = 0;
                for (; feB.More(); feB.Next(), fi++) {
                    TopoDS_Face f = TopoDS::Face(feB.Current());
                    TopLoc_Location loc;
                    Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(f, loc);
                    Bnd_Box bbox;
                    if (!tri.IsNull()) {
                        for (int i = 1; i <= tri->NbNodes(); i++) {
                            gp_Pnt p = tri->Node(i).Transformed(loc.Transformation());
                            bbox.Add(p);
                        }
                    }
                    double x1=0,y1=0,z1=0,x2=0,y2=0,z2=0;
                    if (!bbox.IsVoid()) bbox.Get(x1,y1,z1,x2,y2,z2);
                    fprintf(df, "Face #%-3d  %12.3f %12.3f %12.3f  %12.3f %12.3f %12.3f\n",
                            fi, x1, y1, z1, x2, y2, z2);
                }
            }
            std::map<void*,int> eid; int ne=0;
            { TopExp_Explorer ee(shape,TopAbs_EDGE); for(;ee.More();ee.Next()){void* p=ee.Current().TShape().get();if(eid.find(p)==eid.end())eid[p]=ne++;} }
            fprintf(df, "Edge IDs: [");
            for (int i=0;i<ne;i++) { if(i>0)fprintf(df,", "); fprintf(df,"%d",i); }
            fprintf(df, "]\n");
            int nb=0;
            if (shape.ShapeType() == TopAbs_SOLID) nb=1;
            else { TopExp_Explorer be(shape,TopAbs_SOLID); for(;be.More();be.Next()) nb++; }
            if (nb==0) { TopExp_Explorer be(shape,TopAbs_COMPSOLID); for(;be.More();be.Next()) nb++; }
            fprintf(df, "Body IDs: [");
            for (int i=0;i<nb;i++) { if(i>0)fprintf(df,", "); fprintf(df,"%d",i); }
            fprintf(df, "]\n");
            fprintf(df, "\nEdge → Faces:\n");
            for (auto& kv : eid) {
                void* ep = kv.first; std::set<int> fs;
                TopExp_Explorer fe2(shape,TopAbs_FACE); for(;fe2.More();fe2.Next()){TopExp_Explorer ee2(fe2.Current(),TopAbs_EDGE);for(;ee2.More();ee2.Next()){if(ee2.Current().TShape().get()==ep){fs.insert(fid[fe2.Current().TShape().get()]);break;}}}
                fprintf(df, "  Edge %d → faces [", kv.second);
                bool first=true; for(int f:fs){if(!first)fprintf(df,", ");fprintf(df,"%d",f);first=false;}
                fprintf(df, "]\n");
            }
            int nv = 0;
            { TopExp_Explorer ve(shape,TopAbs_VERTEX); for(;ve.More();ve.Next()) nv++; }
            fprintf(df, "\nVertex IDs: [");
            for (int i=0;i<nv;i++) { if(i>0)fprintf(df,", "); fprintf(df,"%d",i); }
            fprintf(df, "]\n");
            fclose(df);
        }
    }
#endif

    r.ok=true; r.elapsedMs=(int)t.elapsed();
    int renderedFaces = totalFaces - skippedFaces;
    LOG("3D",QString("Worker: faces=%1/%2 tri=%3 verts=%4 edges=%5/%6(filt=%7) %8ms")
        .arg(renderedFaces).arg(totalFaces)
        .arg(r.tris.size()/3).arg(r.verts.size())
        .arg(renderedEdges).arg(totalEdges).arg(filteredEdges)
        .arg(r.elapsedMs));
    LOG("TIME",QString("mesh=%1ms rest=%2ms total=%3ms")
        .arg(tMesh).arg(r.elapsedMs - tMesh).arg(r.elapsedMs));
    if (nonManifoldEdges > 0)
        LOG("3D",QString("WARNING: %1 non-manifold edges (yellow)").arg(nonManifoldEdges));
    if (skippedFaces > 0)
        LOG("3D",QString("WARNING: %1 faces have no triangulation").arg(skippedFaces));
    emit finished(r);
}


#endif

