// test_modeling.cpp —— B-Rep 建模内核测试
#include "TestMain.h"
#include "../src/kernel/Document.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include "../src/kernel/MeshBuilder.h"
#include "../src/analysis/Measure.h"

using namespace cad;

static double volumeOf(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}

int main() {
    runTest("基本体: 长方体/圆柱/球", [&] {
        Feature f;
        f.type = FeatureType::Box;
        f.p1 = 10; f.p2 = 10; f.p3 = 10;
        Document doc;
        auto box = executeFeature(f, {}, {}, doc);
        CHECK_NEAR(volumeOf(box), 1000.0, 1e-6);

        f.type = FeatureType::Cylinder;
        f.p1 = 5; f.p2 = 20;
        auto cyl = executeFeature(f, {}, {}, doc);
        CHECK_NEAR(volumeOf(cyl), M_PI * 25 * 20, 1e-3);

        f.type = FeatureType::Sphere;
        f.p1 = 8; f.p2 = 0;
        auto sph = executeFeature(f, {}, {}, doc);
        CHECK_NEAR(volumeOf(sph), 4.0 / 3.0 * M_PI * 512, 1e-2);
    });

    runTest("草图拉伸 + 特征链", [&] {
        Document doc;
        Body& b = doc.addBody();
        SketchDef& sk = doc.addSketch(planeXY());
        auto& p1 = sk.addPoint(0, 0, doc.newId());
        auto& p2 = sk.addPoint(40, 0, doc.newId());
        auto& p3 = sk.addPoint(40, 30, doc.newId());
        auto& p4 = sk.addPoint(0, 30, doc.newId());
        sk.addLine(p1.id, p2.id, doc.newId());
        sk.addLine(p2.id, p3.id, doc.newId());
        sk.addLine(p3.id, p4.id, doc.newId());
        sk.addLine(p4.id, p1.id, doc.newId());

        Feature& ext = b.features.emplace_back();
        ext.id = doc.newId();
        ext.type = FeatureType::Extrude;
        ext.sketchId = sk.id;
        ext.p1 = 12;
        doc.recomputeAll();
        CHECK(b.result.IsNull() == false);
        CHECK_NEAR(volumeOf(b.result), 40 * 30 * 12, 1e-4);
    });

    runTest("草图弧: 线+半圆弧混合轮廓拉伸", [&] {
        // 20x10 矩形 + 右端半圆(R5): 面积 = 200 + 12.5π, 拉伸 5
        Document doc;
        Body& b = doc.addBody();
        SketchDef& sk = doc.addSketch(planeXY());
        auto& p1 = sk.addPoint(0, 0, doc.newId());
        auto& p2 = sk.addPoint(20, 0, doc.newId());
        auto& p3 = sk.addPoint(20, 10, doc.newId());
        auto& p4 = sk.addPoint(0, 10, doc.newId());
        auto& c = sk.addPoint(20, 5, doc.newId());
        sk.addLine(p1.id, p2.id, doc.newId());
        sk.addArc(c.id, 5.0, -M_PI / 2, M_PI / 2, doc.newId()); // (20,0) -> (20,10) 经 (25,5)
        sk.addLine(p3.id, p4.id, doc.newId());
        sk.addLine(p4.id, p1.id, doc.newId());

        Feature& ext = b.features.emplace_back();
        ext.id = doc.newId();
        ext.type = FeatureType::Extrude;
        ext.sketchId = sk.id;
        ext.p1 = 5;
        doc.recomputeAll();
        CHECK(b.result.IsNull() == false);
        double expect = (200.0 + M_PI * 25.0 / 2.0) * 5.0;
        CHECK_NEAR(volumeOf(b.result), expect, 1e-3);
    });

    runTest("布尔运算: 差/并/交", [&] {
        Document doc;
        Feature f;
        f.type = FeatureType::Box; f.p1 = 20; f.p2 = 20; f.p3 = 20;
        auto box = executeFeature(f, {}, {}, doc);
        Feature c;
        c.type = FeatureType::Cylinder; c.p1 = 4; c.p2 = 40;
        auto cyl = executeFeature(c, {}, {}, doc);

        Feature cut;
        cut.type = FeatureType::Boolean; cut.opMode = 1; cut.targetBody = 77;
        auto cutRes = executeFeature(cut, box, [&](Id) { return cyl; }, doc);
        CHECK_NEAR(volumeOf(cutRes), 8000 - M_PI * 16 * 20, 1e-2); // 贯穿 20 高

        Feature fuse;
        fuse.type = FeatureType::Boolean; fuse.opMode = 0; fuse.targetBody = 77;
        auto fuseRes = executeFeature(fuse, box, [&](Id) { return cyl; }, doc);
        CHECK(volumeOf(fuseRes) > volumeOf(box) + 1e-9);

        Feature common;
        common.type = FeatureType::Boolean; common.opMode = 2; common.targetBody = 77;
        auto comRes = executeFeature(common, box, [&](Id) { return cyl; }, doc);
        CHECK_NEAR(volumeOf(comRes), M_PI * 16 * 20, 1e-2);
    });

    runTest("圆角/倒角", [&] {
        Document doc;
        Feature f;
        f.type = FeatureType::Box; f.p1 = 10; f.p2 = 10; f.p3 = 10;
        auto box = executeFeature(f, {}, {}, doc);

        TopoDS_Shape fil;
        std::string err;
        // 竖直棱 (10, 10) 边上的锚点
        CHECK(applyFillet(box, 2, {{10, 10, 5}}, fil, err));
        CHECK(!fil.IsNull());
        CHECK(volumeOf(fil) < 1000);

        TopoDS_Shape ch;
        CHECK(applyChamfer(box, 1, {{10, 10, 5}}, ch, err));
        CHECK(!ch.IsNull());
    });

    runTest("抽壳", [&] {
        Document doc;
        Feature f;
        f.type = FeatureType::Box; f.p1 = 30; f.p2 = 20; f.p3 = 10;
        auto box = executeFeature(f, {}, {}, doc);
        Feature sh;
        sh.type = FeatureType::Shell;
        sh.p1 = 2;
        sh.faceAnchors = {{15, 10, 10}}; // 顶面
        auto out = executeFeature(sh, box, {}, doc);
        CHECK(!out.IsNull());
        CHECK(volumeOf(out) < volumeOf(box));
    });

    runTest("网格剖分(并行)", [&] {
        Document doc;
        Feature f;
        f.type = FeatureType::Cylinder; f.p1 = 10; f.p2 = 30;
        auto cyl = executeFeature(f, {}, {}, doc);
        MeshData md = buildMesh(cyl, 0.05);
        CHECK(!md.verts.empty());
        CHECK(md.verts.size() == md.normals.size());
        CHECK(md.verts.size() % 9 == 0);
        CHECK(!md.edgeLines.empty());
        // 面数量: 圆柱 = 侧面 + 2 底
        CHECK(md.faceCount.size() == 3);
        // 体积抽查: 用网格体积近似
        double vol = 0;
        for (size_t i = 0; i < md.verts.size(); i += 9) {
            const float* a = &md.verts[i];
            const float* b = a + 3;
            const float* c = b + 3;
            vol += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                    a[2] * (b[0] * c[1] - b[1] * c[0])) /
                   6.0;
        }
        CHECK(std::fabs(std::fabs(vol) - M_PI * 100 * 30) / (M_PI * 100 * 30) < 0.02);
    });

    runTest("导入体: 内嵌 BREP 序列化往返", [&] {
        Document doc;
        Body& b = doc.addBody("导入体");
        Feature f;
        f.id = doc.newId();
        f.type = FeatureType::Imported;
        f.name = "STEP导入";
        f.result = BRepPrimAPI_MakeCylinder(5.0, 10.0).Shape(); // 模拟外部导入的几何
        b.features.push_back(f);
        doc.recomputeAll();
        double v0 = volumeOf(doc.bodies[0].result);
        CHECK_NEAR(v0, M_PI * 25 * 10, 1e-3);

        Document doc2;
        CHECK(doc2.deserialize(doc.serialize()));
        CHECK(doc2.bodies.size() == 1);
        doc2.recomputeAll();
        CHECK(!doc2.bodies[0].result.IsNull());
        CHECK_NEAR(volumeOf(doc2.bodies[0].result), v0, 1e-6);
    });

    runTest("文档序列化往返", [&] {
        Document doc;
        buildWelcomeDocument(doc);
        std::string s = doc.serialize();
        Document doc2;
        CHECK(doc2.deserialize(s));
        CHECK(doc2.bodies.size() == 1);
        CHECK(doc2.sketches.size() == 2);
        CHECK(doc2.bodies[0].features.size() == 3);
        doc2.recomputeAll();
        CHECK(!doc2.bodies[0].result.IsNull());
    });

    runTest("质量属性", [&] {
        Document doc;
        Feature f;
        f.type = FeatureType::Box; f.p1 = 20; f.p2 = 10; f.p3 = 5;
        auto box = executeFeature(f, {}, {}, doc);
        MassProps mp = massProperties(box, 2.7);
        CHECK(mp.ok);
        CHECK_NEAR(mp.volumeMm3, 1000.0, 1e-6);
        CHECK_NEAR(mp.massG, 2.7, 1e-6);
        CHECK_NEAR(mp.centroid.X(), 0.0, 1e-9); // 中心在原点
    });

    runTest("测量: 距离与面夹角", [&] {
        // 两圆柱轴心距 = 40 (BRepExtrema)
        TopoDS_Shape c1 = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 5.0, 10.0).Shape();
        TopoDS_Shape c2 = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(40, 0, 0), gp_Dir(0, 0, 1)), 5.0, 10.0).Shape();
        MeasureResult d = measureDistance(c1, c2);
        CHECK(d.ok);
        CHECK_NEAR(d.value, 30.0, 1e-6); // 面间最小距离 = 40 - 5 - 5
        // 最近点集是两条线段 (z 任意), 只验证落在解集上
        CHECK_NEAR(std::hypot(d.p1.X(), d.p1.Y()), 5.0, 1e-6);
        CHECK(d.p1.Z() > -1e-6 && d.p1.Z() < 10.0 + 1e-6);
        CHECK_NEAR(std::hypot(d.p2.X(), d.p2.Y()), 35.0, 1e-6);
        CHECK(d.p2.Z() > -1e-6 && d.p2.Z() < 10.0 + 1e-6);
        // 同体距离 = 0
        MeasureResult d0 = measureDistance(c1, c1);
        CHECK(d0.ok);
        CHECK_NEAR(d0.value, 0.0, 1e-9);
        // 平面夹角: XY 面 vs XZ 面 = 90°
        BRepBuilderAPI_MakeFace fxy(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
        BRepBuilderAPI_MakeFace fxz(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0)));
        MeasureResult a = measureAngle(fxy.Face(), fxz.Face());
        CHECK(a.ok);
        CHECK_NEAR(a.value, 90.0, 1e-6);
    });

    return testSummary();
}
