// test_solver.cpp —— 约束求解器测试
#include "TestMain.h"
#include "../src/sketch/SketchDef.h"
#include "../src/sketch/Solver.h"

using namespace cad;

int main() {
    ConstraintSolver solver;

    runTest("水平约束 + 距离约束", [&] {
        SketchDef sk;
        sk.plane = planeXY();
        sk.addPoint(0, 0, 1);
        sk.addPoint(10, 7, 2);
        sk.setPointFixed(1);
        sk.addLine(1, 2, 3);
        Constraint h;
        h.id = 10; h.type = CstType::Horizontal; h.refs = {3};
        sk.constraints.push_back(h);
        Constraint d;
        d.id = 11; d.type = CstType::Length; d.refs = {3}; d.value = 42;
        sk.constraints.push_back(d);

        auto r = solver.solve(sk);
        CHECK(r.status != SolveStatus::Failed);
        CHECK(std::fabs(sk.pointPos(2).y - sk.pointPos(1).y) < 1e-6);
        CHECK_NEAR(dist(sk.pointPos(1), sk.pointPos(2)), 42.0, 1e-6);
        CHECK_NEAR(r.residual, 0.0, 1e-6);
    });

    runTest("矩形: 水平/垂直/尺寸 完全约束", [&] {
        SketchDef sk;
        sk.plane = planeXY();
        auto& a = sk.addPoint(0, 0, 1);
        auto& b = sk.addPoint(10, 0, 2);
        auto& c = sk.addPoint(10, 8, 3);
        auto& d = sk.addPoint(2, 8, 4);
        sk.addLine(a.id, b.id, 11);
        sk.addLine(b.id, c.id, 12);
        sk.addLine(c.id, d.id, 13);
        sk.addLine(d.id, a.id, 14);
        auto addC = [&](CstType t, std::vector<Id> refs, double v = 0) {
            Constraint c2;
            c2.id = sk.constraints.size() + 100;
            c2.type = t;
            c2.refs = std::move(refs);
            c2.value = v;
            sk.constraints.push_back(c2);
        };
        addC(CstType::Fix, {1});              // 固定左下角
        addC(CstType::Horizontal, {11});
        addC(CstType::Vertical, {12});
        addC(CstType::Horizontal, {13});
        addC(CstType::Vertical, {14});
        addC(CstType::Length, {11}, 60);      // 长 60
        addC(CstType::Length, {12}, 40);      // 宽 40

        auto r = solver.solve(sk);
        CHECK(r.status == SolveStatus::Ok || r.status == SolveStatus::UnderConstrained);
        CHECK_NEAR(dist(sk.pointPos(1), sk.pointPos(2)), 60.0, 1e-5);
        CHECK_NEAR(dist(sk.pointPos(2), sk.pointPos(3)), 40.0, 1e-5);
        CHECK(sk.pointPos(1).x * sk.pointPos(1).x + sk.pointPos(1).y * sk.pointPos(1).y < 1e-9);
    });

    runTest("圆: 同心 + 半径", [&] {
        SketchDef sk;
        sk.plane = planeXY();
        auto& c1 = sk.addPoint(3, 4, 1);
        auto& c2 = sk.addPoint(9, 9, 2);
        sk.addCircle(c1.id, 5, 11);
        sk.addCircle(c2.id, 9, 12);
        Constraint con;
        con.id = 100; con.type = CstType::Concentric; con.refs = {11, 12};
        sk.constraints.push_back(con);
        Constraint rr;
        rr.id = 101; rr.type = CstType::Radius; rr.refs = {12}; rr.value = 7;
        sk.constraints.push_back(rr);
        sk.setPointFixed(1, 0, 0);

        auto r = solver.solve(sk);
        CHECK(r.status != SolveStatus::Failed);
        CHECK_NEAR(sk.circle(12)->r, 7.0, 1e-6);
        CHECK_NEAR(dist(sk.pointPos(1), sk.pointPos(2)), 0.0, 1e-6);
    });

    runTest("拖拽求解(软约束)", [&] {
        SketchDef sk;
        sk.plane = planeXY();
        sk.addPoint(0, 0, 1);
        sk.addPoint(10, 0, 2);
        sk.setPointFixed(1);
        sk.addLine(1, 2, 3);
        Constraint d;
        d.id = 10; d.type = CstType::Length; d.refs = {3}; d.value = 50;
        sk.constraints.push_back(d);
        solver.solve(sk);

        // 拖到 (35, 12): 长度约束应保持 50
        auto r = solver.solveDrag(sk, 2, {35, 12});
        CHECK(r.status == SolveStatus::OkDrag);
        CHECK_NEAR(dist(sk.pointPos(1), sk.pointPos(2)), 50.0, 1e-4);
        CHECK(sk.pointPos(2).x > 30); // 大致在拖拽方向
    });

    runTest("平行/垂直/角度", [&] {
        SketchDef sk;
        sk.plane = planeXY();
        sk.addPoint(0, 0, 1);
        sk.addPoint(10, 0, 2);
        sk.addPoint(1, 1, 3);
        sk.addPoint(6, 9, 4);
        sk.setPointFixed(1);
        sk.addLine(1, 2, 11);
        sk.addLine(3, 4, 12);
        Constraint perp;
        perp.id = 100; perp.type = CstType::Perpendicular; perp.refs = {11, 12};
        sk.constraints.push_back(perp);
        Constraint ang;
        ang.id = 101; ang.type = CstType::Angle; ang.refs = {11, 12}; ang.value = M_PI / 2;
        sk.constraints.push_back(ang);

        auto r = solver.solve(sk);
        CHECK(r.status != SolveStatus::Failed);
        Vec2 d1 = sk.pointPos(2) - sk.pointPos(1);
        Vec2 d2 = sk.pointPos(4) - sk.pointPos(3);
        CHECK_NEAR(d1.normalized().dot(d2.normalized()), 0.0, 1e-5);
    });

    runTest("过约束检测", [&] {
        SketchDef sk;
        sk.plane = planeXY();
        sk.addPoint(0, 0, 1);
        sk.addPoint(10, 0, 2);
        sk.setPointFixed(1);
        sk.addLine(1, 2, 3);
        Constraint h; h.id = 100; h.type = CstType::Horizontal; h.refs = {3};
        Constraint d1; d1.id = 101; d1.type = CstType::Length; d1.refs = {3}; d1.value = 10;
        Constraint d2; d2.id = 102; d2.type = CstType::Length; d2.refs = {3}; d2.value = 99; // 冲突
        sk.constraints.push_back(h);
        sk.constraints.push_back(d1);
        sk.constraints.push_back(d2);
        auto r = solver.solve(sk);
        CHECK(r.status == SolveStatus::OverConstrained || r.residual > 1e-3);
    });

    runTest("自由度统计", [&] {
        SketchDef sk;
        sk.plane = planeXY();
        auto& p1 = sk.addPoint(0, 0, 1);
        auto& p2 = sk.addPoint(10, 0, 2);
        sk.addLine(p1.id, p2.id, 3);
        int free0 = solver.computeDof(sk);         // 2点4参
        CHECK(free0 == 4);
        Constraint d; d.id = 10; d.type = CstType::Length; d.refs = {3}; d.value = 10;
        sk.constraints.push_back(d);
        int free1 = solver.computeDof(sk);
        CHECK(free1 == 3);
    });

    return testSummary();
}
