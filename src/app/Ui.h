// Ui.h —— ImGui 界面层
#pragma once
#include "Application.h"
#include "imgui.h"

namespace cad::ui {

void drawAll(Application& app);
void drawDrawingCanvas(Application& app, ImDrawList* dl, float w, float h);

} // namespace cad::ui
