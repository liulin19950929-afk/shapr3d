// Drawing.cpp —— HLR 三视图生成
#include "Drawing.h"
#include "../core/ThreadPool.h"

namespace cad {

namespace {

// 单个方向的 HLR 投影, 输出平面折线(投影面坐标)
struct HlrOut {
    std::vector<DrawPoly> visible, hidden;
    double bx0 = 1e9, by0 = 1e9, bx1 = -1e9, by1 = -1e9;
    bool empty() const { return visible.empty() && hidden.empty(); }
};

void collectEdges(const TopoDS_Shape& comp, std::vector<DrawPoly>& out) {
    if (comp.IsNull()) return;
    TopExp_Explorer ex(comp, TopAbs_EDGE);
    for (; ex.More(); ex.Next()) {
        TopoDS_Edge e = TopoDS::Edge(ex.Current());
        try {
            BRepAdaptor_Curve c(e);
            double u0 = c.FirstParameter(), u1 = c.LastParameter();
            GCPnts_QuasiUniformDeflection d(c, 0.02, u0, u1);
            if (!d.IsDone() || d.NbPoints() < 2) continue;
            DrawPoly p;
            for (int i = 1; i <= d.NbPoints(); ++i) {
                gp_Pnt q = d.Value(i);
                p.pts.push_back({q.X(), q.Y()});
            }
            if (p.pts.size() >= 2) out.push_back(std::move(p));
        } catch (...) {
        }
    }
}

HlrOut projectView(const TopoDS_Shape& shape, const gp_Ax2& frame) {
    HlrOut out;
    Handle(HLRBRep_Algo) algo = new HLRBRep_Algo();
    algo->Add(shape);
    HLRAlgo_Projector proj(frame);
    algo->Projector(proj);
    algo->Update();
    algo->Hide();

    HLRBRep_HLRToShape hlr(algo);
    collectEdges(hlr.VCompound(), out.visible);
    collectEdges(hlr.OutLineVCompound(), out.visible);      // 轮廓线
    collectEdges(hlr.Rg1LineVCompound(), out.visible);      // 光顺边
    collectEdges(hlr.HCompound(), out.hidden);
    collectEdges(hlr.OutLineHCompound(), out.hidden);

    for (auto* list : {&out.visible, &out.hidden})
        for (auto& p : *list)
            for (auto& q : p.pts) {
                out.bx0 = std::min(out.bx0, q.x);
                out.by0 = std::min(out.by0, q.y);
                out.bx1 = std::max(out.bx1, q.x);
                out.by1 = std::max(out.by1, q.y);
            }
    if (out.bx0 > out.bx1) { out.bx0 = out.by0 = out.bx1 = out.by1 = 0; }
    return out;
}

gp_Ax2 viewFrame(int which) {
    gp_Pnt o(0, 0, 0);
    switch (which) {
        case 0: return {o, gp_Dir(0, 1, 0), gp_Dir(1, 0, 0)};  // 主视(前视): X 右 Z 上
        case 1: return {o, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)};  // 俯视
        case 2: return {o, gp_Dir(-1, 0, 0), gp_Dir(0, -1, 0)};// 左视
        default:
            gp_Dir n(1, -1, 1);                                 // 轴测(构造时自动归一化)
            gp_Dir x = gp_Dir(0, 0, 1).Crossed(n);
            return {o, n, x};
    }
}

} // namespace

std::string Drawing::generateFromShape(const TopoDS_Shape& shape) {
    views.clear();
    texts.clear();
    if (shape.IsNull()) return "空模型";

    // 4 视图并行投影(多线程 CPU)
    std::vector<HlrOut> outs(4);
    std::vector<std::future<void>> futs;
    for (int i = 0; i < 4; ++i)
        futs.push_back(globalPool().submit([&, i] { outs[i] = projectView(shape, viewFrame(i)); }));
    for (auto& f : futs) f.get();

    static const char* names[4] = {"主视图", "俯视图", "左视图", "轴测图"};
    // 布局区域 (图纸 420x297, 内框 10..410/10..287)
    struct Cell { double x0, y0, x1, y1; };
    static const Cell cells[4] = {
        {20, 158, 195, 282},   // 主视: 左上
        {20, 20, 195, 144},    // 俯视: 左下
        {210, 158, 330, 282},  // 左视: 右上
        {215, 20, 400, 144},   // 轴测: 右下
    };

    for (int i = 0; i < 4; ++i) {
        if (outs[i].empty() && i < 3) continue; // 轴测可能无轮廓也继续
        DrawView v;
        v.name = names[i];
        v.bx0 = outs[i].bx0;
        v.by0 = outs[i].by0;
        v.bx1 = outs[i].bx1;
        v.by1 = outs[i].by1;
        double w = std::max(1e-6, v.bx1 - v.bx0);
        double h = std::max(1e-6, v.by1 - v.by0);
        double cw = cells[i].x1 - cells[i].x0 - 14;
        double ch = cells[i].y1 - cells[i].y0 - 20;
        v.scale = std::min(cw / w, ch / h);
        if (v.scale > 10) v.scale = 10; // 别过分放大
        // 居中放置
        v.ox = cells[i].x0 + 7 + (cw - w * v.scale) / 2;
        v.oy = cells[i].y0 + 12 + (ch - h * v.scale) / 2;
        for (auto& p : outs[i].visible) {
            DrawPoly q = p;
            for (auto& pt : q.pts) pt = toSheet(v, pt);
            v.polies.push_back(std::move(q));
        }
        for (auto& p : outs[i].hidden) {
            DrawPoly q = p;
            q.hidden = true;
            for (auto& pt : q.pts) pt = toSheet(v, pt);
            v.polies.push_back(std::move(q));
        }
        views.push_back(std::move(v));

        // 视图名称
        DrawText t;
        t.pos = {(cells[i].x0 + cells[i].x1) / 2, cells[i].y0 + 4};
        t.height = 4;
        t.text = names[i];
        t.align = 1;
        texts.push_back(t);
    }

    // 标题栏文本
    DrawText title;
    title.pos = {sheetW - 125, sheetH - 22};
    title.height = 5;
    title.text = partName;
    texts.push_back(title);
    DrawText mat;
    mat.pos = {sheetW - 125, sheetH - 30};
    mat.height = 3.5;
    mat.text = "材料: " + material + "   比例: " + scaleText;
    texts.push_back(mat);

    return {};
}

} // namespace cad
