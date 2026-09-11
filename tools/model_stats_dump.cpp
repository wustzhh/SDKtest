// ═══════════════════════════════════════════════════════════════
//  model_stats_dump — 批量模型统计工具
//  扫描目录下所有 .step/.stp/.igs/.iges 模型，用 OCCT 加载后统计
//  点(vertex)/线(edge)/面(face)/体(solid) 拓扑数据与几何类型分布，
//  输出到 modelsInfo/（jsonl 增量 + csv + json 汇总）。
//
//  用法：
//    model_stats_dump [rootDir] [outDir] [maxSizeMB]
//  默认 rootDir = D:/pyProj/SDKtest/pack/sdk/test/hwGeomTests
//  默认 outDir  = D:/pyProj/SDKtest/modelsInfo
//  默认 maxSizeMB = 10（超过该大小的文件标记 skipped_too_large，不加载）
//  单文件加载超时 300s（标记 timeout）
//  已出现在 jsonl 中的文件默认跳过（断点续跑），--fresh 强制重跑
// ═══════════════════════════════════════════════════════════════

#include <STEPControl_Reader.hxx>
#include <IGESControl_Reader.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <GeomAbs_Shape.hxx>
#include <gp_Pnt.hxx>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdint>

namespace fs = std::filesystem;

using Clock = std::chrono::steady_clock;

// ────────────────────────── 数据结构 ──────────────────────────
struct ModelStats {
    std::string name;        // 相对路径
    std::string format;      // step / igs
    std::string status = "ok";  // ok / error / timeout / skipped_too_large
    std::string error;
    std::string shapeType;
    long long sizeBytes = 0;
    long long readMs = 0;    // 读取+转换耗时
    long long statMs = 0;    // 统计耗时
    long long elapsedMs = 0;
    // 拓扑计数（TopExp::MapShapes，含实例）
    long long vertices = 0, edges = 0, faces = 0, wires = 0;
    long long shells = 0, solids = 0, compsolids = 0, compounds = 0;
    long long freeEdges = 0, degenEdges = 0;
    std::map<std::string, long long> faceTypes;  // 面几何类型分布
    std::map<std::string, long long> edgeTypes;  // 边几何类型分布
    double bbox[6] = {0, 0, 0, 0, 0, 0};         // shape 整体包围盒
    double bboxV[6] = {0, 0, 0, 0, 0, 0};        // 顶点 AABB（真实几何范围）
    std::atomic<bool> done{false};
};

static const char* faceTypeName(GeomAbs_SurfaceType t) {
    switch (t) {
        case GeomAbs_Plane:               return "plane";
        case GeomAbs_Cylinder:            return "cylinder";
        case GeomAbs_Cone:                return "cone";
        case GeomAbs_Sphere:              return "sphere";
        case GeomAbs_Torus:               return "torus";
        case GeomAbs_BezierSurface:       return "bezier";
        case GeomAbs_BSplineSurface:      return "bspline";
        case GeomAbs_SurfaceOfRevolution: return "revolution";
        case GeomAbs_SurfaceOfExtrusion:  return "extrusion";
        case GeomAbs_OffsetSurface:       return "offset";
        default:                          return "other";
    }
}

static const char* edgeTypeName(GeomAbs_CurveType t) {
    switch (t) {
        case GeomAbs_Line:          return "line";
        case GeomAbs_Circle:        return "circle";
        case GeomAbs_Ellipse:       return "ellipse";
        case GeomAbs_Hyperbola:     return "hyperbola";
        case GeomAbs_Parabola:      return "parabola";
        case GeomAbs_BezierCurve:   return "bezier";
        case GeomAbs_BSplineCurve:  return "bspline";
        case GeomAbs_OffsetCurve:   return "offset";
        default:                    return "other";
    }
}

static const char* shapeTypeName(TopAbs_ShapeEnum t) {
    switch (t) {
        case TopAbs_COMPOUND:  return "COMPOUND";
        case TopAbs_COMPSOLID: return "COMPSOLID";
        case TopAbs_SOLID:     return "SOLID";
        case TopAbs_SHELL:     return "SHELL";
        case TopAbs_FACE:      return "FACE";
        case TopAbs_WIRE:      return "WIRE";
        case TopAbs_EDGE:      return "EDGE";
        case TopAbs_VERTEX:    return "VERTEX";
        default:               return "SHAPE";
    }
}

// ────────────────────────── JSON 输出 ──────────────────────────
static std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; snprintf(b, 8, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o;
}

static std::string csvField(const std::string& s) {
    bool need = s.find_first_of(",\"\n\r") != std::string::npos;
    if (!need) return s;
    std::string o = "\"";
    for (char c : s) { if (c == '"') o += "\"\""; else o += c; }
    return o + "\"";
}

static void appendTypeMap(std::string& out, const std::map<std::string, long long>& m) {
    out += '{';
    bool first = true;
    for (const auto& kv : m) {
        if (!first) out += ',';
        first = false;
        out += '"' + kv.first + "\":" + std::to_string(kv.second);
    }
    out += '}';
}

static std::string statsToJson(const ModelStats& s) {
    std::string o;
    o += "{\"name\":\"" + jsonEscape(s.name) + "\"";
    o += ",\"format\":\"" + s.format + "\"";
    o += ",\"status\":\"" + s.status + "\"";
    o += ",\"error\":\"" + jsonEscape(s.error) + "\"";
    o += ",\"shape_type\":\"" + s.shapeType + "\"";
    o += ",\"size_bytes\":" + std::to_string(s.sizeBytes);
    o += ",\"read_ms\":" + std::to_string(s.readMs);
    o += ",\"stat_ms\":" + std::to_string(s.statMs);
    o += ",\"elapsed_ms\":" + std::to_string(s.elapsedMs);
    o += ",\"vertices\":" + std::to_string(s.vertices);
    o += ",\"edges\":" + std::to_string(s.edges);
    o += ",\"faces\":" + std::to_string(s.faces);
    o += ",\"wires\":" + std::to_string(s.wires);
    o += ",\"shells\":" + std::to_string(s.shells);
    o += ",\"solids\":" + std::to_string(s.solids);
    o += ",\"compsolids\":" + std::to_string(s.compsolids);
    o += ",\"compounds\":" + std::to_string(s.compounds);
    o += ",\"free_edges\":" + std::to_string(s.freeEdges);
    o += ",\"degen_edges\":" + std::to_string(s.degenEdges);
    o += ",\"face_types\":";
    appendTypeMap(o, s.faceTypes);
    o += ",\"edge_types\":";
    appendTypeMap(o, s.edgeTypes);
    char b[256];
    snprintf(b, sizeof b, ",\"bbox\":[%.6g,%.6g,%.6g,%.6g,%.6g,%.6g]",
             s.bbox[0], s.bbox[1], s.bbox[2], s.bbox[3], s.bbox[4], s.bbox[5]);
    o += b;
    snprintf(b, sizeof b, ",\"bbox_vertices\":[%.6g,%.6g,%.6g,%.6g,%.6g,%.6g]",
             s.bboxV[0], s.bboxV[1], s.bboxV[2], s.bboxV[3], s.bboxV[4], s.bboxV[5]);
    o += b;
    o += '}';
    return o;
}

// ────────────────────────── 加载与统计 ──────────────────────────
static bool gDebug = false;

static void dumpShapeTree(const TopoDS_Shape& s, int depth) {
    std::string indent(depth * 2, ' ');
    std::cerr << indent << shapeTypeName(s.ShapeType()) << "\n";
    if (depth >= 3) return;
    for (TopoDS_Iterator it(s); it.More(); it.Next())
        dumpShapeTree(it.Value(), depth + 1);
}

static void runJob(const std::string& path, const std::string& format, ModelStats& s) {
    auto t0 = Clock::now();
    TopoDS_Shape shape;
    try {
        if (format == "step") {
            STEPControl_Reader reader;
            if (reader.ReadFile(path.c_str()) != IFSelect_RetDone) {
                s.status = "error"; s.error = "ReadFile failed"; s.done = true; return;
            }
            if (reader.TransferRoots() == 0) {
                s.status = "error"; s.error = "no roots transferred"; s.done = true; return;
            }
            shape = reader.OneShape();
        } else {
            IGESControl_Reader reader;
            if (reader.ReadFile(path.c_str()) != IFSelect_RetDone) {
                s.status = "error"; s.error = "ReadFile failed"; s.done = true; return;
            }
            if (reader.TransferRoots() == 0) {
                s.status = "error"; s.error = "no roots transferred"; s.done = true; return;
            }
            shape = reader.OneShape();
        }
    } catch (...) {
        s.status = "error"; s.error = "exception during load"; s.done = true; return;
    }
    if (shape.IsNull()) { s.status = "error"; s.error = "shape is null"; s.done = true; return; }
    s.readMs = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    if (gDebug) dumpShapeTree(shape, 0);
    auto t1 = Clock::now();
    s.shapeType = shapeTypeName(shape.ShapeType());

    TopTools_IndexedMapOfShape mv, me, mf, mw, msh, ms, mcs, mc;
    TopExp::MapShapes(shape, TopAbs_VERTEX, mv);
    TopExp::MapShapes(shape, TopAbs_EDGE, me);
    TopExp::MapShapes(shape, TopAbs_FACE, mf);
    TopExp::MapShapes(shape, TopAbs_WIRE, mw);
    TopExp::MapShapes(shape, TopAbs_SHELL, msh);
    TopExp::MapShapes(shape, TopAbs_SOLID, ms);
    TopExp::MapShapes(shape, TopAbs_COMPSOLID, mcs);
    TopExp::MapShapes(shape, TopAbs_COMPOUND, mc);
    s.vertices = mv.Extent(); s.edges = me.Extent(); s.faces = mf.Extent();
    s.wires = mw.Extent(); s.shells = msh.Extent(); s.solids = ms.Extent();
    s.compsolids = mcs.Extent(); s.compounds = mc.Extent();

    // 边 → 引用它的面集合（用于自由边统计）
    std::map<const void*, std::set<const void*>> edgeFaceMap;
    for (int i = 1; i <= me.Extent(); ++i)
        edgeFaceMap[me(i).TShape().get()];
    for (int i = 1; i <= mf.Extent(); ++i) {
        TopExp_Explorer eExp(mf(i), TopAbs_EDGE);
        for (; eExp.More(); eExp.Next())
            edgeFaceMap[eExp.Current().TShape().get()].insert(mf(i).TShape().get());
    }
    for (const auto& kv : edgeFaceMap)
        if (kv.second.size() <= 1) ++s.freeEdges;

    // 面几何类型 / 边几何类型 / 退化边
    for (int i = 1; i <= mf.Extent(); ++i) {
        BRepAdaptor_Surface ads(TopoDS::Face(mf(i)));
        s.faceTypes[faceTypeName(ads.GetType())]++;
    }
    for (int i = 1; i <= me.Extent(); ++i) {
        TopoDS_Edge e = TopoDS::Edge(me(i));
        if (BRep_Tool::Degenerated(e)) { ++s.degenEdges; continue; }
        BRepAdaptor_Curve ac(e);
        s.edgeTypes[edgeTypeName(ac.GetType())]++;
    }

    // 包围盒：shape 整体 + 顶点 AABB
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    if (!box.IsVoid()) box.Get(s.bbox[0], s.bbox[1], s.bbox[2], s.bbox[3], s.bbox[4], s.bbox[5]);
    double mn[3] = {1e300, 1e300, 1e300}, mx[3] = {-1e300, -1e300, -1e300};
    for (int i = 1; i <= mv.Extent(); ++i) {
        gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(mv(i)));
        for (int k = 0; k < 3; ++k) {
            if (p.Coord(k + 1) < mn[k]) mn[k] = p.Coord(k + 1);
            if (p.Coord(k + 1) > mx[k]) mx[k] = p.Coord(k + 1);
        }
    }
    if (mn[0] < 1e299) {
        s.bboxV[0] = mn[0]; s.bboxV[1] = mn[1]; s.bboxV[2] = mn[2];
        s.bboxV[3] = mx[0]; s.bboxV[4] = mx[1]; s.bboxV[5] = mx[2];
    }
    s.statMs = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t1).count();
    s.elapsedMs = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    s.status = "ok";
    s.done = true;
}

// ────────────────────────── 文件收集 ──────────────────────────
struct FileJob {
    std::string path;       // 绝对路径
    std::string rel;        // 相对 root 的路径
    std::string format;
    long long sizeBytes = 0;
};

static bool collectFiles(const fs::path& root, long long maxBytes,
                         std::vector<FileJob>& jobs, std::vector<FileJob>& skipped) {
    std::error_code ec;
    if (!fs::exists(root, ec)) { std::cerr << "[error] root not found: " << root << "\n"; return false; }
    std::vector<fs::path> files;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (ext != ".step" && ext != ".stp" && ext != ".igs" && ext != ".iges") continue;
        FileJob j;
        j.path = it->path().string();
        j.rel = fs::relative(it->path(), root).generic_string();
        j.format = (ext == ".igs" || ext == ".iges") ? "igs" : "step";
        j.sizeBytes = (long long)it->file_size(ec);
        if (ec) { ec.clear(); continue; }
        if (j.sizeBytes > maxBytes) skipped.push_back(j);
        else jobs.push_back(j);
    }
    std::sort(jobs.begin(), jobs.end(),
              [](const FileJob& a, const FileJob& b) { return a.sizeBytes < b.sizeBytes; });
    return true;
}

// ────────────────────────── 主流程 ──────────────────────────
int main(int argc, char** argv) {
    std::string root = "D:/pyProj/SDKtest/pack/sdk/test/hwGeomTests";
    std::string outDir = "D:/pyProj/SDKtest/modelsInfo";
    long long maxMB = 10;
    bool fresh = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--fresh") fresh = true;
        else if (a == "--debug") gDebug = true;
        else if (a == "--help" || a == "-h") {
            std::cout << "usage: model_stats_dump [rootDir] [outDir] [maxSizeMB] [--fresh]\n";
            return 0;
        } else if (i == 1) root = a;
        else if (i == 2) outDir = a;
        else if (i == 3) maxMB = std::atoll(a.c_str());
    }
    long long maxBytes = maxMB * 1024LL * 1024LL;
    const int timeoutSec = 300;

    std::vector<FileJob> jobs, skipped;
    if (!collectFiles(root, maxBytes, jobs, skipped)) return 1;
    std::cerr << "[scan] found " << jobs.size() << " files to load, "
              << skipped.size() << " too large (skipped)\n";

    std::error_code ec;
    fs::create_directories(outDir, ec);
    fs::path jsonlPath = fs::path(outDir) / "model_stats.jsonl";
    fs::path csvPath = fs::path(outDir) / "model_stats.csv";
    fs::path jsonPath = fs::path(outDir) / "model_stats.json";

    // 断点续跑：读取已完成的 name 集合
    std::set<std::string> doneNames;
    if (!fresh && fs::exists(jsonlPath, ec)) {
        std::ifstream in(jsonlPath);
        std::string line;
        while (std::getline(in, line)) {
            size_t p = line.find("\"name\":\"");
            if (p == std::string::npos) continue;
            p += 8;
            size_t q = line.find('"', p);
            if (q == std::string::npos) continue;
            doneNames.insert(line.substr(p, q - p));
        }
        if (!doneNames.empty()) std::cerr << "[resume] " << doneNames.size() << " already done, skipping\n";
    }

    std::ofstream out(jsonlPath, std::ios::app);
    if (!out) { std::cerr << "[error] cannot open " << jsonlPath << "\n"; return 1; }
    out.flush();

    int okCnt = 0, errCnt = 0, toCnt = 0, skipCnt = 0;

    // 过大文件直接记录
    for (const auto& j : skipped) {
        if (doneNames.count(j.rel)) { ++skipCnt; continue; }
        ModelStats s; s.name = j.rel; s.format = j.format; s.sizeBytes = j.sizeBytes;
        s.status = "skipped_too_large";
        out << statsToJson(s) << "\n" << std::flush;
        ++skipCnt;
        std::cerr << "[skip] " << j.rel << " (" << j.sizeBytes / 1024 / 1024 << "MB)\n";
    }

    for (const auto& j : jobs) {
        if (doneNames.count(j.rel)) { ++skipCnt; continue; }
        auto st = std::make_shared<ModelStats>();
        st->name = j.rel; st->format = j.format; st->sizeBytes = j.sizeBytes;
        std::thread t([j, st]() { runJob(j.path, j.format, *st); });

        bool finished = false;
        for (int waited = 0; waited < timeoutSec; ++waited) {
            if (st->done.load()) { finished = true; break; }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (finished) t.join();
        else { t.detach(); st->status = "timeout"; st->error = "exceeded " + std::to_string(timeoutSec) + "s"; }

        out << statsToJson(*st) << "\n" << std::flush;
        if (st->status == "ok") ++okCnt;
        else if (st->status == "timeout") ++toCnt;
        else ++errCnt;

        std::cerr << "[" << st->status << "] " << j.rel
                  << "  v=" << st->vertices << " e=" << st->edges
                  << " f=" << st->faces << " s=" << st->solids
                  << "  " << st->elapsedMs << "ms\n";
    }
    out.close();

    // ── 汇总：json 数组 + csv ──
    std::ifstream in(jsonlPath);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) if (!line.empty()) lines.push_back(line);
    in.close();

    {
        std::ofstream jf(jsonPath);
        jf << "[";
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i) jf << ",\n";
            jf << lines[i];
        }
        jf << "]\n";
    }

    {
        std::ofstream cf(csvPath, std::ios::binary);
        cf << "\xEF\xBB\xBF";  // UTF-8 BOM
        cf << "name,format,status,error,shape_type,size_bytes,read_ms,stat_ms,elapsed_ms,"
              "vertices,edges,faces,wires,shells,solids,compsolids,compounds,free_edges,degen_edges,"
              "face_plane,face_cylinder,face_cone,face_sphere,face_torus,face_bezier,face_bspline,"
              "face_revolution,face_extrusion,face_offset,face_other,"
              "edge_line,edge_circle,edge_ellipse,edge_hyperbola,edge_parabola,edge_bezier,edge_bspline,"
              "edge_offset,edge_other,"
              "bbox_x1,bbox_y1,bbox_z1,bbox_x2,bbox_y2,bbox_z2,"
              "bboxv_x1,bboxv_y1,bboxv_z1,bboxv_x2,bboxv_y2,bboxv_z2\n";
        for (const auto& l : lines) {
            // 复用简单解析：直接从 JSON 行提取字段太重，改为重新解析行内字段
            // 简便做法：由 jsonl 行解析出各字段（字段顺序固定）
            auto getStr = [&](const std::string& key) -> std::string {
                std::string k = "\"" + key + "\":";
                size_t p = l.find(k);
                if (p == std::string::npos) return "";
                p += k.size();
                if (l[p] == '"') {
                    size_t q = p + 1;
                    while (q < l.size()) { if (l[q] == '"' && l[q-1] != '\\') break; ++q; }
                    return l.substr(p + 1, q - p - 1);
                } else {
                    size_t q = l.find_first_of(",}", p);
                    return l.substr(p, q - p);
                }
            };
            auto getN = [&](const std::string& key) -> std::string {
                return getStr(key);
            };
            std::string row;
            row += csvField(getStr("name")) + ",";
            row += getStr("format") + "," + getStr("status") + "," + csvField(getStr("error")) + ",";
            row += getStr("shape_type") + ",";
            for (const char* k : {"size_bytes","read_ms","stat_ms","elapsed_ms",
                                  "vertices","edges","faces","wires","shells","solids","compsolids","compounds",
                                  "free_edges","degen_edges"})
                row += getN(k) + ",";
            // 面类型（固定 11 个 key）
            static const char* fk[] = {"plane","cylinder","cone","sphere","torus","bezier","bspline",
                                       "revolution","extrusion","offset","other"};
            std::string ft = getStr("face_types");
            for (const char* k : fk) {
                std::string key = "\"" + std::string(k) + "\":";
                size_t p = ft.find(key);
                std::string v = "0";
                if (p != std::string::npos) {
                    p += key.size();
                    size_t q = ft.find_first_of(",}", p);
                    v = ft.substr(p, q - p);
                }
                row += v + ",";
            }
            static const char* ek[] = {"line","circle","ellipse","hyperbola","parabola","bezier","bspline",
                                       "offset","other"};
            std::string et = getStr("edge_types");
            for (const char* k : ek) {
                std::string key = "\"" + std::string(k) + "\":";
                size_t p = et.find(key);
                std::string v = "0";
                if (p != std::string::npos) {
                    p += key.size();
                    size_t q = et.find_first_of(",}", p);
                    v = et.substr(p, q - p);
                }
                row += v + ",";
            }
            // bbox 数组
            auto arr = [&](const std::string& key) {
                std::string k = "\"" + key + "\":[";
                size_t p = l.find(k);
                if (p == std::string::npos) return std::string("0,0,0,0,0,0");
                p += k.size();
                size_t q = l.find(']', p);
                return l.substr(p, q - p);
            };
            std::string bb = arr("bbox"); std::string bv = arr("bbox_vertices");
            // bbox 数组内的逗号正好与 header 的 12 个 bbox 列一一对应，直接拼接
            row += bb + "," + bv + "\n";
            cf << row;
        }
    }

    std::cout << "[done] ok=" << okCnt << " error=" << errCnt
              << " timeout=" << toCnt << " skipped=" << skipCnt
              << " total=" << lines.size() << "\n";
    std::cout << "jsonl: " << jsonlPath.string() << "\n";
    std::cout << "csv:   " << csvPath.string() << "\n";
    std::cout << "json:  " << jsonPath.string() << "\n";
    return 0;
}
