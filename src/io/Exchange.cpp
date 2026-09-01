// Exchange.cpp
#include "Exchange.h"
#include "../kernel/MeshBuilder.h"
#include <cctype>
#include <unordered_map>

namespace cad::io {

Format detectFormat(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return Format::Auto;
    std::string ext = path.substr(dot + 1);
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext == "step" || ext == "stp") return Format::STEP;
    if (ext == "iges" || ext == "igs") return Format::IGES;
    if (ext == "brep" || ext == "brp") return Format::BREP;
    if (ext == "stl") return Format::STL;
    if (ext == "obj") return Format::OBJ;
    return Format::Auto;
}

// ---------------- 导入 ----------------
bool importShape(const std::string& path, TopoDS_Shape& out, std::string& err) {
    Format f = detectFormat(path);
    try {
        if (f == Format::STEP || (f == Format::Auto && path.size() > 5)) {
            STEPControl_Reader reader;
            IFSelect_ReturnStatus st = reader.ReadFile(path.c_str());
            if (st != IFSelect_RetDone) {
                err = "STEP 读取失败";
                if (f == Format::Auto) goto try_others;
                return false;
            }
            reader.TransferRoots();
            out = reader.OneShape();
            if (out.IsNull()) { err = "STEP 无可转换实体"; return false; }
        } else if (f == Format::IGES) {
            IGESControl_Reader reader;
            IFSelect_ReturnStatus st = reader.ReadFile(path.c_str());
            if (st != IFSelect_RetDone) { err = "IGES 读取失败"; return false; }
            reader.TransferRoots();
            out = reader.OneShape();
            if (out.IsNull()) { err = "IGES 无可转换实体"; return false; }
        } else if (f == Format::BREP) {
            BRep_Builder b;
            if (!BRepTools::Read(out, path.c_str(), b)) { err = "BREP 读取失败"; return false; }
        } else if (f == Format::STL) {
            StlAPI_Reader r;
            if (!r.Read(out, path.c_str())) { err = "STL 读取失败"; return false; }
        } else if (f == Format::OBJ) {
            err = "OBJ 仅支持导出";
            return false;
        } else {
        try_others:
            err = "无法识别的文件格式";
            return false;
        }

        // 形状修复
        if (f == Format::STEP || f == Format::IGES) {
            try {
                ShapeFix_Shape fix(out);
                fix.Perform();
                TopoDS_Shape fixed = fix.Shape();
                if (!fixed.IsNull()) out = fixed;
                ShapeUpgrade_UnifySameDomain unify(out, Standard_True, Standard_True, Standard_False);
                unify.Build();
                if (!unify.Shape().IsNull()) out = unify.Shape();
            } catch (...) {
            }
        }
        return true;
    } catch (Standard_Failure& e) {
        err = e.GetMessageString() ? e.GetMessageString() : "导入异常";
        return false;
    } catch (std::exception& e) {
        err = e.what();
        return false;
    }
}

// ---------------- 网格导出 ----------------
bool exportMesh(const TopoDS_Shape& shape, const std::string& path, Format fmt,
                double deflection, std::string& err) {
    if (shape.IsNull()) { err = "空模型"; return false; }
    try {
        if (fmt == Format::STL) {
            BRepMesh_IncrementalMesh mesher(shape, deflection, Standard_False, 0.4, Standard_True);
            StlAPI_Writer w;
            w.ASCIIMode() = Standard_False;
            if (!w.Write(shape, path.c_str())) { err = "STL 写出失败"; return false; }
            return true;
        }
        if (fmt == Format::OBJ) {
            MeshData md = buildMesh(shape, deflection);
            FILE* fp = fopen(path.c_str(), "wb");
            if (!fp) { err = "无法创建文件"; return false; }
            fprintf(fp, "# Exported by Shapr3D Desktop (OCCT)\n");
            for (size_t i = 0; i < md.verts.size(); i += 3)
                fprintf(fp, "v %.6f %.6f %.6f\n", md.verts[i], md.verts[i + 1], md.verts[i + 2]);
            for (size_t i = 0; i < md.normals.size(); i += 3)
                fprintf(fp, "vn %.4f %.4f %.4f\n", md.normals[i], md.normals[i + 1], md.normals[i + 2]);
            for (uint32_t f = 0; f < md.faceCount.size(); ++f) {
                if (md.faceCount[f] == 0) continue;
                for (uint32_t t = 0; t < md.faceCount[f]; ++t) {
                    size_t base = (md.faceStart[f] + t) * 3;
                    // OBJ 索引从 1 开始; 假定面片连续
                    uint32_t i0 = (uint32_t)base + 1, i1 = i0 + 1, i2 = i0 + 2;
                    uint32_t n0 = i0, n1 = i1, n2 = i2;
                    // 顶点序号 = 全局顶点计数: faceStart 是三角基址, 顶点逐三角存放
                    uint32_t voff = (uint32_t)(base) + 1;
                    n0 = voff; n1 = voff + 1; n2 = voff + 2;
                    (void)i0; (void)i1; (void)i2;
                    fprintf(fp, "f %u//%u %u//%u %u//%u\n", n0, n0, n1, n1, n2, n2);
                }
            }
            fclose(fp);
            return true;
        }
        err = "不是网格格式";
        return false;
    } catch (Standard_Failure& e) {
        err = e.GetMessageString() ? e.GetMessageString() : "导出异常";
        return false;
    }
}

// ---------------- B-Rep 导出 ----------------
bool exportBRep(const TopoDS_Shape& shape, const std::string& path, Format fmt, std::string& err) {
    if (shape.IsNull()) { err = "空模型"; return false; }
    try {
        if (fmt == Format::STEP) {
            STEPControl_Writer w;
            IFSelect_ReturnStatus st = w.Transfer(shape, STEPControl_AsIs);
            if (st != IFSelect_RetDone) { err = "STEP 转换失败"; return false; }
            st = w.Write(path.c_str());
            if (st != IFSelect_RetDone) { err = "STEP 写出失败"; return false; }
            return true;
        }
        if (fmt == Format::IGES) {
            IGESControl_Controller::Init();
            IGESControl_Writer w;
            w.AddShape(shape);
            w.ComputeModel();
            if (!w.Write(path.c_str())) { err = "IGES 写出失败"; return false; }
            return true;
        }
        if (fmt == Format::BREP) {
            if (!BRepTools::Write(shape, path.c_str())) { err = "BREP 写出失败"; return false; }
            return true;
        }
        err = "不是 B-Rep 格式";
        return false;
    } catch (Standard_Failure& e) {
        err = e.GetMessageString() ? e.GetMessageString() : "导出异常";
        return false;
    }
}

} // namespace cad::io
