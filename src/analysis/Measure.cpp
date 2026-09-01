// Measure.cpp
#include "Measure.h"

namespace cad {

MeasureResult measureDistance(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    MeasureResult r;
    try {
        BRepExtrema_DistShapeShape d(a, b);
        if (!d.IsDone() || d.NbSolution() == 0) {
            r.error = "无法计算距离";
            return r;
        }
        r.ok = true;
        r.kind = "距离";
        r.value = d.Value();
        r.p1 = d.PointOnShape1(1);
        r.p2 = d.PointOnShape2(1);
        r.text = fmtLength(r.value);
    } catch (Standard_Failure& e) {
        r.error = e.GetMessageString();
    }
    return r;
}

MeasureResult measureAngle(const TopoDS_Face& a, const TopoDS_Face& b) {
    MeasureResult r;
    try {
        BRepAdaptor_Surface sa(a, Standard_False);
        BRepAdaptor_Surface sb(b, Standard_False);
        if (sa.GetType() != GeomAbs_Plane || sb.GetType() != GeomAbs_Plane) {
            r.error = "角度测量需要平面";
            return r;
        }
        gp_Dir da = sa.Plane().Axis().Direction();
        gp_Dir db = sb.Plane().Axis().Direction();
        double cosv = da.Dot(db);
        cosv = std::max(-1.0, std::min(1.0, std::fabs(cosv)));
        r.ok = true;
        r.kind = "角度";
        r.value = std::acos(cosv) * 180.0 / M_PI;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f°", r.value);
        r.text = buf;
    } catch (Standard_Failure& e) {
        r.error = e.GetMessageString();
    }
    return r;
}

MassProps massProperties(const TopoDS_Shape& s, double densityGcc) {
    MassProps m;
    if (s.IsNull()) return m;
    try {
        GProp_GProps vp;
        BRepGProp::VolumeProperties(s, vp);
        GProp_GProps sp;
        BRepGProp::SurfaceProperties(s, sp);
        m.volumeMm3 = vp.Mass();
        m.areaMm2 = sp.Mass();
        m.centroid = vp.CentreOfMass();
        BRepBndLib::Add(s, m.bbox);
        m.density = densityGcc;
        m.massG = m.volumeMm3 / 1000.0 * densityGcc; // mm³ -> cm³ * g/cm³
        m.ok = m.volumeMm3 > 0;
    } catch (...) {
    }
    return m;
}

std::string MassProps::text() const {
    if (!ok) return "无法计算(可能不是实体)";
    char buf[256];
    snprintf(buf, sizeof(buf),
             "体积: %s\n表面积: %s\n质量: %.3f g (密度 %.2f g/cm³)",
             fmtVolume(volumeMm3).c_str(), fmtArea(areaMm2).c_str(), massG, density);
    return buf;
}

gp_Pnt shapeAnchor(const TopoDS_Shape& s) {
    Bnd_Box b;
    BRepBndLib::Add(s, b);
    double x0, y0, z0, x1, y1, z1;
    if (b.IsVoid()) return {};
    b.Get(x0, y0, z0, x1, y1, z1);
    return {(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2};
}

} // namespace cad
