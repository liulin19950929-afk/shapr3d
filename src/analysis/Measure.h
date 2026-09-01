// Measure.h —— 测量与分析(距离/角度/质量属性)
#pragma once
#include "../core/Common.h"
#include "../kernel/Occ.h"

namespace cad {

struct MeasureResult {
    bool ok = false;
    std::string kind;       // 距离/角度/体积/面积
    double value = 0;
    std::string text;
    gp_Pnt p1, p2;          // 标注点(可选)
    std::string error;
};

// 两个拓扑元素之间的最小距离
MeasureResult measureDistance(const TopoDS_Shape& a, const TopoDS_Shape& b);

// 两个平面之间的夹角(度)
MeasureResult measureAngle(const TopoDS_Face& a, const TopoDS_Face& b);

// 质量属性
struct MassProps {
    bool ok = false;
    double volumeMm3 = 0;   // mm³
    double areaMm2 = 0;     // mm²
    double massG = 0;       // g (按密度)
    gp_Pnt centroid;
    Bnd_Box bbox;
    double density = 2.7;
    std::string text() const;
};
MassProps massProperties(const TopoDS_Shape& s, double densityGcc);

// B-Rep 元素拾取辅助: 元素中心点(标注锚点)
gp_Pnt shapeAnchor(const TopoDS_Shape& s);

} // namespace cad
