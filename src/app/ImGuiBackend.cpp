// ImGuiBackend.cpp
#include "ImGuiBackend.h"
#include "../render/GLApi.h"
#include "../core/Common.h"
#include <GLFW/glfw3.h>
#include <cstring>
#include <unordered_map>

namespace cad::ui {

using namespace gl;

static GLFWwindow* g_window = nullptr;
static double g_lastTime = 0;
static float g_mouseWheelX = 0, g_mouseWheelY = 0;
static GLuint g_fontTex = 0;
static GLuint g_shader = 0, g_vbo = 0, g_ibo = 0;
static GLint g_uMVP = -1, g_uTex = -1, g_aPos = -1, g_aUV = -1, g_aCol = -1;

static const char* kImGuiVS = R"(
attribute vec2 aPos;
attribute vec2 aUV;
attribute vec4 aColor;
uniform mat4 uMVP;
varying vec2 vUV;
varying vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
}
)";
static const char* kImGuiFS = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec2 vUV;
varying vec4 vColor;
uniform sampler2D uTex;
void main() { gl_FragColor = vColor * texture2D(uTex, vUV); }
)";

static GLuint compile(const char* vs, const char* fs) {
    GLint ok = 0;
    GLuint v = pCreateShader(VERTEX_SHADER);
    pShaderSource(v, 1, &vs, nullptr);
    pCompileShader(v);
    pGetShaderiv(v, COMPILE_STATUS, &ok);
    if (!ok) return 0;
    GLuint f = pCreateShader(FRAGMENT_SHADER);
    pShaderSource(f, 1, &fs, nullptr);
    pCompileShader(f);
    pGetShaderiv(f, COMPILE_STATUS, &ok);
    if (!ok) return 0;
    GLuint p = pCreateProgram();
    pAttachShader(p, v);
    pAttachShader(p, f);
    pLinkProgram(p);
    return p;
}

static ImGuiKey mapKey(int glfwKey) {
    static const std::unordered_map<int, ImGuiKey> m = {
        {GLFW_KEY_TAB, ImGuiKey_Tab}, {GLFW_KEY_LEFT, ImGuiKey_LeftArrow},
        {GLFW_KEY_RIGHT, ImGuiKey_RightArrow}, {GLFW_KEY_UP, ImGuiKey_UpArrow},
        {GLFW_KEY_DOWN, ImGuiKey_DownArrow}, {GLFW_KEY_PAGE_UP, ImGuiKey_PageUp},
        {GLFW_KEY_PAGE_DOWN, ImGuiKey_PageDown}, {GLFW_KEY_HOME, ImGuiKey_Home},
        {GLFW_KEY_END, ImGuiKey_End}, {GLFW_KEY_INSERT, ImGuiKey_Insert},
        {GLFW_KEY_DELETE, ImGuiKey_Delete}, {GLFW_KEY_BACKSPACE, ImGuiKey_Backspace},
        {GLFW_KEY_SPACE, ImGuiKey_Space}, {GLFW_KEY_ENTER, ImGuiKey_Enter},
        {GLFW_KEY_ESCAPE, ImGuiKey_Escape}, {GLFW_KEY_APOSTROPHE, ImGuiKey_Apostrophe},
        {GLFW_KEY_COMMA, ImGuiKey_Comma}, {GLFW_KEY_MINUS, ImGuiKey_Minus},
        {GLFW_KEY_PERIOD, ImGuiKey_Period}, {GLFW_KEY_SLASH, ImGuiKey_Slash},
        {GLFW_KEY_SEMICOLON, ImGuiKey_Semicolon}, {GLFW_KEY_EQUAL, ImGuiKey_Equal},
        {GLFW_KEY_LEFT_BRACKET, ImGuiKey_LeftBracket}, {GLFW_KEY_BACKSLASH, ImGuiKey_Backslash},
        {GLFW_KEY_RIGHT_BRACKET, ImGuiKey_RightBracket}, {GLFW_KEY_GRAVE_ACCENT, ImGuiKey_GraveAccent},
        {GLFW_KEY_CAPS_LOCK, ImGuiKey_CapsLock}, {GLFW_KEY_SCROLL_LOCK, ImGuiKey_ScrollLock},
        {GLFW_KEY_NUM_LOCK, ImGuiKey_NumLock}, {GLFW_KEY_PRINT_SCREEN, ImGuiKey_PrintScreen},
        {GLFW_KEY_PAUSE, ImGuiKey_Pause}, {GLFW_KEY_KP_0, ImGuiKey_Keypad0},
        {GLFW_KEY_KP_1, ImGuiKey_Keypad1}, {GLFW_KEY_KP_2, ImGuiKey_Keypad2},
        {GLFW_KEY_KP_3, ImGuiKey_Keypad3}, {GLFW_KEY_KP_4, ImGuiKey_Keypad4},
        {GLFW_KEY_KP_5, ImGuiKey_Keypad5}, {GLFW_KEY_KP_6, ImGuiKey_Keypad6},
        {GLFW_KEY_KP_7, ImGuiKey_Keypad7}, {GLFW_KEY_KP_8, ImGuiKey_Keypad8},
        {GLFW_KEY_KP_9, ImGuiKey_Keypad9}, {GLFW_KEY_KP_DECIMAL, ImGuiKey_KeypadDecimal},
        {GLFW_KEY_KP_DIVIDE, ImGuiKey_KeypadDivide}, {GLFW_KEY_KP_MULTIPLY, ImGuiKey_KeypadMultiply},
        {GLFW_KEY_KP_SUBTRACT, ImGuiKey_KeypadSubtract}, {GLFW_KEY_KP_ADD, ImGuiKey_KeypadAdd},
        {GLFW_KEY_KP_ENTER, ImGuiKey_KeypadEnter}, {GLFW_KEY_KP_EQUAL, ImGuiKey_KeypadEqual},
        {GLFW_KEY_LEFT_SHIFT, ImGuiKey_LeftShift}, {GLFW_KEY_LEFT_CONTROL, ImGuiKey_LeftCtrl},
        {GLFW_KEY_LEFT_ALT, ImGuiKey_LeftAlt}, {GLFW_KEY_LEFT_SUPER, ImGuiKey_LeftSuper},
        {GLFW_KEY_RIGHT_SHIFT, ImGuiKey_RightShift}, {GLFW_KEY_RIGHT_CONTROL, ImGuiKey_RightCtrl},
        {GLFW_KEY_RIGHT_ALT, ImGuiKey_RightAlt}, {GLFW_KEY_RIGHT_SUPER, ImGuiKey_RightSuper},
        {GLFW_KEY_MENU, ImGuiKey_Menu},
        {GLFW_KEY_A, ImGuiKey_A}, {GLFW_KEY_B, ImGuiKey_B}, {GLFW_KEY_C, ImGuiKey_C},
        {GLFW_KEY_D, ImGuiKey_D}, {GLFW_KEY_E, ImGuiKey_E}, {GLFW_KEY_F, ImGuiKey_F},
        {GLFW_KEY_G, ImGuiKey_G}, {GLFW_KEY_H, ImGuiKey_H}, {GLFW_KEY_I, ImGuiKey_I},
        {GLFW_KEY_J, ImGuiKey_J}, {GLFW_KEY_K, ImGuiKey_K}, {GLFW_KEY_L, ImGuiKey_L},
        {GLFW_KEY_M, ImGuiKey_M}, {GLFW_KEY_N, ImGuiKey_N}, {GLFW_KEY_O, ImGuiKey_O},
        {GLFW_KEY_P, ImGuiKey_P}, {GLFW_KEY_Q, ImGuiKey_Q}, {GLFW_KEY_R, ImGuiKey_R},
        {GLFW_KEY_S, ImGuiKey_S}, {GLFW_KEY_T, ImGuiKey_T}, {GLFW_KEY_U, ImGuiKey_U},
        {GLFW_KEY_V, ImGuiKey_V}, {GLFW_KEY_W, ImGuiKey_W}, {GLFW_KEY_X, ImGuiKey_X},
        {GLFW_KEY_Y, ImGuiKey_Y}, {GLFW_KEY_Z, ImGuiKey_Z},
        {GLFW_KEY_F1, ImGuiKey_F1}, {GLFW_KEY_F2, ImGuiKey_F2}, {GLFW_KEY_F3, ImGuiKey_F3},
        {GLFW_KEY_F4, ImGuiKey_F4}, {GLFW_KEY_F5, ImGuiKey_F5}, {GLFW_KEY_F6, ImGuiKey_F6},
        {GLFW_KEY_F7, ImGuiKey_F7}, {GLFW_KEY_F8, ImGuiKey_F8}, {GLFW_KEY_F9, ImGuiKey_F9},
        {GLFW_KEY_F10, ImGuiKey_F10}, {GLFW_KEY_F11, ImGuiKey_F11}, {GLFW_KEY_F12, ImGuiKey_F12},
    };
    auto it = m.find(glfwKey);
    return it == m.end() ? ImGuiKey_None : it->second;
}

bool ImGuiInit(GLFWwindow* window, const std::string& fontPath) {
    g_window = window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // 不写 ini
    io.BackendPlatformName = "glfw_custom_gles2";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

    // 字体(支持中文)
    if (!fontPath.empty()) {
        static const ImWchar rangesCJK[] = {
            0x0020, 0x00FF, // ASCII + Latin1
            0x2000, 0x206F, // 常用标点
            0x2100, 0x2BFF, // 数学/几何符号(度量符号)
            0x3000, 0x30FF, // CJK 标点/假名
            0x4E00, 0x9FFF, // CJK 统一表意
            0xFF00, 0xFFEF, // 全角字符
            0,
        };
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f, &cfg, rangesCJK);
    }
    if (io.Fonts->Fonts.empty()) io.Fonts->AddFontDefault();
    io.Fonts->Build();

    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    pGenTextures(1, &g_fontTex);
    pBindTexture(TEXTURE_2D, g_fontTex);
    pTexParameteri(TEXTURE_2D, TEXTURE_MIN_FILTER, LINEAR);
    pTexParameteri(TEXTURE_2D, TEXTURE_MAG_FILTER, LINEAR);
    pPixelStorei(0x0CF5, 1);
    pTexImage2D(TEXTURE_2D, 0, RGBA, w, h, 0, RGBA, UNSIGNED_BYTE, pixels);
    io.Fonts->SetTexID((ImTextureID)(intptr_t)g_fontTex);

    g_shader = compile(kImGuiVS, kImGuiFS);
    pGenBuffers(1, &g_vbo);
    pGenBuffers(1, &g_ibo);
    g_uMVP = pGetUniformLocation(g_shader, "uMVP");
    g_uTex = pGetUniformLocation(g_shader, "uTex");
    g_aPos = pGetAttribLocation(g_shader, "aPos");
    g_aUV = pGetAttribLocation(g_shader, "aUV");
    g_aCol = pGetAttribLocation(g_shader, "aCol");
    return g_shader != 0;
}

void ImGuiShutdown() {
    if (g_fontTex) pDeleteTextures(1, &g_fontTex);
    ImGui::DestroyContext();
}

void ImGuiNewFrame(GLFWwindow* window) {
    ImGuiIO& io = ImGui::GetIO();
    int dw, dh;
    glfwGetWindowSize(window, &dw, &dh);
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    io.DisplaySize = ImVec2((float)dw, (float)dh);
    if (dw > 0 && dh > 0)
        io.DisplayFramebufferScale = ImVec2((float)fbw / dw, (float)fbh / dh);

    double now = glfwGetTime();
    io.DeltaTime = g_lastTime > 0 ? (float)(now - g_lastTime) : 1.0f / 60;
    if (io.DeltaTime <= 0) io.DeltaTime = 1.0f / 60;
    g_lastTime = now;

    // 鼠标
    if (glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        io.AddMousePosEvent((float)mx, (float)my);
    }
    io.AddMouseButtonEvent(0, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    io.AddMouseButtonEvent(1, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    io.AddMouseButtonEvent(2, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    if (g_mouseWheelX || g_mouseWheelY) {
        io.AddMouseWheelEvent(g_mouseWheelX, g_mouseWheelY);
        g_mouseWheelX = g_mouseWheelY = 0;
    }
    // 键盘状态(轮询注入)
    for (int k = GLFW_KEY_SPACE; k < GLFW_KEY_LAST; ++k) {
        ImGuiKey ik = mapKey(k);
        if (ik == ImGuiKey_None) continue;
        int st2 = glfwGetKey(window, k);
        io.AddKeyEvent(ik, st2 == GLFW_PRESS || st2 == GLFW_REPEAT);
    }
    io.AddKeyEvent(ImGuiMod_Ctrl, glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                                      glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
    io.AddKeyEvent(ImGuiMod_Shift, glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                       glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
    io.AddKeyEvent(ImGuiMod_Alt, glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                                     glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);

    ImGui::NewFrame();
}

void ImGuiRender() {
    ImGui::Render();
    ImDrawData* dd = ImGui::GetDrawData();
    if (!dd || dd->TotalVtxCount <= 0) return;

    int fbw, fbh;
    glfwGetFramebufferSize(g_window, &fbw, &fbh);
    float L = dd->DisplayPos.x, T = dd->DisplayPos.y;
    float R = L + dd->DisplaySize.x, B = T + dd->DisplaySize.y;
    float mvp[16] = {
        2.0f / (R - L), 0, 0, 0,
        0, 2.0f / (T - B), 0, 0,
        0, 0, -1, 0,
        (R + L) / (L - R), (T + B) / (B - T), 0, 1};

    pEnable(BLEND);
    pBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
    pDisable(CULL_FACE);
    pDisable(DEPTH_TEST);
    pEnable(SCISSOR_TEST);
    pUseProgram(g_shader);
    pUniformMatrix4fv(g_uMVP, 1, 0, mvp);
    pUniform1i(g_uTex, 0);
    pActiveTexture(TEXTURE0);

    ImDrawVert* vtxBuf = nullptr;
    ImDrawIdx* idxBuf = nullptr;
    for (int n = 0; n < dd->CmdListsCount; ++n) {
        const ImDrawList* cl = dd->CmdLists[n];
        pBindBuffer(ARRAY_BUFFER, g_vbo);
        pBufferData(ARRAY_BUFFER, (GLsizeiptr)cl->VtxBuffer.Size * sizeof(ImDrawVert),
                    cl->VtxBuffer.Data, DYNAMIC_DRAW);
        pBindBuffer(ELEMENT_ARRAY_BUFFER, g_ibo);
        pBufferData(ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cl->IdxBuffer.Size * sizeof(ImDrawIdx),
                    cl->IdxBuffer.Data, DYNAMIC_DRAW);
        pEnableVertexAttribArray(g_aPos);
        pEnableVertexAttribArray(g_aUV);
        pEnableVertexAttribArray(g_aCol);
        // ImDrawVert 布局: pos(f32x2), uv(f32x2), col(u32)
        pVertexAttribPointer(g_aPos, 2, FLOAT, 0, sizeof(ImDrawVert), (void*)IM_OFFSETOF(ImDrawVert, pos));
        pVertexAttribPointer(g_aUV, 2, FLOAT, 0, sizeof(ImDrawVert), (void*)IM_OFFSETOF(ImDrawVert, uv));
        pVertexAttribPointer(g_aCol, 4, UNSIGNED_BYTE, 1, sizeof(ImDrawVert), (void*)IM_OFFSETOF(ImDrawVert, col));
        for (int i = 0; i < cl->CmdBuffer.Size; ++i) {
            const ImDrawCmd* cmd = &cl->CmdBuffer[i];
            if (cmd->UserCallback) {
                cmd->UserCallback(cl, cmd);
                continue;
            }
            ImVec2 pos = dd->DisplayPos;
            pScissor((int)(cmd->ClipRect.x - pos.x), (int)(fbh - cmd->ClipRect.w),
                     (int)(cmd->ClipRect.z - cmd->ClipRect.x), (int)(cmd->ClipRect.w - cmd->ClipRect.y));
            pBindTexture(TEXTURE_2D, (GLuint)(intptr_t)cmd->GetTexID());
            pDrawElements(TRIANGLES, cmd->ElemCount, UNSIGNED_SHORT,
                          (void*)((intptr_t)cmd->IdxOffset * (intptr_t)sizeof(ImDrawIdx)));
        }
        (void)vtxBuf;
        (void)idxBuf;
    }
    pDisableVertexAttribArray(g_aPos);
    pDisableVertexAttribArray(g_aUV);
    pDisableVertexAttribArray(g_aCol);
    pDisable(SCISSOR_TEST);
    pDisable(BLEND);
    pEnable(DEPTH_TEST);
    pEnable(CULL_FACE);
    pBindBuffer(ARRAY_BUFFER, 0);
    pBindBuffer(ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace cad::ui

namespace cad::ui {
void imGuiScroll(float dx, float dy) {
    g_mouseWheelX += dx;
    g_mouseWheelY += dy;
}
void imGuiChar(unsigned int c) {
    ImGui::GetIO().AddInputCharacter(c);
}
} // namespace cad::ui
