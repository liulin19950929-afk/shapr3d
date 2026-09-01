// Renderer.h —— OpenGL ES 2.0 渲染器(影棚渲染风格)
#pragma once
#include <unordered_map>
#include "../core/Common.h"
#include "Math3D.h"
#include "GLApi.h"
#include "../kernel/Document.h"

struct GLFWwindow;

namespace cad {
using gl::GLint;
using gl::GLuint;
using gl::GLenum;

enum class ViewMode { Shaded, ShadedEdges, Wireframe };
enum class Highlight { None, Hover, Selected };

struct RenderSettings {
    ViewMode mode = ViewMode::ShadedEdges;
    bool showGrid = true;
    bool showAxes = true;
    bool showShadow = true;
    bool showSketches = true;
    float shadowStrength = 0.42f;
    Vec3 bgColor{0.93f, 0.94f, 0.96f};   // 亮色影棚背景
    Vec3 shadowColor{0.25f, 0.28f, 0.34f};
};

class Renderer {
public:
    bool init();
    void resize(int w, int h) { width_ = w; height_ = h; }

    // ---- 帧管线 ----
    void beginFrame(const Camera& cam, const RenderSettings& st);
    void drawShadow(const Document& doc);                       // 阴影 pass(在地面上)
    void drawGroundQuad(const Camera& cam, float radius);
    void drawBody(Id bodyId, const MeshData& mesh, const Material& mat,
                  Highlight hl = Highlight::None);
    void drawEdges(const MeshData& mesh, Vec3 color, float alpha = 1.0f,
                   const std::vector<uint32_t>* onlyFace = nullptr);
    void drawGrid(float half = 500, float step = 10, float major = 50);
    void drawAxes(float len = 60);
    void drawPolylines3D(const std::vector<std::vector<Vec3>>& lines, Vec4 color);
    void endFrame();

    // 草图绘制(世界坐标线 + 点)
    void drawSketch(const class Document& doc, Id activeSketchId,
                    const std::vector<std::pair<Vec3, Vec3>>& rubberLines,
                    const std::vector<Vec3>& snapPoints);

    // 屏幕投影
    Vec3 project(const Vec3& p, const Camera& cam, float w, float h) const;
    // 屏幕点 -> 射线
    void rayAt(float sx, float sy, const Camera& cam, Vec3& origin, Vec3& dir, float w, float h) const;

    bool screenshot(const std::string& path, int scale2x = 1);

    int width() const { return width_; }
    int height() const { return height_; }
    void invalidateShadow() { shadowDirty_ = true; }

private:
    // 着色器程序
    struct Program {
        GLuint id = 0;
        std::unordered_map<std::string, GLint> uniforms;
        std::unordered_map<std::string, GLint> attrs;
        GLint u(const char* n) {
            auto it = uniforms.find(n);
            if (it == uniforms.end()) return uniforms[n] = gl::pGetUniformLocation(id, n);
            return it->second;
        }
        GLint a(const char* n) {
            auto it = attrs.find(n);
            if (it == attrs.end()) return attrs[n] = gl::pGetAttribLocation(id, n);
            return it->second;
        }
    };
    GLuint compile(const char* vs, const char* fs);
    Program body_, line_, ground_, blur_;

    // 动态线条缓冲(帧内累积, 一次绘制)
    struct LineVert { float x, y, z, r, g, b, a; };
    std::vector<LineVert> lines_;
    GLuint lineVbo_ = 0;
    void flushLines(const Camera& cam);

    // 阴影 FBO
    GLuint shadowFbo_ = 0, shadowTexA_ = 0, shadowTexB_ = 0, quadVbo_ = 0;
    int shadowSize_ = 512;
    bool shadowDirty_ = true;
    float shadowRadius_ = 300; // 世界半径
    void ensureShadowResources();
    void renderShadowAlpha(const Document& doc, const Camera& cam);

    int width_ = 1280, height_ = 800;
    RenderSettings st_;
    Camera cam_;
    Mat4 vp_;
    Vec3 eye_;

    // 地面/网格缓冲
    GLuint gridVbo_ = 0;
    int gridVertCount_ = 0;
    void buildGrid(float half, float step, float major);
    GLuint axesVbo_ = 0;
    void buildAxes(float len);
};

// 拾取辅助: 射线 vs 三角形
bool rayTriangle(const Vec3& o, const Vec3& d, const Vec3& a, const Vec3& b, const Vec3& c, float& tOut);

} // namespace cad
