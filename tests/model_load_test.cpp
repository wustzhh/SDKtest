// ═══════════════════════════════════════════════════════════════
//  model_load_test.cpp — StepWorker 模型加载测试程序
//  用途：对比新旧 BRepMesh/加载实现下，_ball2 等卡死模型的行为
//  直接调 StepWorker::doWork()，输出各阶段耗时（见日志 stage: 行）
//
//  用法：
//    model_load_test [模型路径]          单个模型
//    MODEL_BATCH="p1;p2" model_load_test  批量模型（分号分隔）
// ═══════════════════════════════════════════════════════════════

#include "core/StepLoader.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDebug>
#include <cstdio>

// 同步执行 StepWorker（信号直连同步，同一线程调用 doWork）
static StepLoadResult runStepWorker(const QString& path) {
    StepLoadResult result;
    QElapsedTimer total; total.start();

    StepWorker worker(path);
    QObject::connect(&worker, &StepWorker::finished, [&](const StepLoadResult& r) {
        result = r;
    });

    worker.doWork();

    fprintf(stderr, "[test] total=%lldms ok=%d verts=%d tris=%d edges=%d\n",
            (long long)total.elapsed(), result.ok ? 1 : 0,
            (int)result.verts.size(), (int)result.tris.size() / 3,
            (int)result.edges.size());
    return result;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QStringList models;
    if (argc > 1) {
        models << QString::fromLocal8Bit(argv[1]);
    } else {
        const char* env = qgetenv("MODEL_BATCH").constData();
        if (env && *env) {
            for (const auto& p : QString(env).split(';', Qt::SkipEmptyParts))
                models << p;
        } else {
            models << "D:/pyProj/HUAWEISDK/test/hwGeomTests/_ball2.step";
        }
    }

    int fail = 0;
    for (const auto& m : models) {
        if (!QFile::exists(m)) {
            fprintf(stderr, "skip (file not found): %s\n", m.toUtf8().constData());
            continue;
        }
        fprintf(stderr, "\n=== load: %s ===\n", m.toUtf8().constData());
        StepLoadResult r = runStepWorker(m);
        fprintf(stderr, "=== done: ok=%d tris=%d elapsed=%dms ===\n",
                r.ok ? 1 : 0, (int)r.tris.size() / 3, r.elapsedMs);
        // 三角形边长统计
        if (r.ok && !r.tris.isEmpty()) {
            double minLen=1e30, maxLen=0, avgLen=0; int n=0;
            for (int i=0; i+2<r.tris.size(); i+=3) {
                QVector3D a=r.verts[r.tris[i]], b=r.verts[r.tris[i+1]], c=r.verts[r.tris[i+2]];
                double len=((a-b).length()+(b-c).length()+(c-a).length())/3.0;
                if(len<minLen)minLen=len; if(len>maxLen)maxLen=len; avgLen+=len; n++;
            }
            if(n) avgLen/=n;
            fprintf(stderr, "  tri边: n=%d min=%.3f max=%.3f avg=%.3f mm\n", n, minLen, maxLen, avgLen);
        }
        if (!r.ok) fail++;
    }
    return fail;
}
