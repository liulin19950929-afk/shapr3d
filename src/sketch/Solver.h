// Solver.h —— 自研草图几何约束求解器
// 思路: 将草图实体参数(点坐标/半径/角度)展平为参数向量 x,
//       每条约束 -> 残差 r_i(x); 用 Levenberg-Marquardt(阻尼最小二乘)
//       迭代 min ||r||², 数值中心差分雅可比, 高斯消元解法方程。
//       QR 列主元秩估计用于自由度统计与过约束检测。
#pragma once
#include "../core/Common.h"
#include "SketchDef.h"

namespace cad {

enum class SolveStatus {
    Ok,              // 收敛
    OkDrag,          // 拖拽模式收敛
    UnderConstrained,// 收敛但欠约束(正常)
    OverConstrained, // 检测到过约束
    Failed,          // 不收敛
    Empty
};

struct SolveResult {
    SolveStatus status = SolveStatus::Empty;
    int iterations = 0;
    double residual = 0;
    int dof = -1;         // 剩余自由度(欠约束数), -1 = 未知
    bool overConstrained = false;
    std::string message;
};

class ConstraintSolver {
public:
    // ---- 参数打包 ----
    struct ParamMap {
        // 点 id -> 参数基址; 圆/弧 -> 半径、角度地址
        std::vector<Id> ptIds;
        std::vector<Id> circIds, arcIds;
        int nPoints = 0, nRadii = 0, nAngles = 0;
        int total() const { return 2 * nPoints + nRadii + 2 * nAngles; }
    };
    // 拖拽软约束: (pid, tx, ty)
    struct DragPin { Id pid = kInvalidId; double tx = 0, ty = 0, w = 1; bool active = false; };

    // 对 sketch 求解(就地修改坐标)
    SolveResult solve(SketchDef& sk);

    // 拖拽: 强制把 draggedPid 拉到 target(临时软约束), 求解后坐标更新
    SolveResult solveDrag(SketchDef& sk, Id draggedPid, const Vec2& target);

    // 自由度统计(不解算)
    int computeDof(const SketchDef& sk);

private:
    // 雅可比矩阵(秩估计用)
    std::vector<double> residualJacobian(const SketchDef& sk, const ParamMap& m,
                                         const std::vector<double>& x, const DragPin& pin) const;

    static ParamMap buildMap(const SketchDef& sk);
    static std::vector<double> packParams(const SketchDef& sk, const ParamMap& m);
    static void unpackParams(SketchDef& sk, const ParamMap& m, const std::vector<double>& x);

    std::vector<double> residuals(const SketchDef& sk, const ParamMap& m,
                                  const std::vector<double>& x, const DragPin& pin) const;

    SolveResult runLM(SketchDef& sk, const DragPin& pin);
};

} // namespace cad
