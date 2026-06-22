#define GL_SILENCE_DEPRECATION
#include <GL/gl.h>
#include "Model3DViewer.h"
#include "core/StepReader.h"
#include "core/Logger.h"
#include <QtMath>
#include <QHBoxLayout>
#include <QMatrix4x4>
#include <QVector4D>

// ════════════════════════════════════════════════════════════
//  GLViewer
// ════════════════════════════════════════════════════════════

GLViewer::GLViewer(QWidget* parent) : QOpenGLWidget(parent) {
    setMinimumSize(200, 150);
    setMouseTracking(true);
    setAttribute(Qt::WA_NativeWindow);
}

void GLViewer::loadMesh(const QVector<QVector3D>& verts, const QVector<int>& indices, const QVector<int>& triangles) {
    m_verts = verts;
    m_idx = indices;
    m_tri = triangles;
    float minX=1e9,minY=1e9,minZ=1e9,maxX=-1e9,maxY=-1e9,maxZ=-1e9;
    for (const auto& v : verts) {
        if (v.x()<minX) minX=v.x(); if (v.x()>maxX) maxX=v.x();
        if (v.y()<minY) minY=v.y(); if (v.y()>maxY) maxY=v.y();
        if (v.z()<minZ) minZ=v.z(); if (v.z()>maxZ) maxZ=v.z();
    }
    m_modelSize = qMax(qMax(maxX-minX, maxY-minY), maxZ-minZ);
    if (m_modelSize < 0.001f) m_modelSize = 1.0f;
    m_bboxDX = maxX-minX; m_bboxDY = maxY-minY; m_bboxDZ = maxZ-minZ;
    m_anchor = QVector3D((minX+maxX)/2, (minY+maxY)/2, (minZ+maxZ)/2);
    m_hasAnchor = false;
    resetView();
}

void GLViewer::resetView() {
    m_panX = 0; m_panY = 0;
    m_hasAnchor = false;
    // 自适应视角：根据模型形状选择最佳初始角度
    float maxDim = qMax(qMax(m_bboxDX, m_bboxDY), m_bboxDZ);
    if (m_bboxDZ < maxDim * 0.3f) {
        // 扁平模型 → 俯视
        m_rotX = 70; m_rotY = -45;
    } else if (m_bboxDY > maxDim * 0.9f && m_bboxDX < maxDim * 0.5f) {
        // 细长模型 → 侧视
        m_rotX = 20; m_rotY = -20;
    } else {
        // 常规 → 等轴侧
        m_rotX = 30; m_rotY = -35;
    }
    m_zoom = 1.0f;
    update();
}

void GLViewer::clear() {
    m_verts.clear(); m_idx.clear(); m_tri.clear();
    update();
}

void GLViewer::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void GLViewer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void GLViewer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_verts.isEmpty()) return;

    float aspect = float(width()) / float(height());
    float s = m_modelSize * 0.5f / qMax(m_zoom, 0.01f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (aspect > 1) glOrtho(-s*aspect, s*aspect, -s, s, -100000, 100000);
    else glOrtho(-s, s, -s/aspect, s/aspect, -100000, 100000);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(m_panX, m_panY, 0);
    glRotatef(m_rotX, 1, 0, 0);
    glRotatef(m_rotY, 0, 1, 0);

    // 处理挂起的锚点拾取（深度缓冲已被当前帧的模型填充）
    if (m_pendingPick) {
        m_pendingPick = false;
        // 采样 21x21 区域取最小深度（线框太细，大范围采样确保命中）
        GLfloat depths[441];
        int px = int(m_pickPos.x()), py = height()-int(m_pickPos.y());
        int rx = 10, ry = 10;
        if (px-rx < 0) rx = px;
        if (py-ry < 0) ry = py;
        if (px+rx >= width()) rx = width()-px-1;
        if (py+ry >= height()) ry = height()-py-1;
        glReadPixels(px-rx, py-ry, rx*2+1, ry*2+1, GL_DEPTH_COMPONENT, GL_FLOAT, depths);
        GLfloat depth = 1.0f;
        int count = (rx*2+1)*(ry*2+1);
        for (int i = 0; i < count; i++)
            if (depths[i] < depth) depth = depths[i];
        LOG("3D", "Pick depth(min)=" + QString::number(depth,'f',4));
        if (depth < 0.999f) {
            // 命中模型表面 → 精确锚点
            GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
            float ndcX = (2.0f*(m_pickPos.x()-vp[0])/vp[2] - 1.0f);
            float ndcY = -(2.0f*(m_pickPos.y()-vp[1])/vp[3] - 1.0f);
            float ndcZ = 2.0f*depth - 1.0f;
            GLfloat mv[16], pj[16];
            glGetFloatv(GL_MODELVIEW_MATRIX, mv);
            glGetFloatv(GL_PROJECTION_MATRIX, pj);
            QMatrix4x4 projMat(static_cast<const float*>(pj));
            QMatrix4x4 mvMat(static_cast<const float*>(mv));
            QVector4D world = (projMat * mvMat).inverted() * QVector4D(ndcX, ndcY, ndcZ, 1.0f);
            if (qAbs(world.w()) > 1e-10f) world /= world.w();
            m_anchor = QVector3D(world.x(), world.y(), world.z());
            m_hasAnchor = true;
            LOG("3D", "Anchor OK: " + QString::number(m_anchor.x(),'f',1) + ","
                + QString::number(m_anchor.y(),'f',1) + "," + QString::number(m_anchor.z(),'f',1));
        } else {
            // 未命中模型表面 → 计算射线与包围盒的交点
            GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
            GLfloat mv[16], pj[16];
            glGetFloatv(GL_MODELVIEW_MATRIX, mv);
            glGetFloatv(GL_PROJECTION_MATRIX, pj);
            // 点击位置在近平面和远平面的NDC坐标
            float ndcX = (2.0f*(m_pickPos.x()-vp[0])/vp[2] - 1.0f);
            float ndcY = -(2.0f*(m_pickPos.y()-vp[1])/vp[3] - 1.0f);
            QMatrix4x4 projMat(static_cast<const float*>(pj));
            QMatrix4x4 mvMat(static_cast<const float*>(mv));
            QMatrix4x4 invMV = (projMat * mvMat).inverted();
            QVector4D nearP = invMV * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
            QVector4D farP  = invMV * QVector4D(ndcX, ndcY, 1.0f, 1.0f);
            if (qAbs(nearP.w()) > 1e-10f) nearP /= nearP.w();
            if (qAbs(farP.w()) > 1e-10f) farP /= farP.w();
            QVector3D rayOrig(nearP.x(), nearP.y(), nearP.z());
            QVector3D rayDir(farP.x()-rayOrig.x(), farP.y()-rayOrig.y(), farP.z()-rayOrig.z());
            rayDir.normalize();
            // 与模型包围盒（球）求交
            QVector3D center = m_anchor; // 当前锚点（默认模型中心）
            float R = m_modelSize * 0.5f;
            QVector3D oc = rayOrig - center;
            float a = QVector3D::dotProduct(rayDir, rayDir);
            float b = 2.0f * QVector3D::dotProduct(oc, rayDir);
            float c = QVector3D::dotProduct(oc, oc) - R*R;
            float disc = b*b - 4*a*c;
            if (disc >= 0) {
                float t = (-b - sqrt(disc)) / (2.0f*a);
                if (t < 0) t = (-b + sqrt(disc)) / (2.0f*a);
                if (t > 0) {
                    m_anchor = rayOrig + rayDir * t;
                    m_hasAnchor = true;
                    LOG("3D", "Anchor(ray): " + QString::number(m_anchor.x(),'f',1) + ","
                        + QString::number(m_anchor.y(),'f',1) + "," + QString::number(m_anchor.z(),'f',1));
                }
            }
        }
    }

    // 校验锚点有效性
    if (qIsNaN(m_anchor.x()) || qIsInf(m_anchor.x()) ||
        m_anchor.length() > m_modelSize * 100) {
        m_anchor = QVector3D(0,0,0);
        m_hasAnchor = false;
    }
    glTranslatef(-m_anchor.x(), -m_anchor.y(), -m_anchor.z());

    // 顶点数组
    glEnableClientState(GL_VERTEX_ARRAY);
    float* arr = new float[m_verts.size() * 3];
    for (int i = 0; i < m_verts.size(); ++i) {
        arr[i*3]=m_verts[i].x(); arr[i*3+1]=m_verts[i].y(); arr[i*3+2]=m_verts[i].z();
    }
    glVertexPointer(3, GL_FLOAT, 0, arr);

    // Pass 1: 三角面深度填充（若有三角数据）
    if (!m_tri.isEmpty()) {
        glPolygonOffset(1.0f, 1.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glColor3f(0.1f, 0.1f, 0.12f);
        glDrawElements(GL_TRIANGLES, m_tri.size(), GL_UNSIGNED_INT, m_tri.data());
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Pass 2: 线框（更亮更粗）
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(0.2f, 0.85f, 1.0f);
    glLineWidth(3.0f);
    glDrawElements(GL_LINES, m_idx.size(), GL_UNSIGNED_INT, m_idx.data());
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);

    // 绘制锚点指示器（红色十字）
    if (m_hasAnchor) {
        float s2 = m_modelSize * 0.03f;
        glColor3f(1.0f, 0.2f, 0.2f);
        glLineWidth(2.0f);
        GLfloat anchorVerts[] = {
            -s2,0,0, s2,0,0,
            0,-s2,0, 0,s2,0,
            0,0,-s2, 0,0,s2
        };
        glVertexPointer(3, GL_FLOAT, 0, anchorVerts);
        glDrawArrays(GL_LINES, 0, 6);
        // 外圈圆圈
        GLfloat circleVerts[72];
        for (int i = 0; i < 36; i++) {
            float a = i * 10 * M_PI / 180.0;
            circleVerts[i*2]   = s2*1.6f*cos(a);
            circleVerts[i*2+1] = s2*1.6f*sin(a);
        }
        glVertexPointer(2, GL_FLOAT, 0, circleVerts);
        glDrawArrays(GL_LINE_LOOP, 0, 36);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    delete[] arr;
}

void GLViewer::mousePressEvent(QMouseEvent* e) {
    m_lastPos = e->pos(); m_dragging = true;

    // Ctrl+左键 → 标记锚点拾取，在 paintGL 中处理
    if ((e->modifiers() & Qt::ControlModifier) && e->button() == Qt::LeftButton && !m_verts.isEmpty()) {
        m_pickPos = e->position();
        m_pendingPick = true;
        update();  // 触发 paintGL → 读取 depth → 计算锚点
    }
}

void GLViewer::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    float dx = e->position().x() - m_lastPos.x();
    float dy = e->position().y() - m_lastPos.y();
    if (e->buttons() & Qt::LeftButton) {
        // 左键 → 旋转
        m_rotY += dx * 0.4f;
        m_rotX += dy * 0.4f;
    } else if (e->buttons() & Qt::MiddleButton) {
        // 中键 → 平移
        m_panX += dx * 0.005f * m_modelSize / m_zoom;
        m_panY -= dy * 0.005f * m_modelSize / m_zoom;
    }
    m_lastPos = e->pos();
    update();
}

void GLViewer::wheelEvent(QWheelEvent* e) {
    if (e->angleDelta().y() > 0) m_zoom = qMin(m_zoom * 1.15f, 100.0f);
    else m_zoom = qMax(m_zoom * 0.85f, 0.01f);
    update();
}

// ════════════════════════════════════════════════════════════
//  Model3DViewer
// ════════════════════════════════════════════════════════════

Model3DViewer::Model3DViewer(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_gl = new GLViewer(this);
    layout->addWidget(m_gl, 1);

    auto* btnRow = new QHBoxLayout();
    m_btnResetView = new QPushButton("\u2191 \u590D\u4F4D\u89C6\u89D2", this);
    m_btnResetView->setFixedHeight(24);
    m_btnResetView->setStyleSheet(
        "QPushButton { background:#2196F3; color:white; border:none; "
        "border-radius:3px; padding:2px 12px; font-size:12px; }"
        "QPushButton:hover { background:#1976D2; }");
    connect(m_btnResetView, &QPushButton::clicked, m_gl, &GLViewer::resetView);
    btnRow->addStretch();
    btnRow->addWidget(m_btnResetView);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    m_status = new QLabel("\u672A\u52A0\u8F7D\u6A21\u578B\u3002\u70B9\u51FB\u300C\u6253\u5F00\u6A21\u578B\u300D\u9009\u62E9 STEP \u6587\u4EF6\u3002", this);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#999; font-size:12px; padding:6px; background:#333;");
    m_status->setMinimumHeight(36);
    m_status->setMaximumHeight(60);
    layout->addWidget(m_status);
}

void Model3DViewer::loadFile(const QString& filePath) {
    LOG("3D", "Load: " + filePath);
    m_status->setText("\u89E3\u6790\u4E2D...");

    StepMesh mesh = StepReader::parse(filePath);
    if (mesh.success && !mesh.vertices.isEmpty()) {
        m_gl->loadMesh(mesh.vertices, mesh.indices, mesh.triangles);
        int nTri = mesh.triangles.size() / 3;
        m_status->setText(QString("%1 \u9876\u70B9, %2 \u8FB9, %3 \u4E09\u89D2\u9762")
            .arg(mesh.vertices.size()).arg(mesh.indices.size()/2).arg(nTri));
        m_status->setStyleSheet("color:#4CAF50; font-size:12px; padding:4px; background:#333;");
        LOG("3D", "OK: " + QString::number(mesh.vertices.size()) + " verts, " + QString::number(nTri) + " tris");
    } else {
        m_gl->clear();
        m_status->setText("\u8BFB\u53D6\u5931\u8D25:\n" + mesh.error);
        m_status->setStyleSheet("color:#f44336; font-size:12px; padding:4px; background:#333;");
        LOG("3D", "FAIL: " + mesh.error);
    }
}

void Model3DViewer::clear() {
    m_gl->clear();
    m_status->setText("\u672A\u52A0\u8F7D\u6A21\u578B");
    m_status->setStyleSheet("color:#999; font-size:12px; padding:4px; background:#333;");
}
