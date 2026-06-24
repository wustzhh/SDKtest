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
#include <QMap>
#include <QSet>
#include <QApplication>
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
    BRepMesh_IncrementalMesh(shape, 1.0).Perform();
    emit progress(QString::fromUtf8("\xE6\x8F\x90\xE5\x8F\x96\xE7\xBD\x91\xE6\xA0\xBC..."));
    int voff=0, faceIdx=0;
    TopExp_Explorer fExp(shape, TopAbs_FACE);
    for (; fExp.More(); fExp.Next()) {
        TopoDS_Face face = TopoDS::Face(fExp.Current()); TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;
        int base=voff, triStart=r.tris.size()/3;
        for (int i=1;i<=tri->NbNodes();i++) { gp_Pnt p=tri->Node(i).Transformed(loc.Transformation()); r.verts.append(QVector3D(p.X(),p.Y(),p.Z())); voff++; }
        for (int i=1;i<=tri->NbTriangles();i++) { int n1,n2,n3; tri->Triangle(i).Get(n1,n2,n3); r.tris.append(base+n1-1); r.tris.append(base+n2-1); r.tris.append(base+n3-1); }
        for (int i=1;i<=tri->NbTriangles();i++) {
            int n1,n2,n3; tri->Triangle(i).Get(n1,n2,n3);
            gp_Pnt p1=tri->Node(n1).Transformed(loc.Transformation()),p2=tri->Node(n2).Transformed(loc.Transformation()),p3=tri->Node(n3).Transformed(loc.Transformation());
            gp_Vec e1(p1,p2),e2(p1,p3),n=e1.Crossed(e2);
            if (n.Magnitude()>1e-10) n.Normalize(); else n.SetCoord(0,0,1);
            if (face.Orientation()==TopAbs_REVERSED) n.Reverse();
            for (int j=0;j<3;j++) { int vi=(j==0?n1:(j==1?n2:n3)); if (base+vi-1>=r.normals.size()) r.normals.resize(base+vi); r.normals[base+vi-1]=QVector3D(n.X(),n.Y(),n.Z()); }
        }
        int triEnd=r.tris.size()/3; QVector3D center(0,0,0); int cnt=0;
        for (int ti=triStart;ti<triEnd;ti++) { r.faceIds.append(faceIdx); int i0=r.tris[ti*3],i1=r.tris[ti*3+1],i2=r.tris[ti*3+2]; center+=r.verts[i0]+r.verts[i1]+r.verts[i2]; cnt+=3; }
        if (cnt>0) center/=cnt; r.faceCenters.append(center); faceIdx++;
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
        int ns=qMax(3,qMin(20,(int)((l-f)/0.5))); double st=(l-f)/ns; int prev=-1;
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
    {   FILE* df = fopen("test_runner_ui_debug.log", "a");
        if (df) { fprintf(df, "\n===== %s =====\n", QFileInfo(m_path).fileName().toUtf8().constData()); }
        if (df) {
            fprintf(df, "=== OCCT Debug ===\nFile: %s\n\n", m_path.toUtf8().constData());
            std::map<void*,int> fid; int nf=0;
            { TopExp_Explorer fe(shape,TopAbs_FACE); for(;fe.More();fe.Next()) fid[fe.Current().TShape().get()] = nf++; }
            fprintf(df, "Face IDs: [");
            for (int i=0;i<nf;i++) { if(i>0)fprintf(df,", "); fprintf(df,"%d",i); }
            fprintf(df, "]\n");
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
GLViewer::GLViewer(QWidget* p):QOpenGLWidget(p){setMinimumSize(200,150);setMouseTracking(true);}
void GLViewer::loadMesh(const QVector<QVector3D>& v,const QVector<int>& t,const QVector<QVector3D>& n,const QVector<EdgeLine>& e,const QVector<int>& fi,const QVector<QVector3D>& fc){
    m_verts=v;m_tri=t;m_normals=n;m_edges=e;m_faceIds=fi;m_faceCenters=fc;
    float mx=1e9,my=1e9,mz=1e9,Mx=-1e9,My=-1e9,Mz=-1e9;
    for(auto& vv:v){if(vv.x()<mx)mx=vv.x();if(vv.x()>Mx)Mx=vv.x();if(vv.y()<my)my=vv.y();if(vv.y()>My)My=vv.y();if(vv.z()<mz)mz=vv.z();if(vv.z()>Mz)Mz=vv.z();}
    m_modelSize=qMax(qMax(Mx-mx,My-my),Mz-mz);m_modelSize=qMax(m_modelSize,.001f);
    m_anchor=QVector3D((mx+Mx)/2,(my+My)/2,(mz+Mz)/2);m_hasAnchor=false;resetView();
}
void GLViewer::resetView(){m_panX=0;m_panY=0;m_hasAnchor=false;m_pendingPick=false;m_rot=QQuaternion();m_zoom=1;update();}
void GLViewer::clear(){m_verts.clear();m_tri.clear();m_normals.clear();m_edges.clear();m_faceIds.clear();m_faceCenters.clear();update();}
void GLViewer::initializeGL(){initializeOpenGLFunctions();glClearColor(.12f,.12f,.14f,1);glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_LIGHT1);glEnable(GL_NORMALIZE);
    GLfloat a0[]={.4f,.4f,.45f,1};glLightfv(GL_LIGHT0,GL_AMBIENT,a0);GLfloat d0[]={.6f,.6f,.7f,1};glLightfv(GL_LIGHT0,GL_DIFFUSE,d0);GLfloat s0[]={.2f,.2f,.2f,1};glLightfv(GL_LIGHT0,GL_SPECULAR,s0);
    GLfloat a1[]={.15f,.15f,.2f,1};glLightfv(GL_LIGHT1,GL_AMBIENT,a1);GLfloat d1[]={.3f,.3f,.4f,1};glLightfv(GL_LIGHT1,GL_DIFFUSE,d1);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);glEnable(GL_COLOR_MATERIAL);}
void GLViewer::resizeGL(int w,int h){glViewport(0,0,w,h);}
void GLViewer::paintGL(){
    glClear(GL_DEPTH_BUFFER_BIT);
    // 渐变背景：深蓝→深紫
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,1,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glDisable(GL_LIGHTING);glBegin(GL_QUADS);
    glColor3f(.08f,.1f,.18f);glVertex2f(0,0);glVertex2f(1,0);
    glColor3f(.12f,.08f,.18f);glVertex2f(1,1);glVertex2f(0,1);
    glEnd();glEnable(GL_LIGHTING);

    if(m_verts.isEmpty())return;
    float as=float(width())/float(height()),sz=m_modelSize*.5f/qMax(m_zoom,.01f);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    float nearF=-sz*5,farF=sz*5;if(as>1)glOrtho(-sz*as,sz*as,-sz,sz,nearF,farF);else glOrtho(-sz,sz,-sz/as,sz/as,nearF,farF);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(m_panX,m_panY,0);glTranslatef(m_anchor.x(),m_anchor.y(),m_anchor.z());
    QMatrix4x4 rmat;rmat.rotate(m_rot);glMultMatrixf(rmat.constData());glTranslatef(-m_anchor.x(),-m_anchor.y(),-m_anchor.z());
    GLfloat lp0[]={1,1,1,0};glLightfv(GL_LIGHT0,GL_POSITION,lp0);
    GLfloat lp1[]={-1,-1,-.5f,0};glLightfv(GL_LIGHT1,GL_POSITION,lp1);GLfloat mv[16],pj[16];GLint vp[4];
    glGetFloatv(GL_MODELVIEW_MATRIX,mv);glGetFloatv(GL_PROJECTION_MATRIX,pj);glGetIntegerv(GL_VIEWPORT,vp);
    glEnable(GL_LIGHTING);
    if(!m_tri.isEmpty()){
        glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);
        float* va=new float[m_verts.size()*3];float* na=new float[m_normals.size()*3];
        for(int i=0;i<m_verts.size();i++){va[i*3]=m_verts[i].x();va[i*3+1]=m_verts[i].y();va[i*3+2]=m_verts[i].z();
            if(i<m_normals.size()){na[i*3]=m_normals[i].x();na[i*3+1]=m_normals[i].y();na[i*3+2]=m_normals[i].z();}else{na[i*3]=0;na[i*3+1]=1;na[i*3+2]=0;}}
        glVertexPointer(3,GL_FLOAT,0,va);glNormalPointer(GL_FLOAT,0,na);
        glPolygonOffset(1,1);glEnable(GL_POLYGON_OFFSET_FILL);glColor3f(.55f,.62f,.72f);
        glDrawElements(GL_TRIANGLES,m_tri.size(),GL_UNSIGNED_INT,m_tri.data());
        glDisable(GL_POLYGON_OFFSET_FILL);glDisableClientState(GL_NORMAL_ARRAY);glDisableClientState(GL_VERTEX_ARRAY);delete[]va;delete[]na;
    }
    glDisable(GL_LIGHTING);
    if(!m_edges.isEmpty()){
        glEnableClientState(GL_VERTEX_ARRAY);float* ea=new float[m_verts.size()*3];
        for(int i=0;i<m_verts.size();i++){ea[i*3]=m_verts[i].x();ea[i*3+1]=m_verts[i].y();ea[i*3+2]=m_verts[i].z();}
        glVertexPointer(3,GL_FLOAT,0,ea);glLineWidth(2);
        for(const auto& e:m_edges){int idx[2]={e.v0,e.v1};glColor3f(e.color.x(),e.color.y(),e.color.z());glDrawElements(GL_LINES,2,GL_UNSIGNED_INT,idx);}
        glDisableClientState(GL_VERTEX_ARRAY);delete[]ea;
    }

    if(m_pendingPick){
        m_pendingPick=false;GLfloat depth;glReadPixels(int(m_pickPos.x()),height()-int(m_pickPos.y()),1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
        if(depth<.999f){float nx=2*(m_pickPos.x()-vp[0])/vp[2]-1,ny=-(2*(m_pickPos.y()-vp[1])/vp[3]-1),nz=2*depth-1;
            QMatrix4x4 pM(static_cast<const float*>(pj)),mM(static_cast<const float*>(mv));QVector4D w=(pM*mM).inverted()*QVector4D(nx,ny,nz,1);if(qAbs(w.w())>1e-10f)w/=w.w();
            m_anchor=QVector3D(w.x(),w.y(),w.z());m_hasAnchor=true;}
    }
    if(m_hasAnchor){glDisable(GL_DEPTH_TEST);float s2=m_modelSize*.03f;glColor3f(1,.2f,.2f);glLineWidth(2);
        glBegin(GL_LINES);glVertex3f(-s2,0,0);glVertex3f(s2,0,0);glVertex3f(0,-s2,0);glVertex3f(0,s2,0);glVertex3f(0,0,-s2);glVertex3f(0,0,s2);glEnd();
        glBegin(GL_LINE_LOOP);for(int i=0;i<36;i++){float a=i*10*(float)M_PI/180;glVertex3f(s2*1.6f*cos(a),s2*1.6f*sin(a),0);}glEnd();glEnable(GL_DEPTH_TEST);}
}
void GLViewer::mousePressEvent(QMouseEvent* e){m_lastPos=e->pos();m_dragging=true;if((e->modifiers()&Qt::ControlModifier)&&e->button()==Qt::LeftButton&&!m_verts.isEmpty()){m_pickPos=e->position();m_pendingPick=true;update();}}
void GLViewer::mouseMoveEvent(QMouseEvent* e){if(!m_dragging)return;float dx=e->position().x()-m_lastPos.x(),dy=e->position().y()-m_lastPos.y();if(e->buttons()&Qt::LeftButton){QQuaternion dq=QQuaternion::fromAxisAndAngle(QVector3D(0,1,0),dx*.4f)*QQuaternion::fromAxisAndAngle(QVector3D(1,0,0),dy*.4f);m_rot=dq*m_rot;m_rot.normalize();}else if(e->buttons()&Qt::MiddleButton){m_panX+=dx*.005f*m_modelSize/m_zoom;m_panY-=dy*.005f*m_modelSize/m_zoom;}m_lastPos=e->pos();update();}
void GLViewer::wheelEvent(QWheelEvent* e){if(e->angleDelta().y()>0)m_zoom=qMin(m_zoom*1.15f,100.f);else m_zoom=qMax(m_zoom*.85f,.01f);update();}

// ═══════════════════════════════════════════════════════════════
//  Model3DViewer
// ═══════════════════════════════════════════════════════════════
Model3DViewer::Model3DViewer(QWidget* p):QWidget(p){
    auto*l=new QVBoxLayout(this);l->setContentsMargins(0,0,0,0);l->setSpacing(2);
    m_gl=new GLViewer(this);l->addWidget(m_gl,1);
    auto*br=new QHBoxLayout();m_btnReset=new QPushButton(QString::fromUtf8("\xE2\x86\x91 \xE5\xA4\x8D\xE4\xBD\x8D\xE8\xA7\x86\xE8\xA7\x92"),this);
    m_btnReset->setFixedHeight(24);m_btnReset->setStyleSheet("QPushButton{background:#2196F3;color:white;border:none;border-radius:3px;padding:2px 12px;font-size:12px;}QPushButton:hover{background:#1976D2;}");
    connect(m_btnReset,&QPushButton::clicked,m_gl,&GLViewer::resetView);
    br->addStretch();br->addWidget(m_btnReset);br->addStretch();l->addLayout(br);
    m_status=new QLabel(QString::fromUtf8("\xE6\x9C\xAA\xE5\x8A\xA0\xE8\xBD\xBD\xE6\xA8\xA1\xE5\x9E\x8B"),this);
    m_status->setAlignment(Qt::AlignLeft|Qt::AlignTop);m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#999;font-size:12px;padding:6px;background:#333;");m_status->setMinimumHeight(36);l->addWidget(m_status);
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
            m_gl->loadMesh(r.verts,r.tris,r.normals,r.edges,r.faceIds,r.faceCenters);
            m_status->setText(QString("OCCT: %1v %2t %3e").arg(r.verts.size()).arg(r.tris.size()/3).arg(r.edges.size()));
            m_status->setStyleSheet("color:#4CAF50;font-size:12px;padding:4px;background:#333;");}
        else{m_status->setText(r.error);m_status->setStyleSheet("color:#f44336;font-size:12px;padding:4px;background:#333;");LOG("3D","FAIL: "+r.error);}
        if(m_workerThread){m_workerThread->quit();m_workerThread->wait();m_workerThread=nullptr;}if(m_worker){m_worker->deleteLater();m_worker=nullptr;}
    });
    connect(m_workerThread,&QThread::finished,this,[this](){if(m_worker){m_worker->deleteLater();m_worker=nullptr;}});
    connect(m_timeoutTimer,&QTimer::timeout,this,[this](){
        LOG("3D","TIMEOUT 30s");m_countdownTimer->stop();
        if(m_workerThread){m_workerThread->requestInterruption();m_workerThread->quit();m_workerThread->wait(1000);m_workerThread=nullptr;}
        if(m_worker){m_worker->deleteLater();m_worker=nullptr;}
        m_status->setText(QString::fromUtf8("\xE8\xB6\x85\xE6\x97\xB6\xEF\xBC\x88")+"30s\xEF\xBC\x89");m_status->setStyleSheet("color:#f44336;font-size:12px;padding:4px;background:#333;");
    });
    m_remainingSeconds=30;m_status->setText(QString::fromUtf8("\xE5\x8A\xA0\xE8\xBD\xBD\xE4\xB8\xAD... 30s"));
    m_countdownTimer->start(1000);m_timeoutTimer->start(30000);m_workerThread->start();
}
void Model3DViewer::clear(){cancelLoad();if(m_worker){m_worker->deleteLater();m_worker=nullptr;}m_gl->clear();
    m_status->setText(QString::fromUtf8("\xE6\x9C\xAA\xE5\x8A\xA0\xE8\xBD\xBD\xE6\xA8\xA1\xE5\x9E\x8B"));m_status->setStyleSheet("color:#999;font-size:12px;padding:4px;background:#333;");}
#include "Model3DViewer.moc"
