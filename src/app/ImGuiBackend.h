// ImGuiBackend.h —— 自研 Dear ImGui 后端(GLFW 事件 + GLES2/桌面GL 渲染)
#pragma once
#include <string>
#include "imgui.h"
struct GLFWwindow;

namespace cad::ui {

bool ImGuiInit(GLFWwindow* window, const std::string& fontPath);
void ImGuiShutdown();
void ImGuiNewFrame(GLFWwindow* window);
void ImGuiRender(); // ImGui::Render() + GL 绘制
void imGuiScroll(float dx, float dy);   // GLFW 回调注入
void imGuiChar(unsigned int c);         // 文本输入注入

} // namespace cad::ui
