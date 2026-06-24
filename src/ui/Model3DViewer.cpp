#include <QtOpenGL>
#include <QtOpenGLWidgets>
#include "Model3DViewer.h"
#include "core/Logger.h"
#include <QtMath>
#include <QHBoxLayout>
#include <QMatrix4x4>
#include <QVector4D>
#include <QFileInfo>

#ifdef HAS_OCC
#include <STEPControl_Reader.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <Poly_Triangulation.hxx>
#include <BRep_Tool.hxx>
#include <gp_Pnt.hxx>
#endif

// ═══════════════════════════════════════════════════════════════
//  GLViewer
// ═══════════════════════════════════════════════════════════════

GLViewer::GLViewer(QWidget* p):QOpenGLWidget(p){setMinimumSize(200,150);setMouseTracking(true);}

void GLViewer::loadMesh(const QVector<QVector3D>& v,const QVector<int>& t){
    m_verts=v;m_tri=t;
    float mx=1e9,my=1e9,mz=1e9,Mx=-1e9,My=-1e9,Mz=-1e9;
    for(auto& vv:v){if(vv.x()<mx)mx=vv.x();if(vv.x()>Mx)Mx=vv.x();if(vv.y()<my)my=vv.y();if(vv.y()>My)My=vv.y();if(vv.z()<mz)mz=vv.z();if(vv.z()>Mz)Mz=vv.z();}
    m_modelSize=qMax(qMax(Mx-mx,My-my),Mz-mz);if(m_modelSize<.001f)m_modelSize=1;
    m_anchor=QVector3D((mx+Mx)/2,(my+My)/2,(mz+Mz)/2);m_hasAnchor=false;
    resetView();
}

void GLViewer::resetView(){m_panX=0;m_panY=0;m_hasAnchor=false;m_pendingPick=false;m_rotX=25;m_rotY=-35;m_zoom=1;update();}
void GLViewer::clear(){m_verts.clear();m_tri.clear();update();}

void GLViewer::initializeGL(){initializeOpenGLFunctions();glClearColor(.12f,.12f,.14f,1);glEnable(GL_DEPTH_TEST);}
void GLViewer::resizeGL(int w,int h){glViewport(0,0,w,h);}

void GLViewer::paintGL(){
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(m_verts.isEmpty())return;

    float as=float(width())/float(height()),s=m_modelSize*.5f/qMax(m_zoom,.01f);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    if(as>1)glOrtho(-s*as,s*as,-s,s,-1e5,1e5);else glOrtho(-s,s,-s/as,s/as,-1e5,1e5);

    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glTranslatef(m_panX,m_panY,0);
    glTranslatef(m_anchor.x(),m_anchor.y(),m_anchor.z());
    glRotatef(m_rotX,1,0,0);
    glRotatef(m_rotY,0,1,0);
    glTranslatef(-m_anchor.x(),-m_anchor.y(),-m_anchor.z());

    GLfloat mv[16],pj[16];GLint vp[4];
    glGetFloatv(GL_MODELVIEW_MATRIX,mv);glGetFloatv(GL_PROJECTION_MATRIX,pj);glGetIntegerv(GL_VIEWPORT,vp);

    glEnableClientState(GL_VERTEX_ARRAY);
    float* arr=new float[m_verts.size()*3];
    for(int i=0;i<m_verts.size();i++){arr[i*3]=m_verts[i].x();arr[i*3+1]=m_verts[i].y();arr[i*3+2]=m_verts[i].z();}
    glVertexPointer(3,GL_FLOAT,0,arr);

    if(!m_tri.isEmpty()){
        glPolygonOffset(1,1);glEnable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);
        GLfloat lp[]={1,1,1,0};glLightfv(GL_LIGHT0,GL_POSITION,lp);
        GLfloat amb[]={.2f,.2f,.25f,1};glLightfv(GL_LIGHT0,GL_AMBIENT,amb);
        GLfloat dif[]={.6f,.6f,.7f,1};glLightfv(GL_LIGHT0,GL_DIFFUSE,dif);
        glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);glEnable(GL_COLOR_MATERIAL);
        glColor3f(.55f,.6f,.7f);
        glDrawElements(GL_TRIANGLES,m_tri.size(),GL_UNSIGNED_INT,m_tri.data());
        glDisable(GL_LIGHTING);glDisable(GL_LIGHT0);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    glDisableClientState(GL_VERTEX_ARRAY);delete[]arr;

    if(m_pendingPick){
        m_pendingPick=false;
        GLfloat depth;glReadPixels(int(m_pickPos.x()),height()-int(m_pickPos.y()),1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
        if(depth<.999f){
            float nx=2*(m_pickPos.x()-vp[0])/vp[2]-1,ny=-(2*(m_pickPos.y()-vp[1])/vp[3]-1),nz=2*depth-1;
            QMatrix4x4 pM(static_cast<const float*>(pj)),mM(static_cast<const float*>(mv));
            QVector4D w=(pM*mM).inverted()*QVector4D(nx,ny,nz,1);
            if(qAbs(w.w())>1e-10f)w/=w.w();
            m_anchor=QVector3D(w.x(),w.y(),w.z());m_hasAnchor=true;
        }
    }

    if(m_hasAnchor){
        glDisable(GL_DEPTH_TEST);float s2=m_modelSize*.03f;
        glColor3f(1,.2f,.2f);glLineWidth(2);
        glBegin(GL_LINES);
        glVertex3f(-s2,0,0);glVertex3f(s2,0,0);glVertex3f(0,-s2,0);glVertex3f(0,s2,0);glVertex3f(0,0,-s2);glVertex3f(0,0,s2);
        glEnd();
        glBegin(GL_LINE_LOOP);
        for(int i=0;i<36;i++){float a=i*10*M_PI/180;glVertex3f(s2*1.6f*cos(a),s2*1.6f*sin(a),0);}
        glEnd();
    }
}

void GLViewer::mousePressEvent(QMouseEvent* e){
    m_lastPos=e->pos();m_dragging=true;
    if((e->modifiers()&Qt::ControlModifier)&&e->button()==Qt::LeftButton&&!m_verts.isEmpty()){
        m_pickPos=e->position();m_pendingPick=true;update();
    }
}

void GLViewer::mouseMoveEvent(QMouseEvent* e){
    if(!m_dragging)return;
    float dx=e->position().x()-m_lastPos.x(),dy=e->position().y()-m_lastPos.y();
    if(e->buttons()&Qt::LeftButton){m_rotY+=dx*.4f;m_rotX+=dy*.4f;}
    else if(e->buttons()&Qt::MiddleButton){m_panX+=dx*.005f*m_modelSize/m_zoom;m_panY-=dy*.005f*m_modelSize/m_zoom;}
    m_lastPos=e->pos();update();
}

void GLViewer::wheelEvent(QWheelEvent* e){
    if(e->angleDelta().y()>0)m_zoom=qMin(m_zoom*1.15f,100.f);else m_zoom=qMax(m_zoom*.85f,.01f);
    update();
}

// ═══════════════════════════════════════════════════════════════
//  Model3DViewer
// ═══════════════════════════════════════════════════════════════

Model3DViewer::Model3DViewer(QWidget* p):QWidget(p){
    auto*l=new QVBoxLayout(this);l->setContentsMargins(0,0,0,0);l->setSpacing(2);
    m_gl=new GLViewer(this);l->addWidget(m_gl,1);
    auto*br=new QHBoxLayout();
    m_btnReset=new QPushButton("\u2191 \u590D\u4F4D\u89C6\u89D2",this);
    m_btnReset->setFixedHeight(24);
    m_btnReset->setStyleSheet("QPushButton{background:#2196F3;color:white;border:none;border-radius:3px;padding:2px 12px;font-size:12px;}QPushButton:hover{background:#1976D2;}");
    connect(m_btnReset,&QPushButton::clicked,m_gl,&GLViewer::resetView);
    br->addStretch();br->addWidget(m_btnReset);br->addStretch();l->addLayout(br);
    m_status=new QLabel("\u672A\u52A0\u8F7D\u6A11\u578B",this);
    m_status->setAlignment(Qt::AlignLeft|Qt::AlignTop);m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#999;font-size:12px;padding:6px;background:#333;");m_status->setMinimumHeight(36);
    l->addWidget(m_status);
}

void Model3DViewer::loadFile(const QString& fp){
    LOG("3D","Load: "+fp);m_status->setText("\u89E3\u6790\u4E2D...");

#ifdef HAS_OCC
    STEPControl_Reader reader;
    if (reader.ReadFile(fp.toUtf8().constData()) != IFSelect_RetDone) {
        m_status->setText("\u8BFB\u53D6\u5931\u8D25"); return;
    }
    reader.TransferRoots();
    TopoDS_Shape shape = reader.OneShape();
    if (shape.IsNull()) { m_status->setText("\u5F62\u72B6\u4E3A\u7A7A"); return; }

    BRepMesh_IncrementalMesh(shape, 0.5).Perform();

    QVector<QVector3D> verts;
    QVector<int> tris;
    int voff = 0;

    TopExp_Explorer exp(shape, TopAbs_FACE);
    for (; exp.More(); exp.Next()) {
        TopoDS_Face face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;
        for (int i = 1; i <= tri->NbNodes(); i++) {
            gp_Pnt p = tri->Node(i).Transformed(loc.Transformation());
            verts.append(QVector3D(p.X(), p.Y(), p.Z()));
        }
        for (int i = 1; i <= tri->NbTriangles(); i++) {
            int n1, n2, n3;
            tri->Triangle(i).Get(n1, n2, n3);
            tris.append(voff + n1 - 1);
            tris.append(voff + n2 - 1);
            tris.append(voff + n3 - 1);
        }
        voff += tri->NbNodes();
    }

    if (verts.isEmpty() || tris.isEmpty()) {
        m_status->setText("\u4E09\u89D2\u5316\u5931\u8D25"); return;
    }

    m_gl->loadMesh(verts, tris);
    m_status->setText(QString("OCCT: %1v %2t").arg(verts.size()).arg(tris.size()/3));
    m_status->setStyleSheet("color:#4CAF50;font-size:12px;padding:4px;background:#333;");
    LOG("3D","OK: "+QString::number(verts.size())+" verts, "+QString::number(tris.size()/3)+" tris");
#else
    m_status->setText("OCCT not available");
    LOG("3D","OCCT not available");
#endif
}

void Model3DViewer::clear(){m_gl->clear();m_status->setText("\u672A\u52A0\u8F7D\u6A11\u578B");m_status->setStyleSheet("color:#999;font-size:12px;padding:4px;background:#333;");}
