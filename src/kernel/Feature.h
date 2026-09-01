// Feature.h —— 参数化特征(基于 OCCT 精确 B-Rep)
#pragma once
#include "../core/Common.h"
#include "Occ.h"

namespace cad {

enum class FeatureType {
    Box,        // 长方体   p1=长 p2=宽 p3=高
    Cylinder,   // 圆柱体   p1=半径 p2=高
    Sphere,     // 球体     p1=半径
    Torus,      // 圆环体   p1=主半径 p2=管半径
    Cone,       // 圆锥台   p1=下半径 p2=上半径 p3=高
    Extrude,    // 拉伸     sketchId, p1=距离, opMode: 0新增/1切除
    Revolve,    // 旋转     sketchId, axisLineId, p1=角度, opMode
    Fillet,     // 圆角     p1=半径, edgeAnchors
    Chamfer,    // 倒角     p1=距离, edgeAnchors
    Shell,      // 抽壳     p1=厚度, faceAnchors(开放面)
    Boolean,    // 布尔     targetBody, opMode: 0并/1差/2交
    Transform,  // 变换     trsf
    Imported    // 导入基体
};

// 特征参数( POD 便于序列化/快照 )
struct Feature {
    Id id = kInvalidId;
    FeatureType type = FeatureType::Box;
    std::string name;
    bool active = true;          // 抑制开关
    std::string error;           // 最近一次执行错误

    double p1 = 0, p2 = 0, p3 = 0;
    Id sketchId = kInvalidId;    // Extrude/Revolve
    Id axisLineId = kInvalidId;  // Revolve 轴线(草图内直线)
    int opMode = 0;              // 布尔/拉伸模式
    Id targetBody = kInvalidId;  // Boolean 另一实体
    bool visible = true;

    // 圆角/倒角: 边锚点(边上采样点), 重算时按最近边重新解析, 保证参数化稳定
    std::vector<gp_Pnt> edgeAnchors;
    // 抽壳: 移除面锚点(面中心点)
    std::vector<gp_Pnt> faceAnchors;

    gp_Trsf trsf;                // Transform
    bool flip = false;           // 拉伸方向反转

    // ---- 执行缓存 ----
    TopoDS_Shape result;
    bool dirty = true;

    std::string displayLabel() const;
};

const char* featureTypeName(FeatureType t);
FeatureType featureTypeFromName(const std::string& s);

// 执行特征: base 为链上前一结果(可为空=首个特征)
// docBodies: 供 Boolean 取其他实体结果 (bodyId -> shape)
using ShapeLookup = std::function<TopoDS_Shape(Id)>;
TopoDS_Shape executeFeature(Feature& f, const TopoDS_Shape& base, const ShapeLookup& lookupBody,
                            class Document& doc);

// 单独: 对任意 shape 做圆角/倒角(工具函数)
bool applyFillet(const TopoDS_Shape& base, double radius, const std::vector<gp_Pnt>& anchors, TopoDS_Shape& out, std::string& err);
bool applyChamfer(const TopoDS_Shape& base, double dist, const std::vector<gp_Pnt>& anchors, TopoDS_Shape& out, std::string& err);

// 材质(渲染用, 也参与质量计算)
struct Material {
    std::string name = "铝";
    float color[3] = {0.72f, 0.74f, 0.78f};
    float metallic = 1.0f;
    float roughness = 0.35f;
    double density = 2.70; // g/cm³
};

} // namespace cad
