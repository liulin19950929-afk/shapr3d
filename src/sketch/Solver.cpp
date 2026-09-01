// Solver.cpp —— Levenberg-Marquardt 阻尼最小二乘 + 秩估计
#include "Solver.h"
#include <unordered_map>
#include <cmath>
#include <cstring>

namespace cad {

// 小型稠密线性代数
namespace {

// 解 A x = b (部分主元高斯消元); n 阶; 就地, 失败返回 false
bool gaussSolve(std::vector<double>& A, std::vector<double>& b, int n) {
    for (int col = 0; col < n; ++col) {
        int piv = col;
        double best = std::fabs(A[col * n + col]);
        for (int r = col + 1; r < n; ++r) {
            double v = std::fabs(A[r * n + col]);
            if (v > best) { best = v; piv = r; }
        }
        if (best < 1e-14) return false;
        if (piv != col) {
            for (int c = 0; c < n; ++c) std::swap(A[col * n + c], A[piv * n + c]);
            std::swap(b[col], b[piv]);
        }
        double d = A[col * n + col];
        for (int r = col + 1; r < n; ++r) {
            double f = A[r * n + col] / d;
            if (f == 0) continue;
            for (int c = col; c < n; ++c) A[r * n + c] -= f * A[col * n + c];
            b[r] -= f * b[col];
        }
    }
    for (int r = n - 1; r >= 0; --r) {
        double s = b[r];
        for (int c = r + 1; c < n; ++c) s -= A[r * n + c] * b[c];
        b[r] = s / A[r * n + r];
    }
    return true;
}

// QR 列主元秩估计 (Householder 简化版: 用 Gram-Schmidt + 范数选主元)
int matrixRank(std::vector<double> J, int rows, int cols) {
    std::vector<double> colNorm(cols);
    for (int j = 0; j < cols; ++j) {
        double s = 0;
        for (int i = 0; i < rows; ++i) s += J[i * cols + j] * J[i * cols + j];
        colNorm[j] = s;
    }
    int rank = 0;
    std::vector<bool> used(cols, false);
    for (int k = 0; k < cols; ++k) {
        // 选最大剩余范数列
        int best = -1;
        double bn = 0;
        for (int j = 0; j < cols; ++j)
            if (!used[j] && colNorm[j] > bn) { bn = colNorm[j]; best = j; }
        if (best < 0 || bn < 1e-16) break;
        used[best] = true;
        if (bn < 1e-14) break;
        ++rank;
        double invNorm = 1.0 / std::sqrt(bn);
        // 正交化其余列
        for (int j = 0; j < cols; ++j) {
            if (used[j]) continue;
            double dot = 0;
            for (int i = 0; i < rows; ++i) dot += J[i * cols + best] * J[i * cols + j];
            dot *= invNorm * invNorm * -1.0; // 减去投影分量系数
            for (int i = 0; i < rows; ++i) J[i * cols + j] += dot * J[i * cols + best];
            double s = 0;
            for (int i = 0; i < rows; ++i) s += J[i * cols + j] * J[i * cols + j];
            colNorm[j] = s;
        }
    }
    return rank;
}

} // namespace

// ---------------- 参数打包 ----------------
ConstraintSolver::ParamMap ConstraintSolver::buildMap(const SketchDef& sk) {
    ParamMap m;
    for (auto& p : sk.points) m.ptIds.push_back(p.id);
    for (auto& c : sk.circles) m.circIds.push_back(c.id);
    for (auto& a : sk.arcs) m.arcIds.push_back(a.id);
    m.nPoints = (int)m.ptIds.size();
    m.nRadii = (int)(m.circIds.size() + m.arcIds.size());
    m.nAngles = (int)m.arcIds.size();
    return m;
}

std::vector<double> ConstraintSolver::packParams(const SketchDef& sk, const ParamMap& m) {
    std::vector<double> x(m.total(), 0.0);
    int k = 0;
    for (auto id : m.ptIds) {
        for (auto& p : sk.points) if (p.id == id) { x[k] = p.x; x[k + 1] = p.y; }
        k += 2;
    }
    for (auto id : m.circIds)
        for (auto& c : sk.circles) if (c.id == id) x[k++] = c.r;
    for (auto id : m.arcIds)
        for (auto& a : sk.arcs)
            if (a.id == id) { x[k++] = a.r; x[k++] = a.a0; x[k++] = a.a1; }
    return x;
}

void ConstraintSolver::unpackParams(SketchDef& sk, const ParamMap& m, const std::vector<double>& x) {
    int k = 0;
    for (auto id : m.ptIds) {
        for (auto& p : sk.points) if (p.id == id) { p.x = x[k]; p.y = x[k + 1]; }
        k += 2;
    }
    for (auto id : m.circIds)
        for (auto& c : sk.circles) if (c.id == id) c.r = x[k++];
    for (auto id : m.arcIds)
        for (auto& a : sk.arcs)
            if (a.id == id) { a.r = x[k++]; a.a0 = x[k++]; a.a1 = x[k++]; }
}

// ---------------- 残差 ----------------
namespace {

struct Ref {
    const SketchDef& sk;
    const ConstraintSolver::ParamMap& m;
    const std::vector<double>& x;

    bool ptIndex(Id id, int& out) const {
        for (size_t i = 0; i < m.ptIds.size(); ++i)
            if (m.ptIds[i] == id) { out = (int)i; return true; }
        return false;
    }
    Vec2 pt(Id id) const {
        int i;
        if (ptIndex(id, i)) return {x[2 * i], x[2 * i + 1]};
        return sk.pointPos(id);
    }
    double radiusC(Id circId) const {
        for (size_t i = 0; i < m.circIds.size(); ++i)
            if (m.circIds[i] == circId) return x[2 * m.nPoints + i];
        for (auto& c : sk.circles) if (c.id == circId) return c.r;
        return 0;
    }
    // 弧: 基址
    int arcBase(Id arcId) const {
        int base = 2 * m.nPoints + m.nRadii;
        for (size_t i = 0; i < m.arcIds.size(); ++i)
            if (m.arcIds[i] == arcId) return base + 3 * (int)i;
        return -1;
    }
};

double segLength(const Ref& R, Id l1) {
    for (auto& l : R.sk.lines)
        if (l.id == l1) return dist(R.pt(l.p1), R.pt(l.p2));
    return 0;
}
Vec2 segDir(const Ref& R, Id l1) {
    for (auto& l : R.sk.lines) {
        if (l.id != l1) continue;
        Vec2 d = R.pt(l.p2) - R.pt(l.p1);
        double len = d.length();
        return len > 1e-12 ? d * (1.0 / len) : Vec2{1, 0};
    }
    return {1, 0};
}

// 点到直线的有向距离
double ptLineDist(const Ref& R, Id pid, Id lineId) {
    for (auto& l : R.sk.lines) {
        if (l.id != lineId) continue;
        Vec2 a = R.pt(l.p1), b = R.pt(l.p2), p = R.pt(pid);
        Vec2 d = b - a;
        double len = d.length();
        if (len < 1e-12) return dist(p, a);
        return d.cross(p - a) / len; // 有向
    }
    return 0;
}

} // namespace

std::vector<double> ConstraintSolver::residuals(const SketchDef& sk, const ParamMap& m,
                                                const std::vector<double>& x, const DragPin& pin) const {
    Ref R{sk, m, x};
    std::vector<double> r;
    r.reserve(sk.constraints.size() * 2 + 8);

    auto push = [&](double v) { r.push_back(v); };

    for (const auto& c : sk.constraints) {
        switch (c.type) {
            case CstType::Coincident: {
                if (c.refs.size() == 2) {
                    Vec2 a = R.pt(c.refs[0]), b = R.pt(c.refs[1]);
                    push(a.x - b.x);
                    push(a.y - b.y);
                }
                break;
            }
            case CstType::Horizontal: {
                Id p1 = c.refs[0], p2 = c.refs[1];
                for (auto& l : sk.lines)
                    if (l.id == c.refs[0]) { p1 = l.p1; p2 = l.p2; }
                push(R.pt(p1).y - R.pt(p2).y);
                break;
            }
            case CstType::Vertical: {
                Id p1 = c.refs[0], p2 = c.refs[1];
                for (auto& l : sk.lines)
                    if (l.id == c.refs[0]) { p1 = l.p1; p2 = l.p2; }
                push(R.pt(p1).x - R.pt(p2).x);
                break;
            }
            case CstType::Parallel: {
                Vec2 d1 = segDir(R, c.refs[0]), d2 = segDir(R, c.refs[1]);
                push(d1.cross(d2)); // sin
                break;
            }
            case CstType::Perpendicular: {
                Vec2 d1 = segDir(R, c.refs[0]), d2 = segDir(R, c.refs[1]);
                push(d1.dot(d2)); // cos
                break;
            }
            case CstType::Distance: {
                Vec2 a = R.pt(c.refs[0]), b = R.pt(c.refs[1]);
                push(dist(a, b) - c.value);
                break;
            }
            case CstType::DistPtLine:
                push(ptLineDist(R, c.refs[0], c.refs[1]) - c.value);
                break;
            case CstType::Length:
                push(segLength(R, c.refs[0]) - c.value);
                break;
            case CstType::Radius:
                push(R.radiusC(c.refs[0]) - c.value);
                break;
            case CstType::Diameter:
                push(2 * R.radiusC(c.refs[0]) - c.value);
                break;
            case CstType::Angle: {
                Vec2 d1 = segDir(R, c.refs[0]), d2 = segDir(R, c.refs[1]);
                double ang = std::atan2(d1.cross(d2), d1.dot(d2));
                double target = c.value;
                // 最小旋转路径
                while (ang - target > M_PI) target += 2 * M_PI;
                while (ang - target < -M_PI) target -= 2 * M_PI;
                push(ang - target);
                break;
            }
            case CstType::Equal: {
                // 长度相等 或 半径相等
                bool c1 = sk.circle(c.refs[0]) || sk.arc(c.refs[0]);
                bool c2 = sk.circle(c.refs[1]) || sk.arc(c.refs[1]);
                if (c1 && c2) push(R.radiusC(c.refs[0]) - R.radiusC(c.refs[1]));
                else push(segLength(R, c.refs[0]) - segLength(R, c.refs[1]));
                break;
            }
            case CstType::Midpoint: {
                for (auto& l : sk.lines) {
                    if (l.id != c.refs[1]) continue;
                    Vec2 a = R.pt(l.p1), b = R.pt(l.p2), p = R.pt(c.refs[0]);
                    push(p.x - 0.5 * (a.x + b.x));
                    push(p.y - 0.5 * (a.y + b.y));
                }
                break;
            }
            case CstType::PointOnLine:
                push(ptLineDist(R, c.refs[0], c.refs[1]));
                break;
            case CstType::PointOnCircle: {
                Id ct1 = kInvalidId;
                for (auto& ci : sk.circles) if (ci.id == c.refs[1]) ct1 = ci.center;
                for (auto& a : sk.arcs) if (a.id == c.refs[1]) ct1 = a.center;
                if (ct1) {
                    double rr = R.radiusC(c.refs[1]);
                    Vec2 p = R.pt(c.refs[0]);
                    push(dist(p, R.pt(ct1)) - rr);
                }
                break;
            }
            case CstType::Concentric: {
                Id ct1 = kInvalidId, ct2 = kInvalidId;
                for (auto& ci : sk.circles) { if (ci.id == c.refs[0]) ct1 = ci.center; if (ci.id == c.refs[1]) ct2 = ci.center; }
                for (auto& a : sk.arcs) { if (a.id == c.refs[0]) ct1 = a.center; if (a.id == c.refs[1]) ct2 = a.center; }
                if (ct1 && ct2) {
                    Vec2 a = R.pt(ct1), b = R.pt(ct2);
                    push(a.x - b.x);
                    push(a.y - b.y);
                }
                break;
            }
            case CstType::Fix: {
                Vec2 a = R.pt(c.refs[0]);
                push(a.x - c.value);
                push(a.y - c.value2);
                break;
            }
        }
    }

    // 用户固定点
    for (size_t i = 0; i < m.ptIds.size(); ++i) {
        for (auto& p : sk.points)
            if (p.id == m.ptIds[i] && p.fixed) {
                push(x[2 * i] - p.x);
                push(x[2 * i + 1] - p.y);
            }
    }

    // 拖拽软约束
    if (pin.active) {
        int i;
        if (R.ptIndex(pin.pid, i)) {
            push((x[2 * i] - pin.tx) * 1e-6);
            push((x[2 * i + 1] - pin.ty) * 1e-6);
        }
    }
    return r;
}

SolveResult ConstraintSolver::runLM(SketchDef& sk, const DragPin& pin) {
    ParamMap m = buildMap(sk);
    SolveResult res;
    int n = m.total();
    if (n == 0) { res.status = SolveStatus::Empty; return res; }

    std::vector<double> x = packParams(sk, m);
    auto scale = x; // 步长参考尺度
    for (auto& s : scale) s = std::max(1.0, std::fabs(s));

    std::vector<double> r = residuals(sk, m, x, pin);
    auto costOf = [&](const std::vector<double>& xx) {
        auto rr = residuals(sk, m, xx, pin);
        double c = 0;
        for (double v : rr) c += v * v;
        return c;
    };
    double cost = costOf(x);
    double lambda = 1e-3;

    const int kMaxIter = 120;
    for (int it = 0; it < kMaxIter; ++it) {
        res.iterations = it + 1;
        // 数值雅可比(中心差分)
        int nr = (int)r.size();
        std::vector<double> J((size_t)nr * n, 0.0);
        for (int j = 0; j < n; ++j) {
            double h = 1e-7 * scale[j];
            std::vector<double> xp = x, xm = x;
            xp[j] += h; xm[j] -= h;
            auto rp = residuals(sk, m, xp, pin);
            auto rm = residuals(sk, m, xm, pin);
            for (int i = 0; i < nr; ++i)
                J[(size_t)i * n + j] = (rp[i] - rm[i]) / (2 * h);
        }
        // 法方程 A = JᵀJ + λ·diag(JᵀJ), g = -Jᵀr (下降方向)
        std::vector<double> A((size_t)n * n, 0.0), g(n, 0.0);
        for (int i = 0; i < nr; ++i) {
            const double* Ji = &J[(size_t)i * n];
            for (int a = 0; a < n; ++a) {
                if (Ji[a] == 0) continue;
                g[a] -= Ji[a] * r[i];
                for (int b = 0; b < n; ++b) A[(size_t)a * n + b] += Ji[a] * Ji[b];
            }
        }
        for (int a = 0; a < n; ++a) A[(size_t)a * n + a] *= (1.0 + lambda);
        // 防奇异
        for (int a = 0; a < n; ++a) A[(size_t)a * n + a] += 1e-12;

        std::vector<double> dx = g;
        if (!gaussSolve(A, dx, n)) { lambda *= 10; if (lambda > 1e8) break; continue; }

        // 线搜索
        double best = cost;
        std::vector<double> bestX = x;
        double t = 1.0;
        for (int ls = 0; ls < 6; ++ls, t *= 0.5) {
            std::vector<double> xn = x;
            for (int j = 0; j < n; ++j) xn[j] += t * dx[j];
            double cn = costOf(xn);
            if (cn < best) { best = cn; bestX = std::move(xn); break; }
        }
        if (best < cost) {
            double improve = cost - best;
            x = std::move(bestX);
            cost = best;
            lambda = std::max(lambda / 4, 1e-9);
            r = residuals(sk, m, x, pin);
            double dxNorm = 0;
            for (int j = 0; j < n; ++j) dxNorm += dx[j] * dx[j];
            if (std::sqrt(dxNorm) < 1e-9 || cost < 1e-12 || improve < 1e-14) break;
        } else {
            lambda *= 8;
            if (lambda > 1e10) break;
        }
    }

    unpackParams(sk, m, x);
    res.residual = cost;
    res.dof = n - matrixRank(std::move(residualJacobian(sk, m, x, pin)), (int)r.size(), n);
    res.overConstrained = (res.dof <= 0 && cost > 1e-6);

    if (pin.active) res.status = SolveStatus::OkDrag;
    else if (res.overConstrained) { res.status = SolveStatus::OverConstrained; res.message = "过约束或约束冲突, 请检查约束"; }
    else if (res.dof > 0) res.status = SolveStatus::UnderConstrained;
    else res.status = SolveStatus::Ok;
    return res;
}

// 供秩估计的显式雅可比
std::vector<double> ConstraintSolver::residualJacobian(const SketchDef& sk, const ParamMap& m,
                                                       const std::vector<double>& x, const DragPin& pin) const {
    auto r0 = residuals(sk, m, x, pin);
    int nr = (int)r0.size(), n = m.total();
    std::vector<double> J((size_t)std::max(nr, 1) * std::max(n, 1), 0.0);
    for (int j = 0; j < n; ++j) {
        double h = 1e-7 * std::max(1.0, std::fabs(x[j]));
        std::vector<double> xp = x, xm = x;
        xp[j] += h; xm[j] -= h;
        auto rp = residuals(sk, m, xp, pin);
        auto rm = residuals(sk, m, xm, pin);
        for (int i = 0; i < nr; ++i)
            J[(size_t)i * n + j] = (rp[i] - rm[i]) / (2 * h);
    }
    return J;
}

SolveResult ConstraintSolver::solve(SketchDef& sk) {
    DragPin none;
    return runLM(sk, none);
}

SolveResult ConstraintSolver::solveDrag(SketchDef& sk, Id draggedPid, const Vec2& target) {
    // 先把被拖点搬到目标处, 再带极低权重软拉求解 ——
    // 硬约束(尺寸等)严格成立, 点落在可行域上离光标最近处
    for (auto& p : sk.points)
        if (p.id == draggedPid && !p.fixed) {
            p.x = target.x;
            p.y = target.y;
        }
    DragPin pin;
    pin.active = true;
    pin.pid = draggedPid;
    pin.tx = target.x;
    pin.ty = target.y;
    return runLM(sk, pin);
}

int ConstraintSolver::computeDof(const SketchDef& sk) {
    ParamMap m = buildMap(sk);
    int n = m.total();
    if (n == 0) return 0;
    std::vector<double> x = packParams(sk, m);
    DragPin none;
    auto r = residuals(sk, m, x, none);
    return n - matrixRank(residualJacobian(sk, m, x, none), (int)r.size(), n);
}

} // namespace cad
