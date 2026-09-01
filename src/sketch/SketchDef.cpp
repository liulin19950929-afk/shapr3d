// SketchDef.cpp —— 草图几何 -> 3D 边 / 封闭轮廓面
#include "SketchDef.h"
#include <set>
#include <algorithm>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <BRep_Builder.hxx>

namespace cad {

static void appendSeg(std::vector<SketchDef::Edge3D>& out, const gp_Pnt& a, const gp_Pnt& b, bool cons) {
    SketchDef::Edge3D e;
    e.construction = cons;
    e.pts = {a, b};
    out.push_back(std::move(e));
}

std::vector<SketchDef::Edge3D> SketchDef::buildEdges3D() const {
    std::vector<Edge3D> out;
    for (const auto& l : lines) {
        const SPoint* a = nullptr;
        const SPoint* b = nullptr;
        for (auto& p : points) {
            if (p.id == l.p1) a = &p;
            if (p.id == l.p2) b = &p;
        }
        if (a && b)
            appendSeg(out, to3D(a->x, a->y), to3D(b->x, b->y), l.construction);
    }
    for (const auto& c : circles) {
        const SPoint* ct = nullptr;
        for (auto& p : points) if (p.id == c.center) ct = &p;
        if (!ct) continue;
        Edge3D e;
        e.construction = false;
        const int N = 64;
        for (int i = 0; i <= N; ++i) {
            double a = 2.0 * M_PI * i / N;
            e.pts.push_back(to3D(ct->x + c.r * std::cos(a), ct->y + c.r * std::sin(a)));
        }
        out.push_back(std::move(e));
    }
    for (const auto& ar : arcs) {
        const SPoint* ct = nullptr;
        for (auto& p : points) if (p.id == ar.center) ct = &p;
        if (!ct) continue;
        Edge3D e;
        e.construction = false;
        double a0 = ar.a0, a1 = ar.a1;
        while (a1 <= a0) a1 += 2 * M_PI;
        const int N = std::max(8, (int)std::ceil(24 * (a1 - a0) / M_PI));
        for (int i = 0; i <= N; ++i) {
            double a = a0 + (a1 - a0) * i / N;
            e.pts.push_back(to3D(ct->x + ar.r * std::cos(a), ct->y + ar.r * std::sin(a)));
        }
        out.push_back(std::move(e));
    }
    return out;
}

// ---------------- 轮廓 -> 面 ----------------
namespace {

struct Loop {
    std::vector<Vec2> pts;       // 闭合多段(不重复首点)
    double areaSigned() const {
        double a = 0;
        size_t n = pts.size();
        for (size_t i = 0; i < n; ++i) {
            const Vec2& p = pts[i];
            const Vec2& q = pts[(i + 1) % n];
            a += p.cross(q);
        }
        return a * 0.5;
    }
};

// 判断点是否在环上(按坐标重合)
bool samePt(const Vec2& a, const Vec2& b, double tol = 1e-7) {
    return dist(a, b) < tol;
}

} // namespace

std::string SketchDef::buildProfileFaces(TopoDS_Shape& outFaces) const {
    // ---- 分段收集: 直线 / 圆弧(精确几何), 完整圆单独成环 ----
    struct Seg {
        Vec2 a, b;               // 有向: 沿环正向 a -> b
        bool isArc = false;      // true = 圆弧, 几何为 c/r 上 a0->a1 的逆时针弧
        Vec2 c;
        double r = 0, a0 = 0, a1 = 0;
        bool flipped = false;    // 环正向与弧存储方向相反(需反转边朝向)
    };
    std::vector<Seg> segs;
    for (const auto& l : lines) {
        if (l.construction) continue;
        Vec2 p1 = pointPos(l.p1), p2 = pointPos(l.p2);
        if (dist(p1, p2) > 1e-9) {
            Seg s;
            s.a = p1; s.b = p2;
            segs.push_back(std::move(s));
        }
    }
    for (const auto& ar : arcs) {
        Vec2 ct = pointPos(ar.center);
        double a0 = ar.a0, a1 = ar.a1;
        while (a1 <= a0) a1 += 2 * M_PI;
        if (a1 - a0 >= 2 * M_PI - 1e-9) continue; // 全圆弧按圆处理(极少见, 忽略)
        Vec2 s{ct.x + ar.r * std::cos(a0), ct.y + ar.r * std::sin(a0)};
        Vec2 e{ct.x + ar.r * std::cos(a1), ct.y + ar.r * std::sin(a1)};
        if (dist(s, e) < 1e-9) continue;
        Seg sg;
        sg.a = s; sg.b = e; sg.isArc = true; sg.c = ct; sg.r = ar.r; sg.a0 = a0; sg.a1 = a1;
        segs.push_back(std::move(sg));
    }

    // 完整圆 -> 精确圆环(不再折线离散)
    struct AnyLoop {
        double area = 0;
        int kind = 0;            // 0 = 线/弧环, 1 = 完整圆
        std::vector<Seg> segs;
        Vec2 c;
        double r = 0;
    };
    std::vector<AnyLoop> loops;
    for (const auto& c : circles) {
        Vec2 ct = pointPos(c.center);
        if (c.r < 1e-9) continue;
        AnyLoop L;
        L.area = M_PI * c.r * c.r;
        L.kind = 1;
        L.c = ct;
        L.r = c.r;
        loops.push_back(std::move(L));
    }

    // ---- 线段/弧连成环(端点匹配, 允许反向) ----
    std::vector<bool> used(segs.size(), false);
    auto nearly = [](const Vec2& a, const Vec2& b) { return dist(a, b) < 1e-6; };
    for (size_t s0 = 0; s0 < segs.size(); ++s0) {
        if (used[s0]) continue;
        AnyLoop L;
        L.kind = 0;
        L.segs.push_back(segs[s0]);
        used[s0] = true;
        int guard = 0;
        while (guard++ < (int)segs.size() * 2 + 2) {
            if (nearly(L.segs.back().b, L.segs.front().a)) break; // 已闭合
            const Vec2 tail = L.segs.back().b;
            bool found = false;
            for (size_t t = 0; t < segs.size() && !found; ++t) {
                if (used[t]) continue;
                if (nearly(segs[t].a, tail)) {
                    L.segs.push_back(segs[t]);
                    used[t] = true;
                    found = true;
                } else if (nearly(segs[t].b, tail)) {
                    Seg r = segs[t];
                    std::swap(r.a, r.b);
                    r.flipped = !r.flipped;
                    L.segs.push_back(std::move(r));
                    used[t] = true;
                    found = true;
                }
            }
            if (!found) break; // 开放轮廓
        }
        if (L.segs.size() >= 3 && nearly(L.segs.back().b, L.segs.front().a)) {
            for (auto& s : L.segs) L.area += s.a.cross(s.b);
            L.area *= 0.5;
            loops.push_back(std::move(L));
        } else if (L.segs.size() >= 2) {
            // 开放轮廓: 不生成面(允许仅作显示)
        }
    }

    if (loops.empty()) return "草图中没有封闭轮廓";

    // 面积排序: 最大优先(便于判定包含关系)
    std::sort(loops.begin(), loops.end(), [](const AnyLoop& a, const AnyLoop& b) {
        return std::fabs(a.area) > std::fabs(b.area);
    });

    // ---- 内外关系: 射线法判定每个环被多少个更大环包含(嵌套深度) ----
    // 深度偶数 = 岛(外环), 深度奇数 = 孔(属于包含它的最小外环)
    auto repPoint = [](const AnyLoop& L) -> Vec2 {
        if (L.kind == 1) return L.c;
        double best = -1;
        Vec2 mid;
        for (auto& s : L.segs) {
            double d = dist(s.a, s.b);
            if (d > best) {
                best = d;
                mid = {(s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5};
            }
        }
        return mid;
    };
    auto polyOf = [](const AnyLoop& L) {
        std::vector<Vec2> poly;
        if (L.kind == 1) {
            const int N = 32;
            for (int i = 0; i < N; ++i) {
                double a = 2 * M_PI * i / N;
                poly.push_back({L.c.x + L.r * std::cos(a), L.c.y + L.r * std::sin(a)});
            }
        } else {
            for (auto& s : L.segs) poly.push_back(s.a);
        }
        return poly;
    };
    auto inPoly = [](const Vec2& p, const std::vector<Vec2>& poly) {
        bool inside = false;
        size_t n = poly.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const Vec2& a = poly[i];
            const Vec2& b = poly[j];
            if ((a.y > p.y) != (b.y > p.y)) {
                double x = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (p.x < x) inside = !inside;
            }
        }
        return inside;
    };

    struct LoopInfo {
        const AnyLoop* loop = nullptr;
        int depth = 0;
        const AnyLoop* parent = nullptr; // 包含它的最小环
    };
    std::vector<LoopInfo> infos;
    std::vector<std::vector<Vec2>> polys(loops.size());
    for (size_t i = 0; i < loops.size(); ++i) polys[i] = polyOf(loops[i]);
    for (size_t i = 0; i < loops.size(); ++i) {
        LoopInfo info;
        info.loop = &loops[i];
        Vec2 rp = repPoint(loops[i]);
        for (size_t j = 0; j < i; ++j) {
            if (inPoly(rp, polys[j])) {
                ++info.depth;
                if (!info.parent || std::fabs(loops[j].area) < std::fabs(info.parent->area))
                    info.parent = &loops[j];
            }
        }
        infos.push_back(info);
    }

    // 圆参数系与 to3D 一致: u 沿 plane X, v 沿 plane Y, 法向 = X x Y
    gp_Dir circNrm(plane.XAxis().Direction().Crossed(plane.YAxis().Direction()));
    auto makeCirc = [&](double cx, double cy, double r) {
        return gp_Circ(gp_Ax2(to3D(cx, cy), circNrm, plane.XAxis().Direction()), r);
    };
    // 由环生成 wire; hole=true 时整体反向(作为孔环)
    auto makeLoopWire = [&](const AnyLoop& L, bool hole) -> TopoDS_Wire {
        BRepBuilderAPI_MakeWire mkWire;
        if (L.kind == 1) {
            TopoDS_Edge e = BRepBuilderAPI_MakeEdge(makeCirc(L.c.x, L.c.y, L.r)).Edge();
            if (hole) e.Reverse();
            mkWire.Add(e);
            return mkWire.Wire();
        }
        const size_t n = L.segs.size();
        for (size_t i = 0; i < n; ++i) {
            const Seg& s0 = L.segs[hole ? n - 1 - i : i];
            Vec2 pa = s0.a, pb = s0.b;
            bool rev = s0.flipped;
            if (hole) {
                std::swap(pa, pb);
                rev = !rev;
            }
            if (!s0.isArc) {
                mkWire.Add(BRepBuilderAPI_MakeEdge(to3D(pa.x, pa.y), to3D(pb.x, pb.y)).Edge());
            } else {
                // 几何弧固定 a0->a1(逆时针); rev = 环走向为 b->a, 反转边朝向
                TopoDS_Edge e = BRepBuilderAPI_MakeEdge(makeCirc(s0.c.x, s0.c.y, s0.r), s0.a0, s0.a1).Edge();
                if (rev) e.Reverse();
                mkWire.Add(e);
            }
        }
        return mkWire.Wire();
    };

    // ---- 按岛组面: 每个偶深度环生成一个面, 其直接子环(奇深度)作为孔 ----
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    int nFaces = 0;
    for (size_t i = 0; i < loops.size(); ++i) {
        if (infos[i].depth % 2 != 0) continue; // 孔环: 挂在所属岛上
        BRepBuilderAPI_MakeFace mf(makeLoopWire(loops[i], false), Standard_True);
        if (!mf.IsDone()) return "面构建失败";
        TopoDS_Face face = TopoDS::Face(mf.Face());
        for (size_t j = 0; j < loops.size(); ++j) {
            if (infos[j].parent != &loops[i]) continue;
            BRepBuilderAPI_MakeFace mf2(face, makeLoopWire(loops[j], true));
            if (mf2.IsDone()) face = TopoDS::Face(mf2.Face());
        }
        builder.Add(compound, face);
        ++nFaces;
    }
    if (nFaces == 0) return "轮廓退化";
    if (nFaces == 1) {
        // 单面: 直接返回面本身
        TopoDS_Iterator it(compound);
        outFaces = it.More() ? it.Value() : TopoDS_Shape(compound);
    } else {
        outFaces = compound;
    }
    return {};
}

} // namespace cad
