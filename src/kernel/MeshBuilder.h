// MeshBuilder.h —— OCCT 精确 B-Rep -> 三角网格 + 棱线(供 OpenGL ES 渲染与拾取)
#pragma once
#include "../core/Common.h"
#include "Occ.h"
#include "Document.h"

namespace cad {

// 网格化(使用 OCCT 内部并行 + 可选外层多实体并行)
// deflection: 弦高(mm), angular: 角度偏差(rad)
MeshData buildMesh(const TopoDS_Shape& shape, double deflection = 0.15, double angular = 0.4);

} // namespace cad
