#define _USE_MATH_DEFINES
#include <QtOpenGL>
#include <QtOpenGLWidgets>
#include "Model3DViewer.h"
#include "core/Logger.h"
#include <QtMath>
#include <QHBoxLayout>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector4D>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QMap>
#include <QSet>
#include <QApplication>
#include <QPainter>
#include <QThread>
#include <QTimer>
#include <set>
#include <map>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════
//  OCCT 读取线程实现（StepWorker 定义在 .h 中）
// ═══════════════════════════════════════════════════════════════
#ifdef HAS_OCC
#include <STEPControl_Reader.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
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

void StepWorker::doWork() {
    StepLoadResult r; QElapsedTimer t; t.start();
    emit progress(QString::fromUtf8("\xE8\xAF\xBB\xE5\x8F\x96 STEP..."));
    STEPControl_Reader reader;
    if (reader.ReadFile(m_path.toUtf8().constData()) != IFSelect_RetDone) { r.error="ReadFile failed"; emit finished(r); return; }
    emit progress(QString::fromUtf8("\xE8\xBD\xAC\xE6\x8D\xA2\xE5\xBD\xA2\xE7\x8A\xB6..."));
    reader.TransferRoots(); TopoDS_Shape shape = reader.OneShape();
    if (shape.IsNull()) { r.error="Shape is null"; emit finished(r); return; }
    emit progress(QString::fromUtf8("\xE4\xB8\x89\xE8\xA7\x92\xE5\x8C\x96..."));
    BRepMesh_IncrementalMesh(shape, 0.1).Perform();
    emit progress(QString::fromUtf8("\xE6\x8F\x90\xE5\x8F\x96\xE7\xBD\x91\xE6\xA0\xBC..."));
    int voff=0, faceIdx=0;
    TopExp_Explorer fExp(shape, TopAbs_FACE);
    for (; fExp.More(); fExp.Next()) {
        TopoDS_Face face = TopoDS::Face(fExp.Current()); TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;
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
    if (r.verts.isEmpty()||r.tris.isEmpty()) { r.error="No triangles"; emit finished(r); return; }
    emit progress(QString::fromUtf8("\xE7\x94\x9F\xE6\x88\x90\xE8\xBE\xB9\xE7\xBA\xBF..."));
    QVector<TopoDS_Edge> allEdges; QMap<void*,QSet<void*>> edgeFaceMap;
    { TopExp_Explorer eExp(shape, TopAbs_EDGE); for (; eExp.More(); eExp.Next()) { void* p=eExp.Current().TShape().get(); if (!edgeFaceMap.contains(p)) { edgeFaceMap[p]={}; allEdges.append(TopoDS::Edge(eExp.Current())); } } }
    { TopExp_Explorer fExp(shape, TopAbs_FACE); for (; fExp.More(); fExp.Next()) { void* fp=fExp.Current().TShape().get(); TopExp_Explorer eExp(fExp.Current(), TopAbs_EDGE); for (; eExp.More(); eExp.Next()) { void* ep=eExp.Current().TShape().get(); if (edgeFaceMap.contains(ep)) edgeFaceMap[ep].insert(fp); } } }
    for (const auto& ed : allEdges) {
        if (BRep_Tool::Degenerated(ed)) continue;
        int nf=(int)edgeFaceMap.value(ed.TShape().get()).size(); if (nf==0) continue;
        QVector3D col=(nf==1)?QVector3D(1,0.15f,0.15f):((nf==2)?QVector3D(0.15f,0.85f,0.15f):QVector3D(1,0.85f,0.1f));
        double f,l; Handle(Geom_Curve) crv=BRep_Tool::Curve(ed,f,l); if (crv.IsNull()) continue;
        int ns=36; double st=(l-f)/ns; int prev=-1;
        for (int s=0;s<=ns;s++) {
            double u=(s==ns)?l:f+s*st; gp_Pnt pt=crv->Value(u); int idx=-1;
            for (int i=0;i<r.verts.size();i++) { if (qAbs(r.verts[i].x()-pt.X())<1e-6&&qAbs(r.verts[i].y()-pt.Y())<1e-6&&qAbs(r.verts[i].z()-pt.Z())<1e-6) { idx=i; break; } }
            if (idx<0) { idx=r.verts.size(); r.verts.append(QVector3D(pt.X(),pt.Y(),pt.Z())); }
            if (r.normals.size()<r.verts.size()) r.normals.resize(r.verts.size());
            if (prev>=0) r.edges.append({prev,idx,col}); prev=idx;
        }
    }
    if (r.normals.size()<r.verts.size()) { int o=r.normals.size(); r.normals.resize(r.verts.size()); for (int i=o;i<r.verts.size();i++) r.normals[i]=QVector3D(0,1,0); }
    // 调试输出（追加到统一日志）
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

    r.ok=true; r.elapsedMs=(int)t.elapsed();
    LOG("3D",QString("Worker done: %1v %2t %3e %4ms").arg(r.verts.size()).arg(r.tris.size()/3).arg(r.edges.size()).arg(r.elapsedMs));
    emit finished(r);
}
#endif

// ═══════════════════════════════════════════════════════════════
//  GLViewer
// ═══════════════════════════════════════════════════════════════
GLViewer::GLViewer(QWidget* p):QOpenGLWidget(p){setMinimumSize(200,150);setMouseTracking(true);
    // 默认等角视角：让平面模型也能看清
    m_rot = QQuaternion::fromAxisAndAngle(QVector3D(0,1,0), -35)
          * QQuaternion::fromAxisAndAngle(QVector3D(1,0,0), -25);
}
void GLViewer::loadMesh(const QVector<QVector3D>& v,const QVector<int>& t,const QVector<QVector3D>& n,const QVector<EdgeLine>& e,const QVector<int>& fi,const QVector<QVector3D>& fc,const QVector<int>& fci,const QVector<FaceBBox>& fbb){
    m_verts=v;m_tri=t;m_normals=n;m_edges=e;m_faceIds=fi;m_faceCenters=fc;m_faceCenterIds=fci;m_faceBBoxes=fbb;
    LOG("3D",QString("Faces=%1 Centers: first=(%2,%3,%4) last=(%5,%6,%7)")
        .arg(fc.size())
        .arg(fc.size()>0?fc[0].x():0,0,'f',3).arg(fc.size()>0?fc[0].y():0,0,'f',3).arg(fc.size()>0?fc[0].z():0,0,'f',3)
        .arg(fc.size()>0?fc[fc.size()-1].x():0,0,'f',3).arg(fc.size()>0?fc[fc.size()-1].y():0,0,'f',3).arg(fc.size()>0?fc[fc.size()-1].z():0,0,'f',3));
    float mx=1e9,my=1e9,mz=1e9,Mx=-1e9,My=-1e9,Mz=-1e9;
    for(auto& vv:v){if(vv.x()<mx)mx=vv.x();if(vv.x()>Mx)Mx=vv.x();if(vv.y()<my)my=vv.y();if(vv.y()>My)My=vv.y();if(vv.z()<mz)mz=vv.z();if(vv.z()>Mz)Mz=vv.z();}
    m_modelSize=qMax(qMax(Mx-mx,My-my),Mz-mz);m_modelSize=qMax(m_modelSize,.001f);
    m_anchor=QVector3D((mx+Mx)/2,(my+My)/2,(mz+Mz)/2);m_hasAnchor=false;resetView();
    LOG("3D",QString("AABB: X=[%1,%2] Y=[%3,%4] Z=[%5,%6] size=%7")
        .arg(mx,0,'f',3).arg(Mx,0,'f',3).arg(my,0,'f',3).arg(My,0,'f',3)
        .arg(mz,0,'f',3).arg(Mz,0,'f',3).arg(m_modelSize,0,'f',3));
    // 所有面 AABB，6值排序输出
    for (const auto& b : fbb) {
        QVector<double> v = {b.minX,b.minY,b.minZ,b.maxX,b.maxY,b.maxZ};
        std::sort(v.begin(), v.end());
        LOG("3D",QString("  faceAABB: [%1, %2, %3, %4, %5, %6]")
            .arg(v[0],0,'f',3).arg(v[1],0,'f',3).arg(v[2],0,'f',3)
            .arg(v[3],0,'f',3).arg(v[4],0,'f',3).arg(v[5],0,'f',3));
    }
}
void GLViewer::resetView(){
    m_rot = QQuaternion::fromAxisAndAngle(QVector3D(0,1,0), -35)
          * QQuaternion::fromAxisAndAngle(QVector3D(1,0,0), -25);
    if(!m_verts.isEmpty()){
        QMatrix4x4 rmat;rmat.rotate(m_rot);
        float minX=1e9,minY=1e9,maxX=-1e9,maxY=-1e9;
        for(const auto& v:m_verts){
            QVector3D rv=rmat*(v-m_anchor)+m_anchor;
            if(rv.x()<minX)minX=rv.x();if(rv.x()>maxX)maxX=rv.x();
            if(rv.y()<minY)minY=rv.y();if(rv.y()>maxY)maxY=rv.y();
        }
        float cx=(minX+maxX)/2,cy=(minY+maxY)/2;
        m_panX=-cx;m_panY=-cy;
        float w=maxX-minX,h=maxY-minY;
        float as=float(width())/float(height());
        float needW=w,needH=h;
        if(as>1)needH=qMax(needH,needW/as);else needW=qMax(needW,needH*as);
        m_zoom=m_modelSize/(qMax(qMax(needW,needH),.001f));
    }else{m_zoom=1;m_panX=0;m_panY=0;}
    m_hasAnchor=false;m_pendingPick=false;update();}
void GLViewer::setHighlightFaces(const QVector<int>& ids){m_hlFaces=ids;update();}
QVector<int> GLViewer::findFacesInBox(double minX,double minY,double minZ,double maxX,double maxY,double maxZ,double eps) const {
    QVector<int> result;
    bool isPoint = (qAbs(maxX-minX) < eps && qAbs(maxY-minY) < eps && qAbs(maxZ-minZ) < eps);
    if (isPoint) {
        for (int fi=0;fi<m_faceBBoxes.size();fi++) {
            const auto& b=m_faceBBoxes[fi];
            if (minX >= b.minX-eps && minX <= b.maxX+eps &&
                minY >= b.minY-eps && minY <= b.maxY+eps &&
                minZ >= b.minZ-eps && minZ <= b.maxZ+eps)
                result.append(fi);
        }
    } else {
        int exactCnt = 0, containBest = -1;
        double bestVol = 1e30;
        QVector<int> overlapList;
        for (int fi=0;fi<m_faceBBoxes.size();fi++) {
            const auto& b=m_faceBBoxes[fi];
            bool exact = (qAbs(b.minX-minX)<=eps && qAbs(b.maxX-maxX)<=eps &&
                          qAbs(b.minY-minY)<=eps && qAbs(b.maxY-maxY)<=eps &&
                          qAbs(b.minZ-minZ)<=eps && qAbs(b.maxZ-maxZ)<=eps);
            if (exact) { result.append(fi); exactCnt++; }
            if (exactCnt==0) {
                if (minX >= b.minX-eps && maxX <= b.maxX+eps &&
                    minY >= b.minY-eps && maxY <= b.maxY+eps &&
                    minZ >= b.minZ-eps && maxZ <= b.maxZ+eps) {
                    double vol = (b.maxX-b.minX)*(b.maxY-b.minY)*(b.maxZ-b.minZ);
                    if (containBest<0 || vol<bestVol) { containBest = fi; bestVol = vol; }
                }
                if (!(minX > b.maxX+eps || maxX < b.minX-eps ||
                      minY > b.maxY+eps || maxY < b.minY-eps ||
                      minZ > b.maxZ+eps || maxZ < b.minZ-eps)) {
                    overlapList.append(fi);
                }
            }
        }
        if (exactCnt>0) { /* exact match already added */ }
        else if (containBest>=0) result.append(containBest);
        else result = overlapList;
    }
    return result;
}
QVector<int> GLViewer::findFacesByCenter(double x, double y, double z, double eps) const {
    QVector<int> result;
    for (int fi = 0; fi < m_faceCenters.size(); fi++) {
        const auto& c = m_faceCenters[fi];
        if (qAbs(c.x() - x) < eps && qAbs(c.y() - y) < eps && qAbs(c.z() - z) < eps) {
            int id = (fi < m_faceCenterIds.size()) ? m_faceCenterIds[fi] : fi;
            result.append(id);
        }
    }
    return result;
}
void GLViewer::setShowFaceIds(bool show){m_showFaceIds=show;update();}
void GLViewer::clear(){m_verts.clear();m_tri.clear();m_normals.clear();m_edges.clear();m_faceIds.clear();m_faceCenters.clear();m_faceCenterIds.clear();m_faceBBoxes.clear();m_hlFaces.clear();update();}
void GLViewer::initializeGL(){initializeOpenGLFunctions();glClearColor(.18f,.18f,.22f,1);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_LIGHT1);glEnable(GL_NORMALIZE);
    GLfloat a0[]={.4f,.4f,.45f,1};glLightfv(GL_LIGHT0,GL_AMBIENT,a0);GLfloat d0[]={.6f,.6f,.7f,1};glLightfv(GL_LIGHT0,GL_DIFFUSE,d0);GLfloat s0[]={.2f,.2f,.2f,1};glLightfv(GL_LIGHT0,GL_SPECULAR,s0);
    GLfloat a1[]={.15f,.15f,.2f,1};glLightfv(GL_LIGHT1,GL_AMBIENT,a1);GLfloat d1[]={.3f,.3f,.4f,1};glLightfv(GL_LIGHT1,GL_DIFFUSE,d1);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);glEnable(GL_COLOR_MATERIAL);}
void GLViewer::resizeGL(int w,int h){glViewport(0,0,w,h);}
void GLViewer::paintGL(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // 渐变背景：深蓝→深紫（不写深度，避免污染模型深度测试）
    glDepthMask(GL_FALSE);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,1,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glDisable(GL_LIGHTING);glBegin(GL_QUADS);
    glColor3f(.08f,.1f,.18f);glVertex2f(0,0);glVertex2f(1,0);
    glColor3f(.12f,.08f,.18f);glVertex2f(1,1);glVertex2f(0,1);
    glEnd();glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);

    if(m_verts.isEmpty())return;
    float as=float(width())/float(height()),sz=m_modelSize*.6f/qMax(m_zoom,.01f);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    if(as>1)glOrtho(-sz*as,sz*as,-sz,sz,-1e5,1e5);else glOrtho(-sz,sz,-sz/as,sz/as,-1e5,1e5);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(m_panX,m_panY,0);glTranslatef(m_anchor.x(),m_anchor.y(),m_anchor.z());
    QMatrix4x4 rmat;rmat.rotate(m_rot);glMultMatrixf(rmat.constData());glTranslatef(-m_anchor.x(),-m_anchor.y(),-m_anchor.z());
    GLfloat lp0[]={1,1,1,0};glLightfv(GL_LIGHT0,GL_POSITION,lp0);
    GLfloat lp1[]={-1,-1,-.5f,0};glLightfv(GL_LIGHT1,GL_POSITION,lp1);
    GLfloat mv[16],pj[16];GLint vp[4];
    glGetFloatv(GL_MODELVIEW_MATRIX,mv);glGetFloatv(GL_PROJECTION_MATRIX,pj);glGetIntegerv(GL_VIEWPORT,vp);
    QMatrix4x4 mvMat((const float*)mv), pjMat((const float*)pj);
    glEnable(GL_LIGHTING);
    if(!m_tri.isEmpty()){
        glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);
        float* va=new float[m_verts.size()*3];float* na=new float[m_normals.size()*3];
        for(int i=0;i<m_verts.size();i++){va[i*3]=m_verts[i].x();va[i*3+1]=m_verts[i].y();va[i*3+2]=m_verts[i].z();
            if(i<m_normals.size()){na[i*3]=m_normals[i].x();na[i*3+1]=m_normals[i].y();na[i*3+2]=m_normals[i].z();}else{na[i*3]=0;na[i*3+1]=1;na[i*3+2]=0;}}
        glVertexPointer(3,GL_FLOAT,0,va);glNormalPointer(GL_FLOAT,0,na);
        if (m_showFaceIds) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        glColor4f(.75f,.80f,.88f, m_showFaceIds ? .35f : 1.f);
        glDrawElements(GL_TRIANGLES,m_tri.size(),GL_UNSIGNED_INT,m_tri.data());
        if (m_showFaceIds) glDisable(GL_BLEND);
        glDisableClientState(GL_NORMAL_ARRAY);glDisableClientState(GL_VERTEX_ARRAY);delete[]va;delete[]na;
    }
    // 高亮面（半透明黄色填充，用于属性高亮）
    if(!m_hlFaces.isEmpty()&&!m_tri.isEmpty()){
        QSet<int> hlFaceSet(m_hlFaces.begin(),m_hlFaces.end());
        QVector<int> hlTri; hlTri.reserve(m_tri.size());
        for(int ti=0;ti<m_tri.size()/3;ti++) if(ti<m_faceIds.size()&&hlFaceSet.contains(m_faceIds[ti])){hlTri.append(m_tri[ti*3]);hlTri.append(m_tri[ti*3+1]);hlTri.append(m_tri[ti*3+2]);}
        if(!hlTri.isEmpty()){
            glEnableClientState(GL_VERTEX_ARRAY);float* va2=new float[m_verts.size()*3];
            for(int i=0;i<m_verts.size();i++){va2[i*3]=m_verts[i].x();va2[i*3+1]=m_verts[i].y();va2[i*3+2]=m_verts[i].z();}
            glVertexPointer(3,GL_FLOAT,0,va2);
            glDisable(GL_LIGHTING);glColor3f(1,.85f,.1f);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-10,-10);
            glDrawElements(GL_TRIANGLES,hlTri.size(),GL_UNSIGNED_INT,hlTri.data());
            glDisable(GL_POLYGON_OFFSET_FILL);glEnable(GL_LIGHTING);
            glDisableClientState(GL_VERTEX_ARRAY);delete[]va2;
        }
    }
    glDisable(GL_LIGHTING);
    if(!m_edges.isEmpty()){
        glDisable(GL_DEPTH_TEST);
        glEnableClientState(GL_VERTEX_ARRAY);float* ea=new float[m_verts.size()*3];
        for(int i=0;i<m_verts.size();i++){ea[i*3]=m_verts[i].x();ea[i*3+1]=m_verts[i].y();ea[i*3+2]=m_verts[i].z();}
        glVertexPointer(3,GL_FLOAT,0,ea);glLineWidth(2);
        for(const auto& e:m_edges){int idx[2]={e.v0,e.v1};glColor3f(e.color.x(),e.color.y(),e.color.z());glDrawElements(GL_LINES,2,GL_UNSIGNED_INT,idx);}
        glDisableClientState(GL_VERTEX_ARRAY);delete[]ea;
        glEnable(GL_DEPTH_TEST);
    }
}


void GLViewer::mousePressEvent(QMouseEvent* e){m_lastPos=e->pos();m_dragging=true;}
void GLViewer::mouseMoveEvent(QMouseEvent* e){if(!m_dragging)return;float dx=e->position().x()-m_lastPos.x(),dy=e->position().y()-m_lastPos.y();if(e->buttons()&Qt::LeftButton){QQuaternion dq=QQuaternion::fromAxisAndAngle(QVector3D(0,1,0),dx*.4f)*QQuaternion::fromAxisAndAngle(QVector3D(1,0,0),dy*.4f);m_rot=dq*m_rot;m_rot.normalize();}else if(e->buttons()&Qt::MiddleButton){m_panX+=dx*.005f*m_modelSize/m_zoom;m_panY-=dy*.005f*m_modelSize/m_zoom;}m_lastPos=e->pos();update();}
void GLViewer::wheelEvent(QWheelEvent* e){if(e->angleDelta().y()>0)m_zoom=qMin(m_zoom*1.15f,100.f);else m_zoom=qMax(m_zoom*.85f,.01f);update();}

QImage GLViewer::grabScreenshot() const {
    // Qt 6: QOpenGLWidget::grab() returns QPixmap
    return const_cast<GLViewer*>(this)->grab().toImage();
}

// ═══════════════════════════════════════════════════════════════
//  快速软件截图（CPU 光栅化，无 OpenGL）
// ═══════════════════════════════════════════════════════════════

// 软件光栅化：将三角形网格渲染为 QImage
static QImage rasterizeTriangles(const QVector<QVector3D>& verts,
                                  const QVector<int>& tris,
                                  const QVector<QVector3D>& normals,
                                  int width, int height)
{
    if (verts.isEmpty() || tris.size() < 3) return {};

    // 计算模型中心
    double cx = 0, cy = 0, cz = 0;
    for (const auto& v : verts) { cx += v.x(); cy += v.y(); cz += v.z(); }
    int n = verts.size();
    cx /= n; cy /= n; cz /= n;

    // 固定视角旋转（参考 CADShoter: Y-35°, X-25°）
    QMatrix4x4 rotMat;
    rotMat.rotate(-35, 0, 1, 0);  // Y 轴旋转
    rotMat.rotate(-25, 1, 0, 0);  // X 轴旋转

    // 投影所有顶点
    QVector<QVector3D> proj;
    proj.reserve(verts.size());
    double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
    for (const auto& v : verts) {
        QVector3D c = v - QVector3D(cx, cy, cz);
        QVector3D r = rotMat * c;
        proj.append(r);
        if (r.x() < minX) minX = r.x(); if (r.x() > maxX) maxX = r.x();
        if (r.y() < minY) minY = r.y(); if (r.y() > maxY) maxY = r.y();
    }

    // 缩放到图像尺寸（留 10% 边距）
    double range = qMax(maxX - minX, maxY - minY);
    if (range < 0.001) return {};
    double scale = qMin(width, height) * 0.85 / range;
    double offX = width / 2.0;
    double offY = height / 2.0;

    // 光方向（与 CADShoter 一致）
    QVector3D lightDir(0.4f, 0.8f, 0.5f);
    lightDir.normalize();

    // 构建三角形列表（含深度和亮度）
    struct TriData {
        QPolygonF poly;
        double depth;
        double brightness;
    };
    QVector<TriData> triData;
    triData.reserve(tris.size() / 3);

    for (int i = 0; i < tris.size() / 3; i++) {
        int i0 = tris[i * 3], i1 = tris[i * 3 + 1], i2 = tris[i * 3 + 2];
        if (i0 >= proj.size() || i1 >= proj.size() || i2 >= proj.size()) continue;

        QPolygonF poly;
        poly << QPointF(proj[i0].x() * scale + offX, proj[i0].y() * scale + offY)
             << QPointF(proj[i1].x() * scale + offX, proj[i1].y() * scale + offY)
             << QPointF(proj[i2].x() * scale + offX, proj[i2].y() * scale + offY);

        double depth = (proj[i0].z() + proj[i1].z() + proj[i2].z()) / 3.0;

        // 计算面法线（从投影后的顶点）
        QVector3D e1 = proj[i1] - proj[i0];
        QVector3D e2 = proj[i2] - proj[i0];
        QVector3D normal = QVector3D::crossProduct(e1, e2);
        if (normal.length() < 1e-10) continue;
        normal.normalize();

        // Lambertian 漫反射
        double dot = QVector3D::dotProduct(normal, lightDir);
        double brightness = qBound(0.35, 0.5 + 0.5 * dot, 1.0);

        triData.append({poly, depth, brightness});
    }

    if (triData.isEmpty()) return {};

    // Painter's algorithm：从远到近排序
    std::sort(triData.begin(), triData.end(), [](const TriData& a, const TriData& b) {
        return a.depth > b.depth;
    });

    // 渲染
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(QColor(20, 25, 46));  // 深色渐变背景
    QPainter painter(&img);

    // 渐变背景
    QLinearGradient bgGrad(0, 0, 0, height);
    bgGrad.setColorAt(0, QColor(20, 25, 46));
    bgGrad.setColorAt(1, QColor(30, 20, 46));
    painter.fillRect(QRect(0, 0, width, height), bgGrad);

    painter.setRenderHint(QPainter::Antialiasing, false);

    for (const auto& td : triData) {
        int g = qBound(0, (int)(td.brightness * 255), 255);
        QColor fill(
            qBound(0, (int)(g * 0.55), 255),
            qBound(0, (int)(g * 0.62), 255),
            qBound(0, (int)(g * 0.72), 255));
        painter.setBrush(fill);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(td.poly);
    }
    painter.end();
    return img;
}

// ═══════════════════════════════════════════════════════════════
//  NAS (Nastran) 网格文件解析器（纯文本，无需 OCCT）
// ═══════════════════════════════════════════════════════════════

static StepLoadResult parseNasFile(const QString& filePath)
{
    StepLoadResult r;
    QElapsedTimer t; t.start();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        r.error = "Cannot open NAS file";
        return r;
    }

    struct Node { double x, y, z; };
    QMap<int, Node> nodes;
    struct TriElem { int g1, g2, g3; };
    QVector<TriElem> tris;
    QSet<QPair<int,int>> wireEdges;

    // 辅助：添加三角形并记录边
    auto addTri = [&](int a, int b, int c) {
        tris.append({a, b, c});
        wireEdges.insert(qMakePair(qMin(a,b), qMax(a,b)));
        wireEdges.insert(qMakePair(qMin(b,c), qMax(b,c)));
        wireEdges.insert(qMakePair(qMin(c,a), qMax(c,a)));
    };
    // 添加三角形（不生成边线，用于四边形内部分割）
    auto addTriNoEdge = [&](int a, int b, int c) {
        tris.append({a, b, c});
    };
    // 辅助：将四边形拆成两个三角形，只添加外部 4 条轮廓边
    auto addQuad = [&](int a, int b, int c, int d) {
        addTriNoEdge(a, b, c);
        addTriNoEdge(a, c, d);
        wireEdges.insert(qMakePair(qMin(a,b), qMax(a,b)));
        wireEdges.insert(qMakePair(qMin(b,c), qMax(b,c)));
        wireEdges.insert(qMakePair(qMin(c,d), qMax(c,d)));
        wireEdges.insert(qMakePair(qMin(d,a), qMax(d,a)));
    };

    // 续行处理：累积当前卡片的所有字段
    QStringList cardParts;
    QString cardLine;  // 保存原始行用于固定宽度解析
    bool inCard = false;
    QSet<QString> unsupportedCards;

    auto finishCard = [&]() {
        if (!inCard) return;
        inCard = false;
        if (cardParts.size() < 2) { cardParts.clear(); return; }
        QString card = cardParts[0].toUpper();

        if (card == "GRID" || card == "GRID*") {
            // GRID: ID CP X Y Z — 支持坐标字段粘连（无空格分隔）
            bool ok; int id = cardParts[1].toInt(&ok);
            if (!ok) { cardParts.clear(); return; }
            double x=0, y=0, z=0;
            if (cardParts.size() >= 5) {
                x = cardParts[2].toDouble();
                y = cardParts[3].toDouble();
                z = cardParts[4].toDouble();
            } else {
                // 坐标字段粘连：从 ID 后所有 token 拼起来，按 [+-]?\d+\.?\d* 模式切分
                QString coords;
                for (int i = 2; i < cardParts.size(); i++)
                    coords += cardParts[i];
                static QRegularExpression numRe(R"([+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)");
                QVector<double> vals;
                auto it = numRe.globalMatch(coords);
                while (it.hasNext() && vals.size() < 3) {
                    vals.append(it.next().captured(0).toDouble());
                }
                if (vals.size() >= 1) x = vals[0];
                if (vals.size() >= 2) y = vals[1];
                if (vals.size() >= 3) z = vals[2];
            }
            if (ok) {
                nodes[id] = {x, y, z};
            }
        } else if (card == "CTRIA3" || card == "CTRIA3*" || card == "CTRIAR") {
            // CTRIA3: EID PID G1 G2 G3
            if (cardParts.size() >= 6) {
                int g1 = cardParts[3].toInt(), g2 = cardParts[4].toInt(), g3 = cardParts[5].toInt();
                addTri(g1, g2, g3);
            }
        } else if (card == "CQUAD4" || card == "CQUAD4*" || card == "CQUADR") {
            // CQUAD4: EID PID G1 G2 G3 G4
            if (cardParts.size() >= 7) {
                int g1 = cardParts[3].toInt(), g2 = cardParts[4].toInt();
                int g3 = cardParts[5].toInt(), g4 = cardParts[6].toInt();
                addQuad(g1, g2, g3, g4);
            }
        } else if (card == "CTRIA6" || card == "CTRIA6*") {
            // CTRIA6: EID PID G1 G2 G3 G4 G5 G6  (G4=mid G1-G2, G5=mid G2-G3, G6=mid G3-G1)
            // 渲染时只用角节点 G1 G2 G3（忽略中节点）
            if (cardParts.size() >= 6) {
                int g1 = cardParts[3].toInt(), g2 = cardParts[4].toInt(), g3 = cardParts[5].toInt();
                addTri(g1, g2, g3);
            }
        } else if (card == "CQUAD8" || card == "CQUAD8*") {
            // CQUAD8: EID PID G1 G2 G3 G4 G5 G6 G7 G8  (G5..G8=mid-side)
            // 渲染时只用角节点 G1 G2 G3 G4
            if (cardParts.size() >= 7) {
                int g1 = cardParts[3].toInt(), g2 = cardParts[4].toInt();
                int g3 = cardParts[5].toInt(), g4 = cardParts[6].toInt();
                addQuad(g1, g2, g3, g4);
            }
        } else if (card == "CTETRA" || card == "CTETRA*") {
            // CTETRA: EID PID G1 G2 G3 G4 — 四面体，提取4个三角面
            if (cardParts.size() >= 7) {
                int g1 = cardParts[3].toInt(), g2 = cardParts[4].toInt();
                int g3 = cardParts[5].toInt(), g4 = cardParts[6].toInt();
                addTri(g1, g2, g3);
                addTri(g1, g2, g4);
                addTri(g2, g3, g4);
                addTri(g1, g3, g4);
            }
        } else if (card == "CPENTA" || card == "CPENTA*") {
            // CPENTA: EID PID G1 G2 G3 G4 G5 G6 — 五面体（楔形），提取外表面
            if (cardParts.size() >= 9) {
                int g1 = cardParts[3].toInt(), g2 = cardParts[4].toInt(), g3 = cardParts[5].toInt();
                int g4 = cardParts[6].toInt(), g5 = cardParts[7].toInt(), g6 = cardParts[8].toInt();
                // 两个三角端面
                addTri(g1, g2, g3);
                addTri(g4, g5, g6);
                // 三个四边形侧面
                addQuad(g1, g2, g5, g4);
                addQuad(g2, g3, g6, g5);
                addQuad(g3, g1, g4, g6);
            }
        } else if (card == "CHEXA" || card == "CHEXA*") {
            // CHEXA: EID PID G1 G2 G3 G4 G5 G6 G7 G8 — 六面体，提取6个外表面
            if (cardParts.size() >= 11) {
                int g1 = cardParts[3].toInt(),  g2 = cardParts[4].toInt();
                int g3 = cardParts[5].toInt(),  g4 = cardParts[6].toInt();
                int g5 = cardParts[7].toInt(),  g6 = cardParts[8].toInt();
                int g7 = cardParts[9].toInt(),  g8 = cardParts[10].toInt();
                // 六个面，每个四边形拆两个三角
                addQuad(g1, g2, g3, g4);  // 底面
                addQuad(g5, g6, g7, g8);  // 顶面
                addQuad(g1, g2, g6, g5);  // 前面
                addQuad(g2, g3, g7, g6);  // 右面
                addQuad(g3, g4, g8, g7);  // 后面
                addQuad(g4, g1, g5, g8);  // 左面
            }
        } else if (card == "CPYRAM" || card == "CPYRAM*") {
            // CPYRAM: EID PID G1 G2 G3 G4 G5 — 金字塔（五面体）
            if (cardParts.size() >= 8) {
                int g1 = cardParts[3].toInt(), g2 = cardParts[4].toInt();
                int g3 = cardParts[5].toInt(), g4 = cardParts[6].toInt();
                int g5 = cardParts[7].toInt();
                addQuad(g1, g2, g3, g4);  // 底面
                addTri(g1, g2, g5);
                addTri(g2, g3, g5);
                addTri(g3, g4, g5);
                addTri(g4, g1, g5);
            }
        } else {
            // 记录未支持的卡片类型（仅记一次）
            unsupportedCards.insert(card);
        }
        cardParts.clear();
        cardLine.clear();
    };

    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('$'))
            continue;
        if (trimmed.startsWith("BEGIN", Qt::CaseInsensitive) ||
            trimmed.startsWith("ENDDATA", Qt::CaseInsensitive) ||
            trimmed.startsWith("NASTRAN", Qt::CaseInsensitive))
            continue;

        // 续行处理：以 '+' 开头或纯数字开头（NAS 小域格式续行特征）
        bool isContinuation = trimmed.startsWith('+') ||
            (trimmed.size() > 0 && trimmed[0].isDigit() && inCard && cardParts.size() >= 1);

        QStringList parts = trimmed.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        if (isContinuation) {
            // 续行：去掉 '+' 标记，剩余数据追加到当前卡片
            int startIdx = (parts[0] == "+") ? 1 : 0;
            for (int i = startIdx; i < parts.size(); i++)
                cardParts.append(parts[i]);
        } else {
            // 新卡片 → 完成旧卡片，开始新卡片
            finishCard();
            cardParts = parts;
            cardLine = line;
            inCard = true;
        }
    }
    finishCard();  // 处理最后一张卡片
    f.close();

    // 日志：未知卡片类型
    if (!unsupportedCards.isEmpty()) {
        QStringList sorted = unsupportedCards.values();
        sorted.sort();
        LOG("3D", "NAS unsupported cards: " + sorted.join(", "));
    }

    if (nodes.isEmpty()) { r.error = "No GRID nodes"; return r; }
    if (tris.isEmpty()) {
        // 有节点但无单元 — 可能文件只有点云
        r.error = "No surface/solid elements found (CTRIA3/CQUAD4/CTETRA/CHEXA/...)";
        return r;
    }

    QMap<int,int> idToIdx;
    for (auto it = nodes.begin(); it != nodes.end(); ++it)
        idToIdx[it.key()] = idToIdx.size();

    r.verts.resize(idToIdx.size());
    for (auto it = nodes.begin(); it != nodes.end(); ++it)
        r.verts[idToIdx[it.key()]] = QVector3D(it.value().x, it.value().y, it.value().z);

    r.tris.reserve(tris.size() * 3);
    for (const auto& tri : tris) {
        if (!idToIdx.contains(tri.g1) || !idToIdx.contains(tri.g2) || !idToIdx.contains(tri.g3))
            continue;
        r.tris.append(idToIdx[tri.g1]);
        r.tris.append(idToIdx[tri.g2]);
        r.tris.append(idToIdx[tri.g3]);
    }
    if (r.tris.size() < 3) { r.error = "No valid triangles after remap"; return r; }

    r.normals.resize(r.verts.size());
    for (int i = 0; i < r.tris.size(); i += 3) {
        QVector3D n = QVector3D::crossProduct(
            r.verts[r.tris[i+1]] - r.verts[r.tris[i]],
            r.verts[r.tris[i+2]] - r.verts[r.tris[i]]);
        float l = n.length();
        if (l > 1e-10f) n /= l;
        r.normals[r.tris[i]]   += n;
        r.normals[r.tris[i+1]] += n;
        r.normals[r.tris[i+2]] += n;
    }
    for (auto& n : r.normals) { float l = n.length(); if (l > 1e-10f) n /= l; else n = QVector3D(0, 1, 0); }

    float mx=1e9f,my=1e9f,mz=1e9f,Mx=-1e9f,My=-1e9f,Mz=-1e9f;
    for (const auto& v : r.verts) {
        if (v.x()<mx) mx=v.x(); if (v.x()>Mx) Mx=v.x();
        if (v.y()<my) my=v.y(); if (v.y()>My) My=v.y();
        if (v.z()<mz) mz=v.z(); if (v.z()>Mz) Mz=v.z();
    }
    QVector3D center(0,0,0);
    for (const auto& v : r.verts) center += v;
    if (!r.verts.isEmpty()) center /= r.verts.size();
    r.faceCenterIds.append(0);
    r.faceCenters.append(center);
    r.faceBBoxes.append({mx, my, mz, Mx, My, Mz});
    r.faceIds.resize(r.tris.size() / 3, 0);

    QVector3D wireColor(0.2f, 0.85f, 0.25f);
    for (const auto& e : wireEdges) {
        int v0 = idToIdx.value(e.first, -1);
        int v1 = idToIdx.value(e.second, -1);
        if (v0 >= 0 && v1 >= 0)
            r.edges.append({v0, v1, wireColor});
    }

    r.ok = true;
    r.elapsedMs = (int)t.elapsed();
    LOG("3D", QString("NAS parsed: %1v %2t %3ms")
        .arg(r.verts.size()).arg(r.tris.size()/3).arg(r.elapsedMs));
    return r;
}

// 在子线程中读取 STEP 文件（带超时保护）
static StepLoadResult readStepFileWithTimeout(const QString& filePath, int timeoutMs)
{
    // .nas 文件直接解析，无需 OCCT
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "nas" || ext == "bdf" || ext == "dat") {
        return parseNasFile(filePath);
    }

    StepLoadResult result;

#ifdef HAS_OCC
    QThread workerThread;
    StepWorker worker(filePath);
    worker.moveToThread(&workerThread);

    bool done = false;
    QObject::connect(&workerThread, &QThread::started, &worker, &StepWorker::doWork);
    QObject::connect(&worker, &StepWorker::finished, [&](const StepLoadResult& r) {
        result = r;
        done = true;
        workerThread.quit();
    });

    // 超时定时器
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
        workerThread.requestInterruption();
        workerThread.quit();
        done = true;
    });

    workerThread.start();
    timeoutTimer.start(timeoutMs);

    // 等待完成或超时
    QEventLoop loop;
    QObject::connect(&workerThread, &QThread::finished, &loop, &QEventLoop::quit);
    if (!done) loop.exec();

    if (!workerThread.isFinished()) {
        workerThread.requestInterruption();
        workerThread.wait(1000);
        if (!workerThread.isFinished()) workerThread.terminate();
    }
#else
    Q_UNUSED(filePath)
    Q_UNUSED(timeoutMs)
#endif

    return result;
}

QImage Model3DViewer::renderModelScreenshot(const QString& filePath,
                                             int width, int height,
                                             int timeoutMs)
{
    if (!QFile::exists(filePath)) return {};

    // 读取模型文件（NAS 直接解析，STEP/IGES/BREP 走 OCCT 子线程 + 超时）
    auto result = readStepFileWithTimeout(filePath, timeoutMs);
    if (!result.ok || result.verts.isEmpty() || result.tris.size() < 3) return {};

    // 软件光栅化
    return rasterizeTriangles(result.verts, result.tris, result.normals, width, height);
}

// ═══════════════════════════════════════════════════════════════
//  Model3DViewer
// ═══════════════════════════════════════════════════════════════
Model3DViewer::Model3DViewer(QWidget* p):QWidget(p){
    auto*l=new QVBoxLayout(this);l->setContentsMargins(0,0,0,0);l->setSpacing(2);
    m_gl=new GLViewer(this);l->addWidget(m_gl,1);
    auto*br=new QHBoxLayout();br->setSpacing(4);br->setContentsMargins(4,2,4,2);
    m_btnReset=new QPushButton(QString::fromUtf8("\xE2\x86\xBB"),this);
    m_btnReset->setFixedSize(28,28);m_btnReset->setToolTip(QString::fromUtf8("\xE5\xA4\x8D\xE4\xBD\x8D\xE8\xA7\x86\xE8\xA7\x92"));
    m_btnReset->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;font-size:16px;padding:0;}QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}");
    connect(m_btnReset,&QPushButton::clicked,m_gl,&GLViewer::resetView);
    m_btnShowFaceIds=new QPushButton(QString::fromUtf8("\xE2\x96\xA3"),this);
    m_btnShowFaceIds->setFixedSize(28,28);m_btnShowFaceIds->setCheckable(true);
    m_btnShowFaceIds->setToolTip(QString::fromUtf8("\xE6\x98\xBE\xE7\xA4\xBA\xE9\x9D\xA2 ID"));
    m_btnShowFaceIds->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;font-size:16px;padding:0;}QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}QPushButton:checked{background:#eef2ff;border-color:#6366f1;color:#6366f1;}");
    connect(m_btnShowFaceIds,&QPushButton::toggled,this,&Model3DViewer::toggleFaceIds);
    br->addWidget(m_btnReset);br->addWidget(m_btnShowFaceIds);br->addStretch();l->addLayout(br);
    m_status=new QLabel(QString::fromUtf8("\xE6\x9C\xAA\xE5\x8A\xA0\xE8\xBD\xBD\xE6\xA8\xA1\xE5\x9E\x8B"),this);
    m_status->setAlignment(Qt::AlignLeft|Qt::AlignTop);m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#64748b;font-size:12px;padding:8px;background:#f8f9fb;border:1px solid #e2e8f0;border-radius:6px;");m_status->setMinimumHeight(36);l->addWidget(m_status);
    m_timeoutTimer=new QTimer(this);m_timeoutTimer->setSingleShot(true);
    m_countdownTimer=new QTimer(this);connect(m_countdownTimer,&QTimer::timeout,this,[this](){updateCountdown();});
}
Model3DViewer::~Model3DViewer(){cancelLoad();}
void Model3DViewer::updateCountdown(){m_remainingSeconds--;if(m_remainingSeconds>0)m_status->setText(QString::fromUtf8("\xE5\x8A\xA0\xE8\xBD\xBD\xE4\xB8\xAD... %1s").arg(m_remainingSeconds));}
void Model3DViewer::cancelLoad(){m_countdownTimer->stop();m_timeoutTimer->stop();
    if(m_workerThread){m_workerThread->requestInterruption();m_workerThread->quit();m_workerThread->wait(2000);}
    if(m_worker){m_worker->deleteLater();m_worker=nullptr;}m_workerThread=nullptr;}
void Model3DViewer::loadFile(const QString& fp){
    cancelLoad();m_gl->clear();
    m_pendingBoxesMap.clear();
    if (!QFile::exists(fp)) {
        LOG("3D", "File not found: " + fp);
        m_status->setText(QString::fromUtf8("\xe6\x96\x87\xe4\xbb\xb6\xe4\xb8\x8d\xe5\xad\x98\xe5\x9c\xa8: ") + fp);
        m_status->setStyleSheet("color:#ef4444;font-size:12px;padding:8px;background:#fef2f2;border:1px solid #fecaca;border-radius:6px;");
        return;
    }

    // .nas 快速解析显示（直接返回，不入 OCCT 线程）
    QString ext = QFileInfo(fp).suffix().toLower();
    if (ext == "nas" || ext == "bdf" || ext == "dat") {
        LOG("3D","Load NAS: "+fp);
        StepLoadResult r = parseNasFile(fp);
        if (r.ok) {
            m_gl->loadMesh(r.verts,r.tris,r.normals,r.edges,r.faceIds,r.faceCenters,r.faceCenterIds,r.faceBBoxes);
            m_status->setText(QString("NAS: %1v %2t").arg(r.verts.size()).arg(r.tris.size()/3));
            m_status->setStyleSheet("color:#10b981;font-size:12px;padding:8px;background:#f0fdf4;border:1px solid #d1fae5;border-radius:6px;");
        } else {
            m_status->setText(r.error);
            m_status->setStyleSheet("color:#ef4444;font-size:12px;padding:8px;background:#fef2f2;border:1px solid #fecaca;border-radius:6px;");
        }
        emit modelLoaded();
        return;
    }

    LOG("3D","Load: "+fp);m_status->setText(QString::fromUtf8("\xE5\x8A\xA0\xE8\xBD\xBD\xE4\xB8\xAD..."));
#ifndef HAS_OCC
    m_status->setText("OCCT not available");LOG("3D","OCCT not available");return;
#endif
    m_worker=new StepWorker(fp);m_workerThread=new QThread(this);m_worker->moveToThread(m_workerThread);
    connect(m_workerThread,&QThread::started,m_worker,&StepWorker::doWork);
    connect(m_worker,&StepWorker::progress,this,[this](const QString& t){m_status->setText(t);});
    connect(m_worker,&StepWorker::finished,this,[this](const StepLoadResult& r){
        m_countdownTimer->stop();m_timeoutTimer->stop();
        if(r.ok){LOG("3D",QString("OK %1v %2t %3e %4ms").arg(r.verts.size()).arg(r.tris.size()/3).arg(r.edges.size()).arg(r.elapsedMs));
            m_gl->loadMesh(r.verts,r.tris,r.normals,r.edges,r.faceIds,r.faceCenters,r.faceCenterIds,r.faceBBoxes);
            applyPendingBoxes();
            m_status->setText(QString("OCCT: %1v %2t %3e").arg(r.verts.size()).arg(r.tris.size()/3).arg(r.edges.size()));
            m_status->setStyleSheet("color:#10b981;font-size:12px;padding:8px;background:#f0fdf4;border:1px solid #d1fae5;border-radius:6px;");
            emit modelLoaded();}
        else{m_status->setText(r.error);m_status->setStyleSheet("color:#ef4444;font-size:12px;padding:8px;background:#fef2f2;border:1px solid #fecaca;border-radius:6px;");LOG("3D","FAIL: "+r.error);emit modelLoaded();}
        if(m_workerThread){m_workerThread->quit();m_workerThread->wait();m_workerThread=nullptr;}if(m_worker){m_worker->deleteLater();m_worker=nullptr;}
    });
    connect(m_workerThread,&QThread::finished,this,[this](){if(m_worker){m_worker->deleteLater();m_worker=nullptr;}});
    connect(m_timeoutTimer,&QTimer::timeout,this,[this](){
        LOG("3D","TIMEOUT 30s");m_countdownTimer->stop();
        if(m_workerThread){m_workerThread->requestInterruption();m_workerThread->quit();m_workerThread->wait(1000);m_workerThread=nullptr;}
        if(m_worker){m_worker->deleteLater();m_worker=nullptr;}
        m_status->setText(QString::fromUtf8("\xE8\xB6\x85\xE6\x97\xB6\xEF\xBC\x88")+"30s\xEF\xBC\x89");m_status->setStyleSheet("color:#ef4444;font-size:12px;padding:8px;background:#fef2f2;border:1px solid #fecaca;border-radius:6px;");
    });
    m_remainingSeconds=30;m_status->setText(QString::fromUtf8("\xE5\x8A\xA0\xE8\xBD\xBD\xE4\xB8\xAD... 30s"));
    m_countdownTimer->start(1000);m_timeoutTimer->start(30000);m_workerThread->start();
}
void Model3DViewer::clear(){cancelLoad();if(m_worker){m_worker->deleteLater();m_worker=nullptr;}m_pendingBoxesMap.clear();m_gl->clear();}
void Model3DViewer::highlightFaces(const QVector<int>& ids){m_gl->setHighlightFaces(ids);}
void Model3DViewer::highlightFacesInBoxes(const QVector<QVector<double>>& boxes, bool on){
    highlightFacesInBoxes(QString(), boxes, on);
}
void Model3DViewer::highlightFacesInBoxes(const QString& propKey, const QVector<QVector<double>>& boxes, bool on){
    if (!on) {
        m_pendingBoxesMap.remove(propKey);
        if (m_pendingBoxesMap.isEmpty()) m_gl->setHighlightFaces({});
        return;
    }
    m_pendingBoxesMap[propKey] = boxes;
    // 模型还没加载时不解析，保持"解析中..."不变
    if (m_gl->faceBBoxCount() == 0 && !propKey.isEmpty()) return;
    // 输出XML包围盒，6值排序
    for (const auto& b : boxes) {
        if (b.size() < 6) continue;
        QVector<double> v = {b[0],b[1],b[2],b[3],b[4],b[5]};
        std::sort(v.begin(), v.end());
        LOG("BOX",QString("  xmlBox: [%1, %2, %3, %4, %5, %6]")
            .arg(v[0],0,'f',3).arg(v[1],0,'f',3).arg(v[2],0,'f',3)
            .arg(v[3],0,'f',3).arg(v[4],0,'f',3).arg(v[5],0,'f',3));
    }
    QSet<int> allIds;
    QStringList faceParts, pointParts;
    double eps = 0.01;
    int matchedBoxCount = 0, totalBoxCount = 0;
    QVector<QVector<double>> unmatchedBoxes;
    for (const auto& box : boxes) {
        if (box.size() < 6) { totalBoxCount++; continue; }
        bool isPoint = (qAbs(box[3]-box[0]) < eps && qAbs(box[4]-box[1]) < eps && qAbs(box[5]-box[2]) < eps);
        auto ids = isPoint ? m_gl->findFacesByCenter(box[0], box[1], box[2]) : m_gl->findFacesInBox(box[0], box[1], box[2], box[3], box[4], box[5]);
        totalBoxCount++;
        if (!ids.isEmpty()) matchedBoxCount++;
        else unmatchedBoxes.append(box);
        for (int id : ids) allIds.insert(id);
        QStringList idStrs;
        for (int id : ids) idStrs << QString::number(id);
        if (isPoint && !ids.isEmpty())
            pointParts << idStrs.join(",");
        else if (!ids.isEmpty())
            faceParts << idStrs.join(",");
    }
    QVector<int> ids(allIds.begin(), allIds.end());
    m_gl->setHighlightFaces(ids);
    LOG("BOX",QString("%1: %2/%3 boxes matched, %4 unique face IDs")
        .arg(propKey.isEmpty()?QString("anon"):propKey)
        .arg(matchedBoxCount).arg(totalBoxCount).arg(ids.size()));
    if (!unmatchedBoxes.isEmpty()) {
        for (const auto& b : unmatchedBoxes) {
            QVector<double> v = {b[0],b[1],b[2],b[3],b[4],b[5]};
            std::sort(v.begin(), v.end());
            LOG("BOX",QString("  noMatch: [%1, %2, %3, %4, %5, %6]")
                .arg(v[0],0,'f',3).arg(v[1],0,'f',3).arg(v[2],0,'f',3)
                .arg(v[3],0,'f',3).arg(v[4],0,'f',3).arg(v[5],0,'f',3));
        }
    }
    if (!propKey.isEmpty()) {
        QString display;
        if (!faceParts.isEmpty()) display += QString::fromUtf8("\xE9\x9D\xA2: ") + faceParts.join(" | ");
        if (!pointParts.isEmpty()) {
            if (!display.isEmpty()) display += "  ";
            display += QString::fromUtf8("\xE7\x82\xB9: ") + pointParts.join(" | ");
        }
        if (display.isEmpty()) display = QString::fromUtf8("(\xE6\x97\xA0\xE5\x8C\xB9\xE9\x85\x8D)");
        emit boxesResolved(propKey, display);
    }
}
QVector<int> Model3DViewer::resolveBoxes(const QVector<QVector<double>>& boxes) const {
    QSet<int> allIds;
    for (const auto& box : boxes) {
        if (box.size() < 6) continue;
        double eps = 0.01;
        bool isPoint = (qAbs(box[3]-box[0]) < eps && qAbs(box[4]-box[1]) < eps && qAbs(box[5]-box[2]) < eps);
        auto ids = isPoint ? m_gl->findFacesByCenter(box[0], box[1], box[2]) : m_gl->findFacesInBox(box[0], box[1], box[2], box[3], box[4], box[5]);
        for (int id : ids) allIds.insert(id);
    }
    return QVector<int>(allIds.begin(), allIds.end());
}
void Model3DViewer::applyPendingBoxes() {
    // 模型加载完成后，逐一解析所有 pending 的包围盒
    for (auto it = m_pendingBoxesMap.begin(); it != m_pendingBoxesMap.end(); ++it)
        highlightFacesInBoxes(it.key(), it.value(), true);
}
void Model3DViewer::toggleFaceIds(){m_showFaceIdsFlag=m_btnShowFaceIds->isChecked();m_gl->setShowFaceIds(m_showFaceIdsFlag);}
#include "Model3DViewer.moc"
