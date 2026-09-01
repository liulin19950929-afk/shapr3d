// test_drawing_io.cpp —— 工程图生成与数据交换测试
#include "TestMain.h"
#include "../src/kernel/Document.h"
#include "../src/drawing/Drawing.h"
#include "../src/io/Exchange.h"
#include <sys/stat.h>

using namespace cad;

static bool fileExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && st.st_size > 0;
}

int main() {
    // 测试输出目录
    const char* outDir = getenv("TEST_OUT");
    std::string out = outDir ? outDir : "/tmp/shapr3d_test";
    system((std::string("mkdir -p ") + out).c_str());

    // 参考零件
    Document doc;
    buildWelcomeDocument(doc);
    const TopoDS_Shape& part = doc.bodies[0].result;
    CHECK(!part.IsNull());

    runTest("HLR 工程图: 四视图生成", [&] {
        Drawing dwg;
        dwg.partName = "法兰盘";
        dwg.material = "铝合金 6061";
        std::string err = dwg.generateFromShape(part);
        CHECK(err.empty());
        CHECK(dwg.views.size() == 4);
        bool anyVisible = false, anyHidden = false;
        for (auto& v : dwg.views)
            for (auto& p : v.polies) {
                if (p.hidden) anyHidden = true;
                else anyVisible = true;
            }
        CHECK(anyVisible);
        CHECK(anyHidden); // 法兰盘有隐藏线(孔)
    });

    runTest("图纸导出: SVG / DXF / PDF", [&] {
        Drawing dwg;
        dwg.partName = "法兰盘";
        CHECK(dwg.generateFromShape(part).empty());

        std::string svg = exportDrawingSVG(dwg);
        CHECK(svg.size() > 1000);
        CHECK(svg.find("<path") != std::string::npos);
        CHECK(writeFileText(out + "/drawing.svg", svg));

        std::string dxf = exportDrawingDXF(dwg);
        CHECK(dxf.find("ENTITIES") != std::string::npos);
        CHECK(dxf.find("LINE") != std::string::npos);
        CHECK(writeFileText(out + "/drawing.dxf", dxf));

        std::string pdf = exportDrawingPDF(dwg);
        CHECK(pdf.size() > 500);
        CHECK(pdf.substr(0, 8) == "%PDF-1.4");
        CHECK(pdf.find("%%EOF") != std::string::npos);
        // 内嵌中文字体 (Type0/CIDFontType2/Identity-H + FontFile2 子集)
        CHECK(pdf.find("/FontFile2") != std::string::npos);
        CHECK(pdf.find("/Identity-H") != std::string::npos);
        CHECK(pdf.find("/F2 ") != std::string::npos); // 文本走内嵌 CID 字体
        CHECK(writeFileText(out + "/drawing.pdf", pdf));
    });

    runTest("STEP 导出/导入 往返", [&] {
        std::string err;
        double v0 = 0;
        {
            GProp_GProps p;
            BRepGProp::VolumeProperties(part, p);
            v0 = p.Mass();
        }
        CHECK(io::exportBRep(part, out + "/part.step", io::Format::STEP, err));
        TopoDS_Shape back;
        CHECK(io::importShape(out + "/part.step", back, err));
        GProp_GProps p;
        BRepGProp::VolumeProperties(back, p);
        CHECK(std::fabs(p.Mass() - v0) / v0 < 1e-4);
    });

    runTest("IGES 导入读回", [&] {
        std::string err;
        double v0 = 0;
        {
            GProp_GProps p;
            BRepGProp::VolumeProperties(part, p);
            v0 = p.Mass();
        }
        CHECK(io::exportBRep(part, out + "/roundtrip.iges", io::Format::IGES, err));
        TopoDS_Shape back;
        CHECK(io::importShape(out + "/roundtrip.iges", back, err));
        CHECK(!back.IsNull());
        GProp_GProps p;
        BRepGProp::VolumeProperties(back, p);
        CHECK(std::fabs(p.Mass() - v0) / v0 < 2e-3); // IGES 修复/缝合后容差放宽
    });

    runTest("IGES / BREP / STL / OBJ 导出", [&] {
        std::string err;
        CHECK(io::exportBRep(part, out + "/part.iges", io::Format::IGES, err));
        CHECK(io::exportBRep(part, out + "/part.brep", io::Format::BREP, err));
        CHECK(io::exportMesh(part, out + "/part.stl", io::Format::STL, 0.05, err));
        CHECK(io::exportMesh(part, out + "/part.obj", io::Format::OBJ, 0.05, err));
        CHECK(fileExists(out + "/part.iges"));
        CHECK(fileExists(out + "/part.stl"));
        // BREP 读回
        TopoDS_Shape back;
        CHECK(io::importShape(out + "/part.brep", back, err));
        CHECK(!back.IsNull());
    });

    runTest("格式检测", [&] {
        CHECK(io::detectFormat("a.step") == io::Format::STEP);
        CHECK(io::detectFormat("a.STP") == io::Format::STEP);
        CHECK(io::detectFormat("b.igs") == io::Format::IGES);
        CHECK(io::detectFormat("c.stl") == io::Format::STL);
        CHECK(io::detectFormat("d.obj") == io::Format::OBJ);
    });

    fprintf(stderr, "\n测试输出文件在: %s\n", out.c_str());
    return testSummary();
}
