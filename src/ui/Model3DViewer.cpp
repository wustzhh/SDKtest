#define GL_SILENCE_DEPRECATION
#include <GL/gl.h>
#include "Model3DViewer.h"
#include "core/StepReader.h"
#include "core/Logger.h"

#include <QtMath>
#include <QHBoxLayout>

// ════════════════════════════════════════════════════════════
//  GLViewer
// ════════════════════════════════════════════════════════════

GLViewer::GLViewer(QWidget* parent) : QOpenGLWidget(parent) {
    setMinimumSize(200, 150);
}

void GLViewer::loadMesh(const QVector<QVector3D>& verts, const QVector<int>& indices) {
    m_verts = verts;
    m_idx = indices;
    // 计算模型包围球半径
    float cx=0, cy=0, cz=0;
    for (const auto& v : verts) { cx+=v.x(); cy+=v.y(); cz+=v.z(); }
    if (!verts.isEmpty()) { cx/=verts.size(); cy/=verts.size(); cz/=verts.size(); }
    float maxDist = 0;
    for (const auto& v : verts) {
        float d = QVector3D(v.x()-cx, v.y()-cy, v.z()-cz).length();
        if (d > maxDist) maxDist = d;
    }
    m_modelRadius = qMax(maxDist, 0.001f);
    resetView();
}

void GLViewer::clear() {
    m_verts.clear();
    m_idx.clear();
    update();
}

void GLViewer::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void GLViewer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void GLViewer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_idx.isEmpty() || m_verts.isEmpty())
        return;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = float(width()) / float(height());
    float s = 2.0f / m_zoom;
    if (aspect > 1) glOrtho(-s*aspect, s*aspect, -s, s, -100, 100);
    else glOrtho(-s, s, -s/aspect, s/aspect, -100, 100);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(m_rotX, 1, 0, 0);
    glRotatef(m_rotY, 0, 1, 0);

    // Center
    float cx=0, cy=0, cz=0;
    for (const auto& v : m_verts) { cx+=v.x(); cy+=v.y(); cz+=v.z(); }
    if (!m_verts.isEmpty()) { cx/=m_verts.size(); cy/=m_verts.size(); cz/=m_verts.size(); }
    glTranslatef(-cx, -cy, -cz);

    glEnableClientState(GL_VERTEX_ARRAY);
    float* arr = new float[m_verts.size() * 3];
    for (int i = 0; i < m_verts.size(); ++i) {
        arr[i*3] = m_verts[i].x();
        arr[i*3+1] = m_verts[i].y();
        arr[i*3+2] = m_verts[i].z();
    }
    glVertexPointer(3, GL_FLOAT, 0, arr);

    // Wireframe only (edges from EDGE_CURVE)
    glColor3f(0.3f, 0.6f, 1.0f);
    glLineWidth(1.5f);
    glDrawElements(GL_LINES, m_idx.size(), GL_UNSIGNED_INT, m_idx.data());

    glDisableClientState(GL_VERTEX_ARRAY);
    delete[] arr;
}

void GLViewer::resetView() {
    m_rotX = 30; m_rotY = -30;
    // zoom = viewport_size / (2 * model_radius) 的近似值
    // 增加 0.6 系数留边距
    m_zoom = 0.6f / qMax(m_modelRadius, 0.001f);
    update();
}

void GLViewer::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_lastPos = e->pos();
        m_dragging = true;
    }
}

void GLViewer::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    m_rotY += (e->position().x() - m_lastPos.x()) * 0.5f;
    m_rotX += (e->position().y() - m_lastPos.y()) * 0.5f;
    m_rotX = qBound(-90.0f, m_rotX, 90.0f);
    m_lastPos = e->pos();
    update();
}

void GLViewer::wheelEvent(QWheelEvent* e) {
    m_zoom *= (e->angleDelta().y() > 0) ? 1.1f : 0.9f;
    m_zoom = qBound(0.01f, m_zoom, 5000.0f);
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
    m_btnResetView = new QPushButton("复位视角", this);
    m_btnResetView->setFixedHeight(22);
    m_btnResetView->setStyleSheet(
        "QPushButton { background:#2196F3; color:white; border:none; "
        "border-radius:3px; padding:2px 10px; font-size:11px; }"
        "QPushButton:hover { background:#1976D2; }");
    connect(m_btnResetView, &QPushButton::clicked, m_gl, &GLViewer::resetView);
    btnRow->addStretch();
    btnRow->addWidget(m_btnResetView);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    m_status = new QLabel("未加载模型。点击「打开模型」选择 STEP 文件。", this);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#999; font-size:12px; padding:6px; background:#f5f5f5;");
    m_status->setMinimumHeight(40);
    m_status->setMaximumHeight(80);
    layout->addWidget(m_status);
}

void Model3DViewer::loadFile(const QString& filePath) {
    LOG("3D", "Load: " + filePath);
    m_status->setText("解析中...");

    StepMesh mesh = StepReader::parse(filePath);
    if (mesh.success && !mesh.vertices.isEmpty()) {
        m_gl->loadMesh(mesh.vertices, mesh.indices);
        m_status->setText(QString("%1 顶点, %2 条边")
            .arg(mesh.vertices.size()).arg(mesh.indices.size() / 3));
        m_status->setStyleSheet("color:#2e7d32; font-size:12px; padding:4px;");
        LOG("3D", "OK: " + QString::number(mesh.vertices.size()) + " verts");
    } else {
        m_gl->clear();
        m_status->setText("读取失败:\n" + mesh.error);
        m_status->setStyleSheet("color:#c62828; font-size:12px; padding:4px;");
        LOG("3D", "FAIL: " + mesh.error);
    }
}

void Model3DViewer::clear() {
    m_gl->clear();
    m_status->setText("未加载模型");
    m_status->setStyleSheet("color:#999; font-size:12px; padding:4px;");
}
