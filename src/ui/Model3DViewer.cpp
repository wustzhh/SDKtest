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
//  GLViewer
// ═══════════════════════════════════════════════════════════════
GLViewer::GLViewer(QWidget* p):QOpenGLWidget(p){setMinimumSize(200,150);setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
    // 默认等角视角：让平面模型也能看清
    m_rot = QQuaternion::fromAxisAndAngle(QVector3D(0,1,0), -35)
          * QQuaternion::fromAxisAndAngle(QVector3D(1,0,0), -25);
}
void GLViewer::loadMesh(const QVector<QVector3D>& v,const QVector<int>& t,const QVector<QVector3D>& n,const QVector<EdgeLine>& e,const QVector<int>& fi,const QVector<QVector3D>& fc,const QVector<int>& fci,const QVector<FaceBBox>& fbb){
    m_verts=v;m_tri=t;m_normals=n;m_edges=e;m_faceIds=fi;m_faceCenters=fc;m_faceCenterIds=fci;m_faceBBoxes=fbb;
    // 检查顶点是否有NaN
    int nanCount = 0;
    for (const auto& vv : v) {
        if (std::isnan(vv.x()) || std::isnan(vv.y()) || std::isnan(vv.z())) nanCount++;
    }
    if (nanCount > 0) LOG("3D", QString("WARNING: %1/%2 vertices are NaN!").arg(nanCount).arg(v.size()));
    LOG("3D",QString("Faces=%1 Centers: first=(%2,%3,%4) last=(%5,%6,%7)")
        .arg(fc.size())
        .arg(fc.size()>0?fc[0].x():0,0,'f',3).arg(fc.size()>0?fc[0].y():0,0,'f',3).arg(fc.size()>0?fc[0].z():0,0,'f',3)
        .arg(fc.size()>0?fc[fc.size()-1].x():0,0,'f',3).arg(fc.size()>0?fc[fc.size()-1].y():0,0,'f',3).arg(fc.size()>0?fc[fc.size()-1].z():0,0,'f',3));
    float mx=1e9,my=1e9,mz=1e9,Mx=-1e9,My=-1e9,Mz=-1e9;
    for(auto& vv:v){if(vv.x()<mx)mx=vv.x();if(vv.x()>Mx)Mx=vv.x();if(vv.y()<my)my=vv.y();if(vv.y()>My)My=vv.y();if(vv.z()<mz)mz=vv.z();if(vv.z()>Mz)Mz=vv.z();}
    m_modelSize=qMax(qMax(Mx-mx,My-my),Mz-mz);m_modelSize=qMax(m_modelSize,.001f);
    m_anchor=QVector3D((mx+Mx)/2,(my+My)/2,(mz+Mz)/2);m_defaultAnchor=m_anchor;m_hasAnchor=false;resetView();
    LOG("3D",QString("AABB: X=[%1,%2] Y=[%3,%4] Z=[%5,%6] size=%7 anchor=(%8,%9,%10)")
        .arg(mx,0,'f',3).arg(Mx,0,'f',3).arg(my,0,'f',3).arg(My,0,'f',3)
        .arg(mz,0,'f',3).arg(Mz,0,'f',3).arg(m_modelSize,0,'f',3)
        .arg(m_anchor.x(),0,'f',3).arg(m_anchor.y(),0,'f',3).arg(m_anchor.z(),0,'f',3));
    // 所有面 AABB（调试用，量大时关闭）
#if 0
    for (const auto& b : fbb) {
        QVector<double> v = {b.minX,b.minY,b.minZ,b.maxX,b.maxY,b.maxZ};
        std::sort(v.begin(), v.end());
        LOG("3D",QString("  faceAABB: [%1, %2, %3, %4, %5, %6]")
            .arg(v[0],0,'f',3).arg(v[1],0,'f',3).arg(v[2],0,'f',3)
            .arg(v[3],0,'f',3).arg(v[4],0,'f',3).arg(v[5],0,'f',3));
    }
#endif
    // 填充 VBO 缓存
    m_vaCache.resize(m_verts.size() * 3);
    m_naCache.resize(m_normals.size() * 3);
    for (int i = 0; i < m_verts.size(); i++) {
        m_vaCache[i*3] = m_verts[i].x(); m_vaCache[i*3+1] = m_verts[i].y(); m_vaCache[i*3+2] = m_verts[i].z();
    }
    for (int i = 0; i < m_normals.size(); i++) {
        m_naCache[i*3] = m_normals[i].x(); m_naCache[i*3+1] = m_normals[i].y(); m_naCache[i*3+2] = m_normals[i].z();
    }
    m_vboDirty = true;
}
void GLViewer::resetView(){
    m_rot = QQuaternion::fromAxisAndAngle(QVector3D(0,1,0), -35)
          * QQuaternion::fromAxisAndAngle(QVector3D(1,0,0), -25);
    m_zoom=1; m_panX=0; m_panY=0;
    m_anchor = m_defaultAnchor;  // 复位锚点到模型中心
    update();
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
        // 极小模型自动放大
        if (m_modelSize < 1.0f) m_zoom = 0.4f;
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
void GLViewer::setNoDepthEdges(bool on){m_noDepthEdges=on;update();}
void GLViewer::setEdgeWidthPct(float pct){m_edgeWidthPct=qBound(0.01f,pct,2.0f);update();}
void GLViewer::clear(){m_verts.clear();m_tri.clear();m_normals.clear();m_edges.clear();m_faceIds.clear();m_faceCenters.clear();m_faceCenterIds.clear();m_faceBBoxes.clear();m_hlFaces.clear();m_vaCache.clear();m_naCache.clear();m_vboDirty=true;update();}
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
    if (std::isnan(m_anchor.x()) || std::isnan(m_modelSize)) {
        LOG("3D", "ERROR: anchor or modelSize is NaN, skipping render");
        return;
    }
    float as=float(width())/float(height()),sz=m_modelSize*.6f/qMax(m_zoom,.01f);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    double depthRange = sz * 100.0;  // 合理深度范围，确保polygon offset生效
    if(as>1)glOrtho(-sz*as,sz*as,-sz,sz,-depthRange,depthRange);
    else     glOrtho(-sz,sz,-sz/as,sz/as,-depthRange,depthRange);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(m_panX,m_panY,0);glTranslatef(m_anchor.x(),m_anchor.y(),m_anchor.z());
    QMatrix4x4 rmat;rmat.rotate(m_rot);glMultMatrixf(rmat.constData());glTranslatef(-m_anchor.x(),-m_anchor.y(),-m_anchor.z());
    // 构建Qt矩阵用于unproject（不用glGetFloatv避免列/行主序混乱）
    m_mvMat.setToIdentity();
    m_mvMat.translate(m_panX, m_panY, 0);
    m_mvMat.translate(m_anchor);
    m_mvMat.rotate(m_rot);
    m_mvMat.translate(-m_anchor);
    m_pjMat.setToIdentity();
    if (as > 1) m_pjMat.ortho(-sz*as, sz*as, -sz, sz, -depthRange, depthRange);
    else        m_pjMat.ortho(-sz, sz, -sz/as, sz/as, -depthRange, depthRange);
    GLfloat lp0[]={1,1,1,0};glLightfv(GL_LIGHT0,GL_POSITION,lp0);
    GLfloat lp1[]={-1,-1,-.5f,0};glLightfv(GL_LIGHT1,GL_POSITION,lp1);
    glEnable(GL_LIGHTING);
    if(!m_tri.isEmpty()){
        // 首次或数据变更时上传 VBO
        if (m_vboDirty) uploadVBO();
        glBindBuffer(GL_ARRAY_BUFFER, m_vboVerts);
        glVertexPointer(3, GL_FLOAT, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, m_vboNorms);
        glNormalPointer(GL_FLOAT, 0, nullptr);
        glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY);
        if (m_showFaceIds) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
        glColor4f(.75f,.80f,.88f, m_showFaceIds ? .35f : 1.f);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vboIdx);
        glDrawElements(GL_TRIANGLES, m_tri.size(), GL_UNSIGNED_INT, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (m_showFaceIds) glDisable(GL_BLEND);
        glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    // 高亮面（半透明黄色填充，用于属性高亮）
    if(!m_hlFaces.isEmpty()&&!m_tri.isEmpty()){
        QSet<int> hlFaceSet(m_hlFaces.begin(),m_hlFaces.end());
        QVector<int> hlTri; hlTri.reserve(m_tri.size());
        for(int ti=0;ti<m_tri.size()/3;ti++) if(ti<m_faceIds.size()&&hlFaceSet.contains(m_faceIds[ti])){hlTri.append(m_tri[ti*3]);hlTri.append(m_tri[ti*3+1]);hlTri.append(m_tri[ti*3+2]);}
        if(!hlTri.isEmpty()){
            glBindBuffer(GL_ARRAY_BUFFER, m_vboVerts);
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, 0, nullptr);
            glDisable(GL_LIGHTING);glColor3f(1,.85f,.1f);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0f, -1.0f);
            glDrawElements(GL_TRIANGLES,hlTri.size(),GL_UNSIGNED_INT,hlTri.data());
            glDisable(GL_POLYGON_OFFSET_FILL);
            glEnable(GL_LIGHTING);
            glDisableClientState(GL_VERTEX_ARRAY);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
    glDisable(GL_LIGHTING);
    if(!m_edges.isEmpty()){
        if(m_noDepthEdges) glDisable(GL_DEPTH_TEST);
        glBindBuffer(GL_ARRAY_BUFFER, m_vboVerts);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, nullptr);
        float lw=m_edgeWidthPct*width()/100.0f;
        glLineWidth(qMax(0.5f,lw));
        if (lw > 1.0f) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);
        for(const auto& e:m_edges){int idx[2]={e.v0,e.v1};glColor3f(e.color.x(),e.color.y(),e.color.z());glDrawElements(GL_LINES,2,GL_UNSIGNED_INT,idx);}
        glDisableClientState(GL_VERTEX_ARRAY);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        if(m_noDepthEdges) glEnable(GL_DEPTH_TEST);
    }
    // ── 处理锚点拾取（使用当前帧刚渲染的深度缓冲） ──
    if (m_pendingPick) {
        m_pendingPick = false;
        if (!m_tri.isEmpty()) {
            float dpr = devicePixelRatioF();
            int devX = qRound(m_pickPos.x() * dpr);
            int devY = qRound(m_pickPos.y() * dpr);
            int devW = width(), devH = height();
            int glY = devH - devY - 1;
            // 用缓存的MV/P矩阵在近远平面unproject → 模型空间射线
            // 检查缓存的MV/P矩阵
            float mvDet = m_mvMat.determinant();
            float pjDet = m_pjMat.determinant();
            LOG("PICK",QString("mvDet=%1 pjDet=%2")
                .arg(mvDet,0,'f',4).arg(pjDet,0,'f',4));
            QVector3D pNear = QVector3D(devX, glY, 0).unproject(m_mvMat, m_pjMat, QRect(0,0,devW,devH));
            QVector3D pFar  = QVector3D(devX, glY, 1).unproject(m_mvMat, m_pjMat, QRect(0,0,devW,devH));
            QVector3D mOrg = pNear;
            QVector3D mDir = (pFar - pNear);
            float rayLen = mDir.length();
            if (rayLen < 0.001f) { LOG("PICK","ray too short"); }
            else {
            mDir /= rayLen;
            LOG("PICK",QString("pNear=(%1,%2,%3) pFar=(%4,%5,%6) len=%7")
                .arg(pNear.x(),0,'f',1).arg(pNear.y(),0,'f',1).arg(pNear.z(),0,'f',1)
                .arg(pFar.x(),0,'f',1).arg(pFar.y(),0,'f',1).arg(pFar.z(),0,'f',1).arg(rayLen,0,'f',1));
            // 射线-三角求交
            float bestT = 1e30f;
            QVector3D bestPt;
            for (int ti = 0; ti < m_tri.size(); ti += 3) {
                QVector3D v0 = m_verts[m_tri[ti]];
                QVector3D v1 = m_verts[m_tri[ti+1]];
                QVector3D v2 = m_verts[m_tri[ti+2]];
                QVector3D e1 = v1 - v0, e2 = v2 - v0;
                QVector3D h = QVector3D::crossProduct(mDir, e2);
                float a = QVector3D::dotProduct(e1, h);
                if (qAbs(a) < 1e-8f) continue;
                float f = 1.0f / a;
                QVector3D s = mOrg - v0;
                float u = f * QVector3D::dotProduct(s, h);
                if (u < 0 || u > 1) continue;
                QVector3D q = QVector3D::crossProduct(s, e1);
                float v = f * QVector3D::dotProduct(mDir, q);
                if (v < 0 || u + v > 1) continue;
                float t = f * QVector3D::dotProduct(e2, q);
                if (t > 0 && t < bestT) {
                    bestT = t;
                    bestPt = mOrg + mDir * t;
                }
            }
            if (bestT < 1e30f) {
                LOG("PICK",QString("hit bestT=%.2f bestPt=(%.1f,%.1f,%.1f)").arg(bestT).arg(bestPt.x(),0,'f',1).arg(bestPt.y(),0,'f',1).arg(bestPt.z(),0,'f',1));
                QVector3D oldAnchor = m_anchor;
                QVector3D oldPan(m_panX, m_panY, 0);
                // 点击点在旧世界空间的位置
                QVector3D oldWorld = m_rot.rotatedVector(bestPt - oldAnchor) + oldAnchor + oldPan;
                // 设新锚点，调整pan使该点世界位置不变
                m_anchor = bestPt;
                // newWorld(bestPt) = bestPt + newPan, 令其 = oldWorld
                m_panX = oldWorld.x() - bestPt.x();
                m_panY = oldWorld.y() - bestPt.y();
            } else {
                LOG("PICK","ray miss (no triangle hit)");
            }
            } // end if rayLen ok
        }
    }
    // ── Ctrl锚点高亮 ──
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        glColor3f(1, 1, 0);
        float r = m_modelSize * 0.01f / m_zoom;  // 半径1%模型尺寸
        // 3D球体：6个纬度圈×16经度点，用三角形条带
        for (int lat = 0; lat < 6; lat++) {
            float phi1 = lat * M_PI / 6.0f;
            float phi2 = (lat + 1) * M_PI / 6.0f;
            QVector<GLfloat> ring;
            for (int lon = 0; lon <= 16; lon++) {
                float theta = lon * 2.0f * M_PI / 16.0f;
                float sinT = sin(theta), cosT = cos(theta);
                ring.append(m_anchor.x() + r * sin(phi2) * cosT);
                ring.append(m_anchor.y() + r * sin(phi2) * sinT);
                ring.append(m_anchor.z() + r * cos(phi2));
                ring.append(m_anchor.x() + r * sin(phi1) * cosT);
                ring.append(m_anchor.y() + r * sin(phi1) * sinT);
                ring.append(m_anchor.z() + r * cos(phi1));
            }
            glEnableClientState(GL_VERTEX_ARRAY);
            glDisable(GL_LIGHTING);  // 纯色不依赖光照
            glVertexPointer(3, GL_FLOAT, 0, ring.constData());
            glDrawArrays(GL_TRIANGLE_STRIP, 0, ring.size() / 3);
            glDisableClientState(GL_VERTEX_ARRAY);
        }
    }
}


void GLViewer::mousePressEvent(QMouseEvent* e){
    m_lastPos=e->pos();m_dragging=true;
    m_pickPos = e->pos();
    // 仅Ctrl+左键时设置锚点
    m_pendingPick = (e->modifiers() & Qt::ControlModifier) && (e->buttons() & Qt::LeftButton);
    m_arcballFrom = screenToArcball(e->pos());
    update();
}
void GLViewer::mouseMoveEvent(QMouseEvent* e){
    if(!m_dragging)return;
    if(e->buttons()&Qt::LeftButton){
        m_arcballTo = screenToArcball(e->pos());
        QVector3D axis = QVector3D::crossProduct(m_arcballFrom, m_arcballTo);
        float dot = QVector3D::dotProduct(m_arcballFrom, m_arcballTo);
        dot = qBound(-1.0f, dot, 1.0f);
        float angle = acos(dot);
        if (axis.length() > 0.001f && angle > 0.001f) {
            axis.normalize();
            m_rot = QQuaternion::fromAxisAndAngle(axis, angle * 180.0f / M_PI) * m_rot;
            m_rot.normalize();
            m_arcballFrom = m_arcballTo;
        }
    }else if(e->buttons()&Qt::MiddleButton){
        float dx=e->position().x()-m_lastPos.x(), dy=e->position().y()-m_lastPos.y();
        m_panX+=dx*.002f*m_modelSize/m_zoom; m_panY-=dy*.002f*m_modelSize/m_zoom;
    }
    m_lastPos=e->pos();update();
}
void GLViewer::keyPressEvent(QKeyEvent* e){
    if (e->key() == Qt::Key_Control) { m_ctrlHeld = true; update(); }
    QOpenGLWidget::keyPressEvent(e);
}
void GLViewer::keyReleaseEvent(QKeyEvent* e){
    if (e->key() == Qt::Key_Control) { m_ctrlHeld = false; update(); }
    QOpenGLWidget::keyReleaseEvent(e);
}
void GLViewer::wheelEvent(QWheelEvent* e){if(e->angleDelta().y()>0)m_zoom=qMin(m_zoom*1.15f,100.f);else m_zoom=qMax(m_zoom*.85f,.01f);update();}

QImage GLViewer::grabScreenshot() const {
    // Qt 6: QOpenGLWidget::grab() returns QPixmap
    return const_cast<GLViewer*>(this)->grab().toImage();
}

QVector3D GLViewer::screenToArcball(const QPointF& screenPos) const {
    // 将屏幕坐标映射到单位球面（arcball）
    float x = 2.0f * screenPos.x() / width() - 1.0f;
    float y = 1.0f - 2.0f * screenPos.y() / height();
    float r2 = x*x + y*y;
    float z = (r2 < 1.0f) ? sqrt(1.0f - r2) : 0.0f;
    if (r2 > 1.0f) {
        // 球外：归一化到单位圆上
        float r = sqrt(r2);
        x /= r; y /= r;
    }
    return QVector3D(x, y, z);
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
            // GRID: ID CP X Y Z — 支持固定宽度格式（坐标字段之间无空格）
            bool ok; int id = cardParts[1].toInt(&ok);
            if (!ok) { cardParts.clear(); return; }
            double x=0, y=0, z=0;
            if (cardParts.size() >= 5) {
                x = cardParts[2].toDouble();
                y = cardParts[3].toDouble();
                z = cardParts[4].toDouble();
            } else {
                // 固定宽度格式：cols 25-32=X, 33-40=Y, 41-48=Z（每列8字符）
                x = cardLine.mid(24, 8).trimmed().toDouble();
                y = cardLine.mid(32, 8).trimmed().toDouble();
                z = cardLine.mid(40, 8).trimmed().toDouble();
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
        // 不 terminate：避免 OCCT 全局状态被污染导致后续加载死锁
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
    m_btnReset->setFixedSize(32,32);m_btnReset->setToolTip(QString::fromUtf8("\xE5\xA4\x8D\xE4\xBD\x8D\xE8\xA7\x86\xE8\xA7\x92"));
    m_btnReset->setStyleSheet("QPushButton{background:#ffffff;border:1px solid #e2e8f0;border-radius:6px;font-size:16px;padding:0;}QPushButton:hover{background:#f1f5f9;border-color:#cbd5e1;}");
    connect(m_btnReset,&QPushButton::clicked,m_gl,&GLViewer::resetView);
    m_btnShowFaceIds=new QPushButton(QString::fromUtf8("\xE2\x97\x8F"),this);
    m_btnShowFaceIds->setFixedSize(32,32);m_btnShowFaceIds->setCheckable(true);
    m_btnShowFaceIds->setToolTip(QString::fromUtf8("\xe7\xba\xbf\xe6\xa1\x86\xe7\xa9\xbf\xe9\x80\x8f\xe6\xa8\xa1\xe5\xbc\x8f"));
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
void Model3DViewer::cancelLoad(){m_countdownTimer->stop();m_timeoutTimer->stop();m_busyLoading=false;
    if(m_workerThread){m_workerThread->requestInterruption();m_workerThread->quit();m_workerThread->wait(2000);}
    if(m_worker){m_worker->deleteLater();m_worker=nullptr;}m_workerThread=nullptr;}
void Model3DViewer::loadFile(const QString& fp){
    cancelLoad();m_gl->clear();
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
        applyPendingBoxes();
        emit modelLoaded();
        return;
    }

    LOG("3D","Load: "+fp);m_status->setText(QString::fromUtf8("\xE5\x8A\xA0\xE8\xBD\xBD\xE4\xB8\xAD..."));
#ifndef HAS_OCC
    m_status->setText("OCCT not available");LOG("3D","OCCT not available");return;
#endif
    // 加载互斥：上一个 worker 若因 BRepMesh 卡死还没终止，拒绝新加载，避免叠加
    if (m_busyLoading) {
        LOG("3D","busy loading, reject new load: "+fp);
        m_status->setText(QString::fromUtf8("\xE5\x8A\xA0\xE8\xBD\xBD\xE4\xB8\xAD\xEF\xBC\x8C\xE8\xAF\xB7\xE7\xAD\x89\xE5\xBE\x85"));
        return;
    }
    m_busyLoading = true;
    m_worker=new StepWorker(fp);m_workerThread=new QThread(this);m_worker->moveToThread(m_workerThread);
    connect(m_workerThread,&QThread::started,m_worker,&StepWorker::doWork);
    connect(m_worker,&StepWorker::progress,this,[this](const QString& t){m_status->setText(t);});
    connect(m_worker,&StepWorker::finished,this,[this](const StepLoadResult& r){
        m_countdownTimer->stop();m_timeoutTimer->stop();
        if(r.ok){LOG("3D",QString("OK %1v %2t %3e %4ms").arg(r.verts.size()).arg(r.tris.size()/3).arg(r.edges.size()).arg(r.elapsedMs));
            m_gl->loadMesh(r.verts,r.tris,r.normals,r.edges,r.faceIds,r.faceCenters,r.faceCenterIds,r.faceBBoxes);
            m_gl->setShape(r.shape);
            applyPendingBoxes();
            m_status->setText(QString("OCCT: %1v %2t %3e").arg(r.verts.size()).arg(r.tris.size()/3).arg(r.edges.size()));
            m_status->setStyleSheet("color:#10b981;font-size:12px;padding:8px;background:#f0fdf4;border:1px solid #d1fae5;border-radius:6px;");
            emit modelLoaded();}
        else{m_status->setText(r.error);m_status->setStyleSheet("color:#ef4444;font-size:12px;padding:8px;background:#fef2f2;border:1px solid #fecaca;border-radius:6px;");LOG("3D","FAIL: "+r.error);emit modelLoaded();}
        m_busyLoading=false;
        if(m_workerThread){m_workerThread->quit();m_workerThread->wait();m_workerThread=nullptr;}if(m_worker){m_worker->deleteLater();m_worker=nullptr;}
    });
    connect(m_workerThread,&QThread::finished,this,[this](){if(m_worker){m_worker->deleteLater();m_worker=nullptr;}});
    connect(m_timeoutTimer,&QTimer::timeout,this,[this](){
        LOG("3D","TIMEOUT 30s");m_countdownTimer->stop();
        // 不 terminate：OCCT 线程被杀会污染全局状态导致后续加载死锁。
        // 保留线程后台运行（虽泄漏但 UI 不卡死），加载互斥阻止新加载叠加。
        if(m_workerThread){m_workerThread->requestInterruption();}
        m_busyLoading=false;
        m_status->setText(QString::fromUtf8("\xE8\xB6\x85\xE6\x97\xB6\xEF\xBC\x88")+"30s"+QString::fromUtf8("\xEF\xBC\x89"));m_status->setStyleSheet("color:#ef4444;font-size:12px;padding:8px;background:#fef2f2;border:1px solid #fecaca;border-radius:6px;");
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
void Model3DViewer::toggleFaceIds(){bool on=m_btnShowFaceIds->isChecked();m_gl->setNoDepthEdges(on);}

void GLViewer::uploadVBO() {
    makeCurrent();
    // 删除旧 VBO
    if (m_vboVerts) { glDeleteBuffers(1, &m_vboVerts); m_vboVerts = 0; }
    if (m_vboNorms) { glDeleteBuffers(1, &m_vboNorms); m_vboNorms = 0; }
    if (m_vboIdx)   { glDeleteBuffers(1, &m_vboIdx);   m_vboIdx = 0; }
    if (m_vaCache.isEmpty() || m_tri.isEmpty()) { m_vboDirty = false; return; }

    // 上传顶点
    glGenBuffers(1, &m_vboVerts);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboVerts);
    glBufferData(GL_ARRAY_BUFFER, m_vaCache.size() * sizeof(float), m_vaCache.constData(), GL_STATIC_DRAW);

    // 上传法线
    glGenBuffers(1, &m_vboNorms);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboNorms);
    glBufferData(GL_ARRAY_BUFFER, m_naCache.size() * sizeof(float), m_naCache.constData(), GL_STATIC_DRAW);

    // 上传索引
    glGenBuffers(1, &m_vboIdx);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vboIdx);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_tri.size() * sizeof(int), m_tri.constData(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m_vboDirty = false;
}

#include "Model3DViewer.moc"
