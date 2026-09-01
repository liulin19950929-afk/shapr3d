// Exchange.h —— 工程数据交换 (STEP / IGES / BREP / STL / OBJ 导入导出)
#pragma once
#include "../core/Common.h"
#include "../kernel/Occ.h"

namespace cad::io {

enum class Format { Auto, STEP, IGES, BREP, STL, OBJ };

Format detectFormat(const std::string& path);

// ---------- 导入 ----------
bool importShape(const std::string& path, TopoDS_Shape& out, std::string& err);
// 模型 -> 网格文件
bool exportMesh(const TopoDS_Shape& shape, const std::string& path, Format fmt,
                double deflection, std::string& err);
// 精确 B-Rep 导出
bool exportBRep(const TopoDS_Shape& shape, const std::string& path, Format fmt, std::string& err);

} // namespace cad::io
