// ═══════════════════════════════════════════════════════════════
//  shared_topo_detect — 共享拓扑检测工具
//  对每个模型加载后统计：
//    - unique TShape 数 vs 实例数（顶点/边/面），差值=重复引用(dup)
//    - shared_faces / shared_edges / shared_vertices：
//      同一 TShape 被 >=2 个 solid 引用（真正的共享拓扑）
//    - nonmanifold_edges：一条边被 >=3 个面引用
//  输出 jsonl + 按共享程度排序的 top 列表（stdout）。
//
//  用法：shared_topo_detect [rootDir] [outDir] [maxSizeMB]
// ═══════════════════════════════════════════════════════════════

#include <STEPControl_Reader.hxx>
#include <IGESControl_Reader.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Curve.hxx>
#include <BRepExtrema_DistShapeShape.hxx>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <cstdio>
#include <cstdint>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

struct SharedStats {
    std::string name, format, status = "ok", error;
    long long solids = 0, shells = 0, faces = 0, edges = 0, vertices = 0;
    long long uniqueV = 0, uniqueE = 0, uniqueF = 0;
    long long dupV = 0, dupE = 0, dupF = 0;      // 实例数 - unique TShape 数
    long long sharedFaces = 0, sharedEdges = 0, sharedVertices = 0;  // 被>=2个solid引用
    long long nonmanifoldEdges = 0;              // 边被>=3个面引用
    long long sharedCurves = 0;                  // Geom_Curve 句柄被 >=2 条独立边共享（Handle key，真实共享）
    long long touchingPairs = 0;                 // solid 对最小距离 <= 0.001（几何贴合）
    long long touchingSolids = 0;                // 与其他 solid 贴合的 solid 数
    long long crossSolidCurves = 0;              // 一条共享曲线覆盖 >=2 个 solid
    long long maxCurveShare = 0;                 // 单条曲线最多被几条边共享
    long long elapsedMs = 0;
    std::atomic<bool> done{false};
};

static void detect(const std::string& path, const std::string& format, SharedStats& s) {
    auto t0 = Clock::now();
    TopoDS_Shape shape;
    try {
        if (format == "step") {
            STEPControl_Reader r;
            if (r.ReadFile(path.c_str()) != IFSelect_RetDone) { s.status="error"; s.error="ReadFile failed"; s.done=true; return; }
            if (r.TransferRoots() == 0) { s.status="error"; s.error="no roots"; s.done=true; return; }
            shape = r.OneShape();
        } else {
            IGESControl_Reader r;
            if (r.ReadFile(path.c_str()) != IFSelect_RetDone) { s.status="error"; s.error="ReadFile failed"; s.done=true; return; }
            if (r.TransferRoots() == 0) { s.status="error"; s.error="no roots"; s.done=true; return; }
            shape = r.OneShape();
        }
    } catch (...) { s.status="error"; s.error="exception"; s.done=true; return; }
    if (shape.IsNull()) { s.status="error"; s.error="null shape"; s.done=true; return; }

    TopTools_IndexedMapOfShape mV, mE, mF, mS, mSh;
    TopExp::MapShapes(shape, TopAbs_VERTEX, mV);
    TopExp::MapShapes(shape, TopAbs_EDGE, mE);
    TopExp::MapShapes(shape, TopAbs_FACE, mF);
    TopExp::MapShapes(shape, TopAbs_SOLID, mS);
    TopExp::MapShapes(shape, TopAbs_SHELL, mSh);
    s.vertices = mV.Extent(); s.edges = mE.Extent(); s.faces = mF.Extent();
    s.solids = mS.Extent(); s.shells = mSh.Extent();
    if (getenv("SHARED_DEBUG")) {
        fprintf(stderr, "[dbg] %s: inst v=%d e=%d f=%d s=%d\n",
                path.c_str(), mV.Extent(), mE.Extent(), mF.Extent(), mS.Extent());
    }

    // unique TShape 数 + 实例数（TopExp_Explorer 遍历，含 orientation/location 重复）
    std::set<const void*> uV, uE, uF;
    long long instV = 0, instE = 0, instF = 0;
    for (TopExp_Explorer x(shape, TopAbs_VERTEX); x.More(); x.Next()) { ++instV; uV.insert(x.Current().TShape().get()); }
    for (TopExp_Explorer x(shape, TopAbs_EDGE); x.More(); x.Next())   { ++instE; uE.insert(x.Current().TShape().get()); }
    for (TopExp_Explorer x(shape, TopAbs_FACE); x.More(); x.Next())   { ++instF; uF.insert(x.Current().TShape().get()); }
    s.vertices = (long long)uV.size(); s.edges = (long long)uE.size(); s.faces = (long long)uF.size();
    s.uniqueV = (long long)uV.size(); s.uniqueE = (long long)uE.size(); s.uniqueF = (long long)uF.size();
    s.dupV = instV - s.uniqueV; s.dupE = instE - s.uniqueE; s.dupF = instF - s.uniqueF;
    if (getenv("SHARED_DEBUG"))
        fprintf(stderr, "[dbg] %s: unique v=%lld e=%lld f=%lld  dup v=%lld e=%lld f=%lld\n",
                path.c_str(), s.uniqueV, s.uniqueE, s.uniqueF, s.dupV, s.dupE, s.dupF);

    // edge → 引用它的 face 数（非流形检测）
    std::map<const void*, int> edgeFaceCount;
    for (int i = 1; i <= mE.Extent(); ++i) edgeFaceCount[mE(i).TShape().get()] = 0;
    for (int i = 1; i <= mF.Extent(); ++i) {
        TopExp_Explorer eExp(mF(i), TopAbs_EDGE);
        for (; eExp.More(); eExp.Next()) edgeFaceCount[eExp.Current().TShape().get()]++;
    }
    for (const auto& kv : edgeFaceCount) if (kv.second >= 3) ++s.nonmanifoldEdges;

    // face/edge/vertex → 所属 solid 集合
    std::map<const void*, std::set<int>> faceSolid, edgeSolid, vertexSolid;
    int sid = 0;
    TopExp_Explorer sExp(shape, TopAbs_SOLID);
    for (; sExp.More(); sExp.Next(), ++sid) {
        const TopoDS_Shape& solid = sExp.Current();
        TopExp_Explorer fExp(solid, TopAbs_FACE);
        for (; fExp.More(); fExp.Next()) faceSolid[fExp.Current().TShape().get()].insert(sid);
        TopExp_Explorer eExp(solid, TopAbs_EDGE);
        for (; eExp.More(); eExp.Next()) edgeSolid[eExp.Current().TShape().get()].insert(sid);
        TopExp_Explorer vExp(solid, TopAbs_VERTEX);
        for (; vExp.More(); vExp.Next()) vertexSolid[vExp.Current().TShape().get()].insert(sid);
    }
    for (const auto& kv : faceSolid)   if (kv.second.size() >= 2) ++s.sharedFaces;
    for (const auto& kv : edgeSolid)   if (kv.second.size() >= 2) ++s.sharedEdges;
    for (const auto& kv : vertexSolid) if (kv.second.size() >= 2) ++s.sharedVertices;

    // Geom_Curve 句柄级共享：多条独立边（不同 TShape）共用同一曲线几何
    std::map<Handle(Geom_Curve), std::set<int>> curveEdges;
    std::map<Handle(Geom_Curve), std::set<int>> curveSolids;
    for (int i = 1; i <= mE.Extent(); ++i) {
        double f, l;
        Handle(Geom_Curve) c = BRep_Tool::Curve(TopoDS::Edge(mE(i)), f, l);
        if (c.IsNull()) continue;
        curveEdges[c].insert(i);
        auto it = edgeSolid.find(mE(i).TShape().get());
        if (it != edgeSolid.end()) curveSolids[c].insert(it->second.begin(), it->second.end());
    }
    if (getenv("SHARED_DEBUG")) {
        // 对比：裸指针 key 的结果
        std::map<const void*, std::set<int>> raw;
        for (int i = 1; i <= mE.Extent(); ++i) {
            double f, l;
            Handle(Geom_Curve) c = BRep_Tool::Curve(TopoDS::Edge(mE(i)), f, l);
            if (c.IsNull()) continue;
            raw[c.get()].insert(i);
        }
        long long rawShared = 0, rawMax = 0;
        for (const auto& kv : raw) if (kv.second.size() >= 2) {
            ++rawShared;
            if ((long long)kv.second.size() > rawMax) rawMax = (long long)kv.second.size();
        }
        fprintf(stderr, "[dbg] %s: handle-map shared=%lld raw-map shared=%lld raw-max=%lld\n",
                path.c_str(), s.sharedCurves, rawShared, rawMax);
        // 打印 handle-map 所有 key 指针 + raw-map 的共享 key 指针
        std::vector<void*> hp;
        for (const auto& kv : curveEdges) hp.push_back((void*)kv.first.get());
        std::sort(hp.begin(), hp.end());
        fprintf(stderr, "[dbg] handle keys: ");
        for (void* p : hp) fprintf(stderr, "%p ", p);
        fprintf(stderr, "\n[dbg] raw shared keys: ");
        for (const auto& kv : raw) if (kv.second.size() >= 2) fprintf(stderr, "%p(%d) ", kv.first, (int)kv.second.size());
        fprintf(stderr, "\n");
    }
    for (const auto& kv : curveEdges) {
        if (kv.second.size() >= 2) {
            ++s.sharedCurves;
            if ((long long)kv.second.size() > s.maxCurveShare) s.maxCurveShare = (long long)kv.second.size();
        }
    }
    if (getenv("SHARED_DEBUG")) {
        fprintf(stderr, "[dbg] %s: shared_curves=%lld max_share=%lld\n",
                path.c_str(), s.sharedCurves, s.maxCurveShare);
    }
    for (const auto& kv : curveSolids) {
        if (curveEdges[kv.first].size() >= 2 && kv.second.size() >= 2) ++s.crossSolidCurves;
    }

    // solid 两两贴合检测（共享拓扑的实用信号：多体几何接触）
    std::vector<TopoDS_Shape> solidList;
    for (int i = 1; i <= mS.Extent(); ++i) solidList.push_back(mS(i));
    for (size_t i = 0; i < solidList.size(); ++i)
        for (size_t j = i + 1; j < solidList.size(); ++j) {
            BRepExtrema_DistShapeShape d(solidList[i], solidList[j]);
            if (d.IsDone() && d.Value() <= 0.001) ++s.touchingPairs;
        }

    s.elapsedMs = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    s.done = true;
}

static std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c < 0x20) { char b[8]; snprintf(b, 8, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o;
}

static std::string toJson(const SharedStats& s) {
    char b[1280];
    snprintf(b, sizeof b,
        "{\"name\":\"%s\",\"format\":\"%s\",\"status\":\"%s\",\"error\":\"%s\","
        "\"solids\":%lld,\"shells\":%lld,\"faces\":%lld,\"edges\":%lld,\"vertices\":%lld,"
        "\"unique_v\":%lld,\"unique_e\":%lld,\"unique_f\":%lld,"
        "\"dup_v\":%lld,\"dup_e\":%lld,\"dup_f\":%lld,"
        "\"shared_faces\":%lld,\"shared_edges\":%lld,\"shared_vertices\":%lld,"
        "\"nonmanifold_edges\":%lld,"
        "\"shared_curves\":%lld,\"cross_solid_curves\":%lld,\"max_curve_share\":%lld,"
        "\"touching_pairs\":%lld,"
        "\"elapsed_ms\":%lld}",
        jsonEscape(s.name).c_str(), s.format.c_str(), s.status.c_str(), jsonEscape(s.error).c_str(),
        s.solids, s.shells, s.faces, s.edges, s.vertices,
        s.uniqueV, s.uniqueE, s.uniqueF,
        s.dupV, s.dupE, s.dupF,
        s.sharedFaces, s.sharedEdges, s.sharedVertices,
        s.nonmanifoldEdges,
        s.sharedCurves, s.crossSolidCurves, s.maxCurveShare,
        s.touchingPairs,
        s.elapsedMs);
    return b;
}

int main(int argc, char** argv) {
    std::string root = "D:/pyProj/SDKtest/pack/sdk/test/hwGeomTests";
    std::string outDir = "D:/pyProj/SDKtest/modelsInfo";
    long long maxMB = 10;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (i == 1) root = a;
        else if (i == 2) outDir = a;
        else if (i == 3) maxMB = std::atoll(a.c_str());
    }
    long long maxBytes = maxMB * 1024LL * 1024LL;
    const int timeoutSec = 300;

    std::vector<std::pair<std::string, std::string>> files;  // path, rel
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        if (ext != ".step" && ext != ".stp" && ext != ".igs" && ext != ".iges") continue;
        if ((long long)it->file_size(ec) > maxBytes) continue;
        files.emplace_back(it->path().string(), fs::relative(it->path(), root).generic_string());
    }
    std::cerr << "[scan] " << files.size() << " files\n";

    fs::create_directories(outDir, ec);
    std::ofstream out(fs::path(outDir) / "shared_topo.jsonl");
    struct Row { std::string json; long long touchingPairs, sharedFaces, sharedEdges, sharedVertices, dupF, dupE, nonmanifold, sharedCurves, crossSolidCurves, maxCurveShare; };
    std::vector<Row> rows;

    for (const auto& f : files) {
        auto st = std::make_shared<SharedStats>();
        st->name = f.second;
        st->format = (f.second.rfind(".igs") != std::string::npos || f.second.rfind(".iges") != std::string::npos) ? "igs" : "step";
        std::thread t([&f, st]() { detect(f.first, st->format, *st); });
        bool fin = false;
        for (int w = 0; w < timeoutSec; ++w) {
            if (st->done.load()) { fin = true; break; }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (fin) t.join();
        else { t.detach(); st->status = "timeout"; }
        std::string j = toJson(*st);
        out << j << "\n" << std::flush;
        rows.push_back({j, st->touchingPairs, st->sharedFaces, st->sharedEdges, st->sharedVertices, st->dupF, st->dupE, st->nonmanifoldEdges,
                        st->sharedCurves, st->crossSolidCurves, st->maxCurveShare});
    }
    out.close();

    // 排序：solid 贴合对数优先（共享拓扑的实用信号），其次共享面/边/顶点
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.touchingPairs != b.touchingPairs) return a.touchingPairs > b.touchingPairs;
        if (a.crossSolidCurves != b.crossSolidCurves) return a.crossSolidCurves > b.crossSolidCurves;
        if (a.sharedCurves != b.sharedCurves) return a.sharedCurves > b.sharedCurves;
        if (a.maxCurveShare != b.maxCurveShare) return a.maxCurveShare > b.maxCurveShare;
        if (a.sharedFaces != b.sharedFaces) return a.sharedFaces > b.sharedFaces;
        if (a.sharedEdges != b.sharedEdges) return a.sharedEdges > b.sharedEdges;
        if (a.sharedVertices != b.sharedVertices) return a.sharedVertices > b.sharedVertices;
        if (a.dupF != b.dupF) return a.dupF > b.dupF;
        if (a.dupE != b.dupE) return a.dupE > b.dupE;
        return a.nonmanifold > b.nonmanifold;
    });
    std::cout << "\n=== top 30 (shared topology) ===\n";
    for (int i = 0; i < (int)rows.size() && i < 30; ++i) {
        if (rows[i].sharedCurves == 0 && rows[i].sharedFaces == 0 && rows[i].sharedEdges == 0
            && rows[i].sharedVertices == 0 && rows[i].dupF == 0 && rows[i].nonmanifold == 0) break;
        std::cout << rows[i].json << "\n";
    }
    std::cout << "[done] " << rows.size() << " models -> " << fs::path(outDir).string() << "/shared_topo.jsonl\n";
    return 0;
}
