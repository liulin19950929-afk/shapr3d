// main.cpp —— 入口
#include "app/Application.h"
#include "core/Common.h"
#include <cstdio>

int main(int argc, char** argv) {
    LOGI("Shapr3D 桌面版 (开源实现) — OCCT %s 内核 / 自研约束求解器 / OpenGL ES 2.0 渲染",
         OCC_VERSION_COMPLETE);
    cad::Application app;
    int rc = app.run();
    LOGI("退出码 %d", rc);
    return rc;
}
