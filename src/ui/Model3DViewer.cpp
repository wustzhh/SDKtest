#define GL_SILENCE_DEPRECATION
#include <GL/gl.h>
#include "Model3DViewer.h"
#include "core/StepReader.h"
#include "core/Logger.h"
#include <QtMath>
#include <QHBoxLayout>

GLViewer::GLViewer(QWidget* parent) : QOpenGLWidget(parent) {
    setMinimumSize(200, 150);
    setMouseTracking(true);
    setAttribute(Qt::WA_NativeWindow);
}

void GLViewer::loadMesh(const QVector<QVector3D>& verts, const QVector<int>& indices, const QVector<int>& triangles) {
    m_verts = verts; m_idx = indices; m_tri = triangles;
    float minX=1e9,minY=1e9,minZ=1e9,maxX=-1e9,maxY=-1e9,maxZ=-1e9;
    for (const auto& v : verts) {
        if (v.x()<minX) minX=v.x(); if (v.x()>maxX) maxX=v.x();
        if (v.y()<minY) minY=v.y(); if (v.y()>maxY) maxY=v.y();
        if (v.z()<minZ) minZ=v.z(); if (v.z()>maxZ) maxZ=v.z();
    }
    m_modelSize = qMax(qMax(maxX-minX, maxY-minY), maxZ-minZ);
    if (m_modelSize < 0.001f) m_modelSize = 1.0f;
    m_bboxDX = maxX-minX; m_bboxDY = maxY-minY; m_bboxDZ = maxZ-minZ;
    resetView();
}

void GLViewer::resetView() {
    m_panX=0; m_panY=0;
    float maxDim = qMax(qMax(m_bboxDX,m_bboxDY),m_bboxDZ);
    if (m_bboxDZ < maxDim*0.3f) { m_rotX=70; m_rotY=-45; }
    else if (m_bboxDY>maxDim*0.9f && m_bboxDX<maxDim*0.5f) { m_rotX=20; m_rotY=-20; }
    else { m_rotX=30; m_rotY=-35; }
    m_zoom=1.0f; update();
}

void GLViewer::clear() { m_verts.clear(); m_idx.clear(); m_tri.clear(); update(); }

void GLViewer::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.12f,0.12f,0.14f,1.0f);
    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
}

void GLViewer::resizeGL(int w, int h) { glViewport(0,0,w,h); }

void GLViewer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_verts.isEmpty()) return;

    float aspect = float(width()) / float(height());
    float s = m_modelSize * 0.5f / qMax(m_zoom, 0.01f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (aspect > 1) glOrtho(-s*aspect,s*aspect,-s,s,-100000,100000);
    else glOrtho(-s,s,-s/aspect,s/aspect,-100000,100000);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(m_panX,m_panY,0);
    glRotatef(m_rotX,1,0,0);
    glRotatef(m_rotY,0,1,0);

    glEnableClientState(GL_VERTEX_ARRAY);
    float* arr = new float[m_verts.size()*3];
    for (int i=0;i<m_verts.size();i++) { arr[i*3]=m_verts[i].x(); arr[i*3+1]=m_verts[i].y(); arr[i*3+2]=m_verts[i].z(); }
    glVertexPointer(3,GL_FLOAT,0,arr);

    if (!m_tri.isEmpty()) {
        glPolygonOffset(1,1); glEnable(GL_POLYGON_OFFSET_FILL);
        glColor3f(0.1f,0.1f,0.12f);
        glDrawElements(GL_TRIANGLES,m_tri.size(),GL_UNSIGNED_INT,m_tri.data());
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT,GL_NICEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(0.2f,0.85f,1.0f); glLineWidth(3.0f);
    glDrawElements(GL_LINES,m_idx.size(),GL_UNSIGNED_INT,m_idx.data());
    glDisable(GL_LINE_SMOOTH); glDisable(GL_BLEND);

    glDisableClientState(GL_VERTEX_ARRAY);
    delete[] arr;
}

void GLViewer::mousePressEvent(QMouseEvent* e) {
    m_lastPos=e->pos(); m_dragging=true;
}

void GLViewer::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    float dx=e->position().x()-m_lastPos.x(), dy=e->position().y()-m_lastPos.y();
    if (e->buttons() & Qt::LeftButton) { m_rotY+=dx*0.4f; m_rotX+=dy*0.4f; }
    else if (e->buttons() & Qt::MiddleButton) { m_panX+=dx*0.005f*m_modelSize/m_zoom; m_panY-=dy*0.005f*m_modelSize/m_zoom; }
    m_lastPos=e->pos(); update();
}

void GLViewer::wheelEvent(QWheelEvent* e) {
    if (e->angleDelta().y()>0) m_zoom=qMin(m_zoom*1.15f,100.0f);
    else m_zoom=qMax(m_zoom*0.85f,0.01f);
    update();
}

Model3DViewer::Model3DViewer(QWidget* parent) : QWidget(parent) {
    auto* l=new QVBoxLayout(this); l->setContentsMargins(0,0,0,0); l->setSpacing(2);
    m_gl=new GLViewer(this); l->addWidget(m_gl,1);
    auto* br=new QHBoxLayout();
    m_btnResetView=new QPushButton("\u2191 \u590D\u4F4D\u89C6\u89D2",this);
    m_btnResetView->setFixedHeight(24);
    m_btnResetView->setStyleSheet("QPushButton{background:#2196F3;color:white;border:none;border-radius:3px;padding:2px 12px;font-size:12px;}QPushButton:hover{background:#1976D2;}");
    connect(m_btnResetView,&QPushButton::clicked,m_gl,&GLViewer::resetView);
    br->addStretch(); br->addWidget(m_btnResetView); br->addStretch();
    l->addLayout(br);
    m_status=new QLabel("\u672A\u52A0\u8F7D\u6A21\u578B",this);
    m_status->setAlignment(Qt::AlignLeft|Qt::AlignTop); m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#999;font-size:12px;padding:6px;background:#333;");
    m_status->setMinimumHeight(36); m_status->setMaximumHeight(60);
    l->addWidget(m_status);
}

void Model3DViewer::loadFile(const QString& filePath) {
    LOG("3D","Load: "+filePath); m_status->setText("\u89E3\u6790\u4E2D...");
    StepMesh mesh=StepReader::parse(filePath);
    if (mesh.success&&!mesh.vertices.isEmpty()) {
        m_gl->loadMesh(mesh.vertices,mesh.indices,mesh.triangles);
        m_status->setText(QString("%1v %2e %3t").arg(mesh.vertices.size()).arg(mesh.indices.size()/2).arg(mesh.triangles.size()/3));
        m_status->setStyleSheet("color:#4CAF50;font-size:12px;padding:4px;background:#333;");
    } else {
        m_gl->clear(); m_status->setText("\u8BFB\u53D6\u5931\u8D25: "+mesh.error);
        m_status->setStyleSheet("color:#f44336;font-size:12px;padding:4px;background:#333;");
    }
}

void Model3DViewer::clear() { m_gl->clear(); m_status->setText("\u672A\u52A0\u8F7D\u6A21\u578B"); m_status->setStyleSheet("color:#999;font-size:12px;padding:4px;background:#333;"); }
