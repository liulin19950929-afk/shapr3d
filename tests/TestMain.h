// TestMain.h —— 极简测试框架
#pragma once
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <functional>

static int g_fail = 0, g_total = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_total;                                                                  \
        if (!(cond)) {                                                              \
            ++g_fail;                                                               \
            fprintf(stderr, "  [失败] %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                           \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                       \
    do {                                                                            \
        ++g_total;                                                                  \
        double _a = (a), _b = (b);                                                  \
        if (!(std::fabs(_a - _b) <= (tol))) {                                       \
            ++g_fail;                                                               \
            fprintf(stderr, "  [失败] %s:%d  |%.6f - %.6f| > %g\n", __FILE__,       \
                    __LINE__, _a, _b, tol);                                         \
        }                                                                           \
    } while (0)

inline void runTest(const char* name, const std::function<void()>& fn) {
    fprintf(stderr, "---- 测试: %s ----\n", name);
    int before = g_fail;
    fn();
    fprintf(stderr, "     %s (%d/%d 通过)\n", g_fail == before ? "通过" : "存在失败",
            g_total - g_fail, g_total);
}

static int testSummary() {
    fprintf(stderr, "\n========== 合计 %d 项, 失败 %d ==========\n", g_total, g_fail);
    return g_fail ? 1 : 0;
}
