// demo_headless.cpp —— 无头端到端演示: 参数化建模 -> 测量 -> 工程图 -> 全格式导出
// 在无 GPU/无显示环境下验证完整内核管线
#include "TestMain.h"
#include "../src/kernel/Document.h"
#include "../src/kernel/MeshBuilder.h"
#include "../src/drawing/Drawing.h"
#include "../src/io/Exchange.h"
#include "../src/analysis/Measure.h"
#include "../src/core/ThreadPool.h"

using namespace cad;

int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : "/tmp/shapr3d_demo";
    system(("mkdir -p " + out).c_str());

    fprintf(stderr, "==== Shapr3D 桌面版 无头演示 ====\n");
    fprintf(stderr, "线程池: %u 线程\n", ThreadPool::hardwareThreads());

    // 1) 参数化建模: 法兰盘(欢迎文档)
    Document doc;
    Stopwatch sw;
    sw.start();
    buildWelcomeDocument(doc);
    double tModel = sw.stop();
    const Body& b = doc.bodies[0];
    CHECK(!b.result.IsNull());
    fprintf(stderr, "\n[1] 参数化建模: 法兰盘 (草图x2 + 拉伸 + 拉伸切除 + 圆角)\n");
    fprintf(stderr, "    耗时 %.1f ms\n", tModel);

    // 2) 质量属性
    MassProps mp = massProperties(b.result, b.material.density);
    CHECK(mp.ok);
    fprintf(stderr, "\n[2] 测量与分析\n    体积: %s\n    表面积: %s\n    质量: %.2f g\n    质心: (%.2f, %.2f, %.2f)\n",
            fmtVolume(mp.volumeMm3).c_str(), fmtArea(mp.areaMm2).c_str(), mp.massG,
            mp.centroid.X(), mp.centroid.Y(), mp.centroid.Z());

    // 3) 网格剖分(OCCT 并行)
    sw.start();
    MeshData mesh = buildMesh(b.result, 0.05);
    double tMesh = sw.stop();
    CHECK(!mesh.verts.empty());
    fprintf(stderr, "\n[3] 并行网格剖分: %zu 三角形, %zu 棱线, %.1f ms\n",
            mesh.verts.size() / 9, mesh.edgeLines.size(), tMesh);

    // 4) 工程图 (HLR 四视图并行)
    sw.start();
    Drawing dwg;
    dwg.partName = "法兰盘";
    dwg.material = "铝合金 6061";
    std::string err = dwg.generateFromShape(b.result);
    double tDwg = sw.stop();
    CHECK(err.empty());
    fprintf(stderr, "\n[4] 工程制图: %zu 视图 (HLR 并行), %.1f ms\n", dwg.views.size(), tDwg);

    // 5) 数据交换
    fprintf(stderr, "\n[5] 工程数据交换\n");
    auto tryExport = [&](bool ok, const char* fmt, const std::string& path) {
        CHECK(ok);
        fprintf(stderr, "    %-5s -> %s %s\n", fmt, path.c_str(), ok ? "[OK]" : "[失败]");
    };
    tryExport(io::exportBRep(b.result, out + "/flange.step", io::Format::STEP, err), "STEP", out + "/flange.step");
    tryExport(io::exportBRep(b.result, out + "/flange.iges", io::Format::IGES, err), "IGES", out + "/flange.iges");
    tryExport(io::exportBRep(b.result, out + "/flange.brep", io::Format::BREP, err), "BREP", out + "/flange.brep");
    tryExport(io::exportMesh(b.result, out + "/flange.stl", io::Format::STL, 0.05, err), "STL", out + "/flange.stl");
    tryExport(io::exportMesh(b.result, out + "/flange.obj", io::Format::OBJ, 0.05, err), "OBJ", out + "/flange.obj");

    // STEP 往返校验
    TopoDS_Shape back;
    CHECK(io::importShape(out + "/flange.step", back, err));
    MassProps mp2 = massProperties(back, 1.0);
    CHECK(std::fabs(mp2.volumeMm3 - mp.volumeMm3) / mp.volumeMm3 < 1e-4);
    fprintf(stderr, "    STEP 往返: 体积偏差 %.6f%%\n",
            100.0 * std::fabs(mp2.volumeMm3 - mp.volumeMm3) / mp.volumeMm3);

    // 6) 图纸导出
    tryExport(writeFileText(out + "/flange_drawing.svg", exportDrawingSVG(dwg)), "SVG", out + "/flange_drawing.svg");
    tryExport(writeFileText(out + "/flange_drawing.dxf", exportDrawingDXF(dwg)), "DXF", out + "/flange_drawing.dxf");
    tryExport(writeFileText(out + "/flange_drawing.pdf", exportDrawingPDF(dwg)), "PDF", out + "/flange_drawing.pdf");

    // 7) 工程文件保存/加载
    CHECK(doc.saveToFile(out + "/flange.scn"));
    Document doc2;
    CHECK(doc2.loadFromFile(out + "/flange.scn"));
    CHECK(!doc2.bodies[0].result.IsNull());
    fprintf(stderr, "\n[6] 工程文件 .scn 保存/加载 [OK]\n");

    fprintf(stderr, "\n输出目录: %s\n", out.c_str());
    return testSummary();
}
