// Drawing.h —— 工程图: HLR 投影生成标准三视图 + 轴测图, 2D 制图实体
#pragma once
#include "../core/Common.h"
#include "../kernel/Occ.h"

namespace cad {

// 2D 图元(图纸坐标: mm, y 向上, 原点左下)
struct DrawPoly {
    std::vector<Vec2> pts;
    bool hidden = false;     // 虚线(不可见轮廓)
    bool center_line = false;// 中心线(点划线)
};

struct DrawText {
    Vec2 pos;
    double height = 3.5;
    std::string text;
    int align = 0;           // 0左 1中 2右
};

struct DrawCircle {
    Vec2 center;
    double r = 1;
    bool hidden = false;
};

struct DrawDim {
    Vec2 a, b;               // 两端
    double offset = 8;       // 尺寸线偏移
    std::string text;        // 覆盖文本(空=自动)
};

struct DrawView {
    std::string name;
    double ox = 0, oy = 0;   // 图纸上位置
    double scale = 1.0;      // 视图比例
    std::vector<DrawPoly> polies;
    // 视图局部包围盒(未定位前)
    double bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
};

class Drawing {
public:
    double sheetW = 420, sheetH = 297; // A3
    std::string partName = "零件1";
    std::string material = "";
    std::string scaleText = "1:1";
    std::vector<DrawView> views;
    std::vector<DrawText> texts;
    std::vector<DrawCircle> circles;   // 2D 制图
    std::vector<DrawPoly> drafts;      // 2D 制图线段
    std::vector<DrawDim> dims;         // 2D 制图尺寸

    // 由实体生成四视图(HLR 隐藏线消除, 内部多线程并行)
    // 返回错误信息, 空=成功
    std::string generateFromShape(const TopoDS_Shape& shape);

    // 视图局部坐标 -> 图纸坐标
    Vec2 toSheet(const DrawView& v, const Vec2& p) const {
        return {v.ox + (p.x - v.bx0) * v.scale, v.oy + (p.y - v.by0) * v.scale};
    }
};

// ---------- 导出 ----------
std::string exportDrawingSVG(const Drawing& dwg);
std::string exportDrawingDXF(const Drawing& dwg);   // R12 ASCII
std::string exportDrawingPDF(const Drawing& dwg);   // 矢量 PDF
bool writeFileText(const std::string& path, const std::string& content);

} // namespace cad
