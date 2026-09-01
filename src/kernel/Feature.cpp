// Feature.cpp —— 特征执行(OCCT B-Rep 运算)
#include "Feature.h"
#include "Document.h"
#include <unordered_map>

namespace cad {

const char* featureTypeName(FeatureType t) {
    switch (t) {
        case FeatureType::Box: return "Box";
        case FeatureType::Cylinder: return "Cylinder";
        case FeatureType::Sphere: return "Sphere";
        case FeatureType::Torus: return "Torus";
        case FeatureType::Cone: return "Cone";
        case FeatureType::Extrude: return "Extrude";
        case FeatureType::Revolve: return "Revolve";
        case FeatureType::Fillet: return "Fillet";
        case FeatureType::Chamfer: return "Chamfer";
        case FeatureType::Shell: return "Shell";
        case FeatureType::Boolean: return "Boolean";
        case FeatureType::Transform: return "Transform";
        case FeatureType::Imported: return "Imported";
    }
    return "?";
}
FeatureType featureTypeFromName(const std::string& s) {
    static const std::unordered_map<std::string, FeatureType> m = {
        {"Box", FeatureType::Box}, {"Cylinder", FeatureType::Cylinder}, {"Sphere", FeatureType::Sphere},
        {"Torus", FeatureType::Torus}, {"Cone", FeatureType::Cone}, {"Extrude", FeatureType::Extrude},
        {"Revolve", FeatureType::Revolve}, {"Fillet", FeatureType::Fillet}, {"Chamfer", FeatureType::Chamfer},
        {"Shell", FeatureType::Shell}, {"Boolean", FeatureType::Boolean}, {"Transform", FeatureType::Transform},
        {"Imported", FeatureType::Imported}};
    auto it = m.find(s);
    return it == m.end() ? FeatureType::Box : it->second;
}

std::string Feature::displayLabel() const {
    switch (type) {
        case FeatureType::Box: return "长方体";
        case FeatureType::Cylinder: return "圆柱体";
        case FeatureType::Sphere: return "球体";
        case FeatureType::Torus: return "圆环体";
        case FeatureType::Cone: return "圆锥体";
        case FeatureType::Extrude: return opMode == 1 ? "拉伸切除" : "拉伸";
        case FeatureType::Revolve: return opMode == 1 ? "旋转切除" : "旋转";
        case FeatureType::Fillet: return "圆角";
        case FeatureType::Chamfer: return "倒角";
        case FeatureType::Shell: return "抽壳";
        case FeatureType::Boolean: return opMode == 0 ? "布尔并" : (opMode == 1 ? "布尔差" : "布尔交");
        case FeatureType::Transform: return "变换";
        case FeatureType::Imported: return "导入模型";
    }
    return name;
}

// ---------- 锚点 -> 拓扑元素解析 ----------
namespace {

// 在 base 的所有棱中找到最接近 anchor 的棱
bool resolveEdge(const TopoDS_Shape& base, const gp_Pnt& anchor, TopoDS_Edge& out) {
    double best = 1e9;
    TopExp_Explorer ex(base, TopAbs_EDGE);
    for (; ex.More(); ex.Next()) {
        TopoDS_Edge e = TopoDS::Edge(ex.Current());
        BRepAdaptor_Curve c(e);
        double u0 = c.FirstParameter(), u1 = c.LastParameter();
        GCPnts_QuasiUniformDeflection d(c, 0.05, u0, u1);
        if (!d.IsDone()) continue;
        for (int i = 1; i <= d.NbPoints(); ++i) {
            double dd = d.Value(i).Distance(anchor);
            if (dd < best) { best = dd; out = e; }
        }
        // 端点也参与
        if (c.Value(u0).Distance(anchor) < best) { best = c.Value(u0).Distance(anchor); out = e; }
        if (c.Value(u1).Distance(anchor) < best) { best = c.Value(u1).Distance(anchor); out = e; }
    }
    return best < 1e6 && !out.IsNull();
}

bool resolveFace(const TopoDS_Shape& base, const gp_Pnt& anchor, TopoDS_Face& out) {
    double best = 1e9;
    TopExp_Explorer ex(base, TopAbs_FACE);
    for (; ex.More(); ex.Next()) {
        TopoDS_Face f = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface s(f);
        // 面参数域中心与角点采样
        double u0 = s.FirstUParameter(), u1 = s.LastUParameter();
        double v0 = s.FirstVParameter(), v1 = s.LastVParameter();
        for (int i = 0; i <= 2; ++i)
            for (int j = 0; j <= 2; ++j) {
                double u = u0 + (u1 - u0) * i / 2.0;
                double v = v0 + (v1 - v0) * j / 2.0;
                gp_Pnt p = s.Value(u, v);
                double dd = p.Distance(anchor);
                if (dd < best) { best = dd; out = f; }
            }
    }
    return !out.IsNull();
}

} // namespace

bool applyFillet(const TopoDS_Shape& base, double radius, const std::vector<gp_Pnt>& anchors,
                 TopoDS_Shape& out, std::string& err) {
    if (anchors.empty()) { err = "未选择边"; return false; }
    BRepFilletAPI_MakeFillet mk(base);
    int added = 0;
    for (auto& a : anchors) {
        TopoDS_Edge e;
        if (resolveEdge(base, a, e)) { mk.Add(radius, e); ++added; }
    }
    if (!added) { err = "锚点未能解析到边"; return false; }
    mk.Build();
    if (!mk.IsDone()) { err = "圆角失败(半径过大或几何冲突)"; return false; }
    out = mk.Shape();
    return true;
}

bool applyChamfer(const TopoDS_Shape& base, double d, const std::vector<gp_Pnt>& anchors,
                  TopoDS_Shape& out, std::string& err) {
    if (anchors.empty()) { err = "未选择边"; return false; }
    BRepFilletAPI_MakeChamfer mk(base);
    int added = 0;
    for (auto& a : anchors) {
        TopoDS_Edge e;
        if (resolveEdge(base, a, e)) { mk.Add(d, e); ++added; }
    }
    if (!added) { err = "锚点未能解析到边"; return false; }
    mk.Build();
    if (!mk.IsDone()) { err = "倒角失败"; return false; }
    out = mk.Shape();
    return true;
}

// ---------- 特征执行 ----------
TopoDS_Shape executeFeature(Feature& f, const TopoDS_Shape& base, const ShapeLookup& lookupBody,
                            Document& doc) {
    f.error.clear();
    try {
        switch (f.type) {
            case FeatureType::Box: {
                double L = std::max(1e-3, f.p1), W = std::max(1e-3, f.p2), H = std::max(1e-3, f.p3);
                gp_Trsf tr; tr.SetTranslation(gp_Vec(-L / 2, -W / 2, 0)); // 以原点为中心
                BRepBuilderAPI_Transform bt(BRepPrimAPI_MakeBox(L, W, H).Shape(), tr, Standard_True);
                return bt.Shape();
            }
            case FeatureType::Cylinder:
                return BRepPrimAPI_MakeCylinder(std::max(1e-3, f.p1), std::max(1e-3, f.p2)).Shape();
            case FeatureType::Sphere:
                return BRepPrimAPI_MakeSphere(std::max(1e-3, f.p1)).Shape();
            case FeatureType::Torus:
                return BRepPrimAPI_MakeTorus(std::max(1e-3, f.p1), std::max(1e-4, f.p2)).Shape();
            case FeatureType::Cone:
                return BRepPrimAPI_MakeCone(std::max(1e-3, f.p1), std::max(0.0, f.p2), std::max(1e-3, f.p3)).Shape();

            case FeatureType::Extrude: {
                SketchDef* sk = doc.sketch(f.sketchId);
                if (!sk) { f.error = "草图不存在"; return base; }
                TopoDS_Shape faces;
                std::string err = sk->buildProfileFaces(faces);
                if (!err.empty()) { f.error = err; return base; }
                gp_Dir n = sk->plane.Axis().Direction();
                if (f.flip) n.Reverse();
                gp_Vec v(n);
                v *= f.p1;
                TopoDS_Shape prism = BRepPrimAPI_MakePrism(faces, v, Standard_False, Standard_True).Shape();

                if (f.opMode == 0) { // 新增
                    if (base.IsNull()) return prism;
                    BRepAlgoAPI_Fuse fu(base, prism);
                    if (!fu.IsDone()) { f.error = "布尔并失败"; return base; }
                    return fu.Shape();
                } else {             // 切除
                    if (base.IsNull()) { f.error = "没有可切除的实体"; return base; }
                    BRepAlgoAPI_Cut cu(base, prism);
                    if (!cu.IsDone()) { f.error = "布尔差失败"; return base; }
                    return cu.Shape();
                }
            }
            case FeatureType::Revolve: {
                SketchDef* sk = doc.sketch(f.sketchId);
                if (!sk) { f.error = "草图不存在"; return base; }
                TopoDS_Shape faces;
                std::string err = sk->buildProfileFaces(faces);
                if (!err.empty()) { f.error = err; return base; }
                // 轴: 草图内直线, 或默认草图平面 X 轴
                gp_Ax1 axis;
                bool have = false;
                if (f.axisLineId) {
                    for (auto& l : sk->lines)
                        if (l.id == f.axisLineId) {
                            gp_Pnt a = sk->to3D(sk->pointPos(l.p1).x, sk->pointPos(l.p1).y);
                            gp_Pnt b = sk->to3D(sk->pointPos(l.p2).x, sk->pointPos(l.p2).y);
                            axis = gp_Ax1(a, gp_Dir(gp_Vec(a, b)));
                            have = true;
                        }
                }
                if (!have) { axis = sk->plane.XAxis(); }
                double ang = f.p1 * M_PI / 180.0;
                TopoDS_Shape rev = BRepPrimAPI_MakeRevol(faces, axis, ang).Shape();
                if (f.opMode == 0) {
                    if (base.IsNull()) return rev;
                    BRepAlgoAPI_Fuse fu(base, rev);
                    return fu.IsDone() ? fu.Shape() : rev;
                } else {
                    if (base.IsNull()) { f.error = "没有可切除的实体"; return base; }
                    BRepAlgoAPI_Cut cu(base, rev);
                    return cu.IsDone() ? cu.Shape() : base;
                }
            }
            case FeatureType::Fillet: {
                if (base.IsNull()) { f.error = "空实体"; return base; }
                TopoDS_Shape out;
                if (!applyFillet(base, f.p1, f.edgeAnchors, out, f.error)) return base;
                return out;
            }
            case FeatureType::Chamfer: {
                if (base.IsNull()) { f.error = "空实体"; return base; }
                TopoDS_Shape out;
                if (!applyChamfer(base, f.p1, f.edgeAnchors, out, f.error)) return base;
                return out;
            }
            case FeatureType::Shell: {
                if (base.IsNull()) { f.error = "空实体"; return base; }
                TopTools_ListOfShape rm;
                for (auto& a : f.faceAnchors) {
                    TopoDS_Face fc;
                    if (resolveFace(base, a, fc)) rm.Append(fc);
                }
                BRepOffsetAPI_MakeThickSolid mk;
                mk.MakeThickSolidByJoin(base, rm, -std::fabs(f.p1), 1e-6);
                if (!mk.IsDone()) { f.error = "抽壳失败"; return base; }
                return mk.Shape();
            }
            case FeatureType::Boolean: {
                if (base.IsNull()) { f.error = "空实体"; return base; }
                TopoDS_Shape other = lookupBody(f.targetBody);
                if (other.IsNull()) { f.error = "目标实体不可用"; return base; }
                BRepAlgoAPI_BooleanOperation* op = nullptr;
                if (f.opMode == 0) op = new BRepAlgoAPI_Fuse(base, other);
                else if (f.opMode == 1) op = new BRepAlgoAPI_Cut(base, other);
                else op = new BRepAlgoAPI_Common(base, other);
                if (!op->IsDone()) { delete op; f.error = "布尔运算失败"; return base; }
                TopoDS_Shape r = op->Shape();
                delete op;
                // 合并共面面, 清理碎片
                ShapeUpgrade_UnifySameDomain unify(r, Standard_True, Standard_True, Standard_False);
                unify.Build();
                return unify.Shape();
            }
            case FeatureType::Transform: {
                if (base.IsNull()) return base;
                BRepBuilderAPI_Transform bt(base, f.trsf, Standard_True);
                return bt.Shape();
            }
            case FeatureType::Imported:
                return f.result; // 导入时直接保存
        }
    } catch (Standard_Failure& e) {
        f.error = e.GetMessageString() ? e.GetMessageString() : "OCCT 异常";
    } catch (std::exception& e) {
        f.error = e.what();
    }
    return base;
}

} // namespace cad
