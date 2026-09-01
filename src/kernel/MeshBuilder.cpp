// MeshBuilder.cpp
#include "MeshBuilder.h"

namespace cad {

MeshData buildMesh(const TopoDS_Shape& shape, double deflection, double angular) {
    MeshData md;
    if (shape.IsNull()) return md;

    // OCCT 并行网格剖分(IMeshTools 参数 InParallel -> OSD 多线程)
    IMeshTools_Parameters mp;
    mp.Deflection = deflection;
    mp.Angle = angular;
    mp.Relative = Standard_False;
    mp.InParallel = Standard_True;
    mp.AllowQualityDecrease = Standard_True;
    BRepMesh_IncrementalMesh(shape, mp);

    TopExp_Explorer fx(shape, TopAbs_FACE);
    uint32_t triBase = 0;
    for (; fx.More(); fx.Next()) {
        TopoDS_Face face = TopoDS::Face(fx.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;

        int faceIdx = md.faceMap.Add(face); // 1-based
        md.faceStart.push_back(triBase);
        uint32_t count = 0;

        const gp_Trsf& tr = loc.Transformation();
        bool reversed = (face.Orientation() == TopAbs_REVERSED);
        bool hasNormals = tri->HasNormals();

        // 收集顶点+法线
        std::vector<float> nv;
        nv.reserve((size_t)tri->NbNodes() * 3);
        std::vector<float> nn;
        nn.reserve((size_t)tri->NbNodes() * 3);
        for (int i = 1; i <= tri->NbNodes(); ++i) {
            gp_Pnt p = tri->Node(i).Transformed(tr);
            nv.push_back((float)p.X());
            nv.push_back((float)p.Y());
            nv.push_back((float)p.Z());
            if (hasNormals) {
                gp_Dir d = tri->Normal(i).Transformed(tr);
                if (reversed) d.Reverse();
                nn.push_back((float)d.X());
                nn.push_back((float)d.Y());
                nn.push_back((float)d.Z());
            }
        }
        // 三角形
        for (int i = 1; i <= tri->NbTriangles(); ++i) {
            int a, b, c;
            tri->Triangle(i).Get(a, b, c);
            if (reversed) std::swap(b, c);
            const float* P[3] = {&nv[(a - 1) * 3], &nv[(b - 1) * 3], &nv[(c - 1) * 3]};
            // 平面三角形退化检测
            float ux = P[1][0] - P[0][0], uy = P[1][1] - P[0][1], uz = P[1][2] - P[0][2];
            float wx = P[2][0] - P[0][0], wy = P[2][1] - P[0][1], wz = P[2][2] - P[0][2];
            if (std::fabs(uy * wz - uz * wy) + std::fabs(uz * wx - ux * wz) + std::fabs(ux * wy - uy * wx) < 1e-14f)
                continue;
            for (int k = 0; k < 3; ++k) {
                md.verts.insert(md.verts.end(), P[k], P[k] + 3);
                if (hasNormals) {
                    int ni = (k == 0 ? a : k == 1 ? b : c) - 1;
                    md.normals.insert(md.normals.end(), nn.begin() + ni * 3, nn.begin() + ni * 3 + 3);
                }
            }
            ++count;
        }
        // 平面三角形且无法线: 用面法线填充
        if (!hasNormals && count > 0) {
            BRepAdaptor_Surface surf(face, Standard_False);
            bool planar = (surf.GetType() == GeomAbs_Plane);
            for (uint32_t t = 0; t < count; ++t) {
                const float* v = &md.verts[(triBase + t) * 9];
                float nx, ny, nz;
                if (planar) {
                    gp_Dir d = surf.Plane().Axis().Direction();
                    if (reversed) d.Reverse();
                    nx = (float)d.X(); ny = (float)d.Y(); nz = (float)d.Z();
                } else {
                    // 三角形几何法线
                    float ux = v[3] - v[0], uy = v[4] - v[1], uz = v[5] - v[2];
                    float wx = v[6] - v[0], wy = v[7] - v[1], wz = v[8] - v[2];
                    nx = uy * wz - uz * wy; ny = uz * wx - ux * wz; nz = ux * wy - uy * wx;
                    float l = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (l < 1e-12f) { nx = 0; ny = 0; nz = 1; } else { nx /= l; ny /= l; nz /= l; }
                }
                for (int k = 0; k < 3; ++k) {
                    md.normals.push_back(nx);
                    md.normals.push_back(ny);
                    md.normals.push_back(nz);
                }
            }
        }
        triBase += count;
        md.faceCount.push_back(count);
    }

    // ---- 棱线(全局 3D 折线) ----
    TopExp_Explorer ex(shape, TopAbs_EDGE);
    for (; ex.More(); ex.Next()) {
        TopoDS_Edge edge = TopoDS::Edge(ex.Current());
        TopLoc_Location loc;
        Handle(Poly_Polygon3D) poly = BRep_Tool::Polygon3D(edge, loc);
        std::vector<float> pts;
        if (!poly.IsNull()) {
            const gp_Trsf& tr = loc.Transformation();
            const TColgp_Array1OfPnt& nodes = poly->Nodes();
            for (int i = nodes.Lower(); i <= nodes.Upper(); ++i) {
                gp_Pnt p = nodes(i).Transformed(tr);
                pts.push_back((float)p.X());
                pts.push_back((float)p.Y());
                pts.push_back((float)p.Z());
            }
        } else {
            try {
                BRepAdaptor_Curve c(edge);
                GCPnts_QuasiUniformDeflection d(c, deflection * 0.5);
                if (d.IsDone()) {
                    for (int i = 1; i <= d.NbPoints(); ++i) {
                        gp_Pnt p = d.Value(i);
                        pts.push_back((float)p.X());
                        pts.push_back((float)p.Y());
                        pts.push_back((float)p.Z());
                    }
                }
            } catch (...) {
            }
        }
        if (pts.size() >= 6) md.edgeLines.push_back(std::move(pts));
    }
    return md;
}

} // namespace cad
