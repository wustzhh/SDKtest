// 一次性探查：检查模型里 Geom 句柄级共享（多面同 Surface、多边同 Curve）
#include <STEPControl_Reader.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <map>
#include <set>
#include <cstdio>
#include <iostream>
#include <BRepExtrema_DistShapeShape.hxx>

int main(int argc, char** argv) {
    STEPControl_Reader r;
    if (r.ReadFile(argv[1]) != IFSelect_RetDone) { puts("read fail"); return 1; }
    r.TransferRoots();
    TopoDS_Shape shape = r.OneShape();
    TopTools_IndexedMapOfShape mF, mE;
    TopExp::MapShapes(shape, TopAbs_FACE, mF);
    TopExp::MapShapes(shape, TopAbs_EDGE, mE);
    // 面 -> Geom_Surface 句柄指针
    std::map<const void*, std::set<int>> surfFaces;
    std::map<const void*, std::set<int>> curveEdges;
    for (int i = 1; i <= mF.Extent(); ++i) {
        TopLoc_Location loc;
        Handle(Geom_Surface) s = BRep_Tool::Surface(TopoDS::Face(mF(i)), loc);
        surfFaces[s.get()].insert(i);
    }
    for (int i = 1; i <= mE.Extent(); ++i) {
        double f, l;
        Handle(Geom_Curve) c = BRep_Tool::Curve(TopoDS::Edge(mE(i)), f, l);
        curveEdges[c.get()].insert(i);
    }
    int sharedSurf = 0;
    for (const auto& kv : surfFaces) if (kv.second.size() >= 2) {
        ++sharedSurf;
        if (sharedSurf <= 10) { printf("Surface %p shared by faces:", kv.first); for (int i : kv.second) printf(" %d", i); printf("\n"); }
    }
    // 每条边所属的 solid（0-based id）
    std::map<int, int> edgeSolidId;  // edge idx -> solid idx
    {
        int sid = 0;
        TopExp_Explorer sExp(shape, TopAbs_SOLID);
        for (; sExp.More(); sExp.Next(), ++sid) {
            TopExp_Explorer eExp(sExp.Current(), TopAbs_EDGE);
            for (; eExp.More(); eExp.Next()) {
                // 找这条 edge 在 mE 里的索引
                for (int i = 1; i <= mE.Extent(); ++i)
                    if (mE(i).TShape() == eExp.Current().TShape()) { edgeSolidId[i] = sid; break; }
            }
        }
    }
    int sharedCurve = 0;
    for (const auto& kv : curveEdges) if (kv.second.size() >= 2) {
        ++sharedCurve;
        std::set<int> sids;
        for (int i : kv.second) sids.insert(edgeSolidId[i]);
        if (sharedCurve <= 15) {
            printf("Curve %p shared by %d edges covering %d solids:", kv.first, (int)kv.second.size(), (int)sids.size());
            for (int i : kv.second) printf(" e%d(s%d)", i, edgeSolidId[i]);
            printf("\n");
        }
    }
    printf("%s: faces=%d edges=%d  shared_surfaces=%d  shared_curves=%d\n",
           argv[1], mF.Extent(), mE.Extent(), sharedSurf, sharedCurve);
    // solid 两两最小距离（贴合 = 距离 0）
    {
        TopTools_IndexedMapOfShape mS;
        TopExp::MapShapes(shape, TopAbs_SOLID, mS);
        if (mS.Extent() >= 2) {
            printf("  solid pair distances:\n");
            for (int i = 1; i <= mS.Extent(); ++i)
                for (int j = i + 1; j <= mS.Extent(); ++j) {
                    BRepExtrema_DistShapeShape d(mS(i), mS(j));
                    if (d.IsDone()) printf("    s%d-s%d: %.4f\n", i - 1, j - 1, d.Value());
                }
        }
    }
    return 0;
}
