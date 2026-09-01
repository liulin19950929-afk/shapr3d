// SketchDef.h —— 草图数据模型(纯数据, 求解器/序列化/3D 重建共用)
#pragma once
#include "../core/Common.h"
#include "../kernel/Occ.h"

namespace cad {

// ---------------- 草图几何实体 ----------------
struct SPoint {
    Id id = kInvalidId;
    double x = 0, y = 0;      // 平面坐标 (mm)
    bool fixed = false;        // 用户固定
};

struct SLine {
    Id id = kInvalidId;
    Id p1 = kInvalidId, p2 = kInvalidId;
    bool construction = false; // 构造线
};

struct SCircle {
    Id id = kInvalidId;
    Id center = kInvalidId;
    double r = 1.0;
};

// 三点信息以 圆心+半径+起止角 存储
struct SArc {
    Id id = kInvalidId;
    Id center = kInvalidId;
    double r = 1.0;
    double a0 = 0, a1 = M_PI / 2; // 弧度, 逆时针 a0 -> a1
};

enum class CstType {
    Coincident,     // 重合        refs: p1, p2
    Horizontal,     // 水平        refs: line | p1,p2
    Vertical,       // 垂直        refs: line | p1,p2
    Parallel,       // 平行        refs: l1, l2
    Perpendicular,  // 垂直        refs: l1, l2
    Distance,       // 距离        refs: p1,p2          value
    DistPtLine,     // 点线距离    refs: p, line        value
    Length,         // 长度        refs: line           value
    Radius,         // 半径        refs: circle/arc     value
    Diameter,       // 直径        refs: circle         value
    Angle,          // 角度        refs: l1, l2         value(弧度)
    Equal,          // 相等        refs: g1, g2
    Midpoint,       // 中点        refs: p, line
    PointOnLine,    // 点在线上    refs: p, line
    PointOnCircle,  // 点在圆/弧上 refs: p, circle/arc
    Concentric,     // 同心        refs: c1, c2
    Fix             // 固定        refs: p
};

struct Constraint {
    Id id = kInvalidId;
    CstType type = CstType::Coincident;
    std::vector<Id> refs;
    double value = 0;
    double value2 = 0;   // Fix 约束的纵坐标快照
};

// ---------------- 草图定义 ----------------
struct SketchDef {
    Id id = kInvalidId;
    std::string name = "草图";
    gp_Pln plane;              // 3D 所在平面

    std::vector<SPoint> points;
    std::vector<SLine> lines;
    std::vector<SCircle> circles;
    std::vector<SArc> arcs;
    std::vector<Constraint> constraints;
    bool visible = true;

    // ---- 实体查找 ----
    SPoint* point(Id id) {
        for (auto& p : points) if (p.id == id) return &p;
        return nullptr;
    }
    const SPoint* point(Id id) const {
        for (auto& p : points) if (p.id == id) return &p;
        return nullptr;
    }
    SLine* line(Id id) {
        for (auto& l : lines) if (l.id == id) return &l;
        return nullptr;
    }
    SCircle* circle(Id id) {
        for (auto& c : circles) if (c.id == id) return &c;
        return nullptr;
    }
    const SCircle* circle(Id id) const {
        for (auto& c : circles) if (c.id == id) return &c;
        return nullptr;
    }
    SArc* arc(Id id) {
        for (auto& a : arcs) if (a.id == id) return &a;
        return nullptr;
    }
    const SArc* arc(Id id) const {
        for (auto& a : arcs) if (a.id == id) return &a;
        return nullptr;
    }

    // 便捷创建
    // 注意: 返回引用在后续 push 时可能因扩容失效 —— 需要"固定"请用 setPointFixed()
    SPoint& addPoint(double x, double y, Id id) {
        points.push_back({id, x, y, false});
        return points.back();
    }
    SLine& addLine(Id p1, Id p2, Id id, bool construction = false) {
        lines.push_back({id, p1, p2, construction});
        return lines.back();
    }
    SCircle& addCircle(Id center, double r, Id id) {
        circles.push_back({id, center, r});
        return circles.back();
    }
    SArc& addArc(Id center, double r, double a0, double a1, Id id) {
        arcs.push_back({id, center, r, a0, a1});
        return arcs.back();
    }

    Vec2 pointPos(Id id) const {
        for (auto& p : points) if (p.id == id) return {p.x, p.y};
        return {};
    }
    Vec2 pointPos(Id id) {
        for (auto& p : points) if (p.id == id) return {p.x, p.y};
        return {};
    }

    // 稳定的固定点设置(内部重新查找, 避免引用失效)
    void setPointFixed(Id id, double fx, double fy) {
        if (SPoint* p = point(id)) {
            p->x = fx;
            p->y = fy;
            p->fixed = true;
        }
    }
    void setPointFixed(Id id) {
        if (SPoint* p = point(id)) p->fixed = true;
    }

    // 草图是否为空
    bool isEmpty() const {
        return lines.empty() && circles.empty() && arcs.empty();
    }

    // 2D -> 3D
    gp_Pnt to3D(double u, double v) const {
        const gp_Pnt& o = plane.Location();
        gp_Dir dx = plane.XAxis().Direction();
        gp_Dir dy = plane.YAxis().Direction();
        return {o.X() + dx.X() * u + dy.X() * v,
                o.Y() + dx.Y() * u + dy.Y() * v,
                o.Z() + dx.Z() * u + dy.Z() * v};
    }
    // 3D -> 2D (投影到草图平面)
    Vec2 to2D(const gp_Pnt& p) const {
        const gp_Pnt& o = plane.Location();
        gp_Dir dx = plane.XAxis().Direction();
        gp_Dir dy = plane.YAxis().Direction();
        gp_Vec d(p.X() - o.X(), p.Y() - o.Y(), p.Z() - o.Z());
        return {d.Dot(gp_Vec(dx)), d.Dot(gp_Vec(dy))};
    }

    // 求解后的几何 -> 一组 3D 边(用于显示与轮廓提取)
    // 返回 3D 折线(每条边离散成点串)
    struct Edge3D {
        std::vector<gp_Pnt> pts;
        bool construction = false;
    };
    std::vector<Edge3D> buildEdges3D() const;

    // 将封闭轮廓组合成面(用于拉伸/旋转), 返回失败原因(空=成功)
    std::string buildProfileFaces(TopoDS_Shape& outFaces) const;
};

// 默认平面快捷构造
inline gp_Pln planeXY() { return {gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0))}; }
inline gp_Pln planeXZ() { return {gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0), gp_Dir(1, 0, 0))}; }
inline gp_Pln planeYZ() { return {gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), gp_Dir(0, 1, 0))}; }

} // namespace cad
