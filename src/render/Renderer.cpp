// Renderer.cpp —— GLES2 渲染管线实现
#include "Renderer.h"
#include "GLApi.h"
#include "Shaders.h"
#include "PngWriter.h"
#include <cstring>
#include <cstdio>

namespace cad {
using namespace gl;

// ---------------- 着色器编译 ----------------
GLuint Renderer::compile(const char* vs, const char* fs) {
    GLint ok = 0;
    GLuint v = pCreateShader(gl::VERTEX_SHADER);
    pShaderSource(v, 1, &vs, nullptr);
    pCompileShader(v);
    pGetShaderiv(v, gl::COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {0};
        pGetShaderInfoLog(v, 1023, nullptr, log);
        LOGE("顶点着色器编译失败: %s", log);
        return 0;
    }
    GLuint f = pCreateShader(gl::FRAGMENT_SHADER);
    pShaderSource(f, 1, &fs, nullptr);
    pCompileShader(f);
    pGetShaderiv(f, gl::COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {0};
        pGetShaderInfoLog(f, 1023, nullptr, log);
        LOGE("片段着色器编译失败: %s", log);
        return 0;
    }
    GLuint p = pCreateProgram();
    pAttachShader(p, v);
    pAttachShader(p, f);
    pLinkProgram(p);
    pGetProgramiv(p, gl::LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = {0};
        pGetProgramInfoLog(p, 1023, nullptr, log);
        LOGE("程序链接失败: %s", log);
        return 0;
    }
    pDeleteShader(v);
    pDeleteShader(f);
    return p;
}

bool Renderer::init() {
    body_.id = compile(shaders::kBodyVS, shaders::kBodyFS);
    line_.id = compile(shaders::kLineVS, shaders::kLineFS);
    ground_.id = compile(shaders::kGroundVS, shaders::kGroundFS);
    blur_.id = compile(shaders::kBlurVS, shaders::kBlurFS);
    if (!body_.id || !line_.id || !ground_.id || !blur_.id) return false;

    pGenBuffers(1, &lineVbo_);
    pGenBuffers(1, &gridVbo_);
    pGenBuffers(1, &axesVbo_);
    pGenBuffers(1, &quadVbo_);
    if (gl::hasFBO) ensureShadowResources();
    return true;
}

void Renderer::ensureShadowResources() {
    pGenFramebuffers(1, &shadowFbo_);
    pGenTextures(1, &shadowTexA_);
    pGenTextures(1, &shadowTexB_);
    for (GLuint t : {shadowTexA_, shadowTexB_}) {
        pBindTexture(gl::TEXTURE_2D, t);
        pTexImage2D(gl::TEXTURE_2D, 0, gl::RGBA, shadowSize_, shadowSize_, 0, gl::RGBA, gl::UNSIGNED_BYTE, nullptr);
        pTexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR);
        pTexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR);
        pTexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE);
        pTexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE);
    }
    pBindFramebuffer(gl::FRAMEBUFFER, shadowFbo_);
    pBindTexture(gl::TEXTURE_2D, 0);
    pBindFramebuffer(gl::FRAMEBUFFER, 0);
}

// ---------------- 网格线 ----------------
void Renderer::buildGrid(float half, float step, float major) {
    lines_.clear();
    int n = (int)(half / step);
    for (int i = -n; i <= n; ++i) {
        float x = i * step;
        bool isMajor = (i % (int)(major / step) == 0);
        float a = isMajor ? 0.30f : 0.13f;
        Vec3 c{0.55f, 0.58f, 0.64f};
        if (isMajor) c = {0.45f, 0.49f, 0.56f};
        lines_.push_back({x, 0, -half, c.x, c.y, c.z, a});
        lines_.push_back({x, 0, half, c.x, c.y, c.z, a});
        lines_.push_back({-half, 0, x, c.x, c.y, c.z, a});
        lines_.push_back({half, 0, x, c.x, c.y, c.z, a});
    }
    pBindBuffer(gl::ARRAY_BUFFER, gridVbo_);
    pBufferData(gl::ARRAY_BUFFER, (GLsizeiptr)(lines_.size() * sizeof(LineVert)), lines_.data(), gl::STATIC_DRAW);
    gridVertCount_ = (int)lines_.size();
    lines_.clear();
}

void Renderer::buildAxes(float len) {
    lines_.clear();
    Vec3 ox{0.85f, 0.28f, 0.25f}, oy{0.30f, 0.65f, 0.30f}, oz{0.28f, 0.45f, 0.90f};
    lines_.push_back({0, 0.01f, 0, ox.x, ox.y, ox.z, 1});
    lines_.push_back({len, 0.01f, 0, ox.x, ox.y, ox.z, 1});
    lines_.push_back({0, 0.01f, 0, oy.x, oy.y, oy.z, 1});
    lines_.push_back({0, len, 0, oy.x, oy.y, oy.z, 1});
    lines_.push_back({0, 0.01f, 0, oz.x, oz.y, oz.z, 1});
    lines_.push_back({0, 0.01f, len, oz.x, oz.y, oz.z, 1});
    pBindBuffer(gl::ARRAY_BUFFER, axesVbo_);
    pBufferData(gl::ARRAY_BUFFER, (GLsizeiptr)(lines_.size() * sizeof(LineVert)), lines_.data(), gl::STATIC_DRAW);
    lines_.clear();
}

// ---------------- 帧管线 ----------------
void Renderer::beginFrame(const Camera& cam, const RenderSettings& st) {
    cam_ = cam;
    st_ = st;
    float aspect = (float)width_ / (float)std::max(1, height_);
    Mat4 v = cam.view();
    Mat4 p = cam.proj(aspect);
    vp_ = p * v;
    eye_ = cam.eye();

    pViewport(0, 0, width_, height_);
    pClearColor(st.bgColor.x, st.bgColor.y, st.bgColor.z, 1);
    // 底部稍暗的渐变由 ImGui 叠加, 这里纯色
    pClear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);
    pEnable(gl::DEPTH_TEST);
    pDepthFunc(gl::LEQUAL);
    pEnable(gl::CULL_FACE);
    pCullFace(gl::BACK);
    pDisable(gl::BLEND);

    if (st.showGrid) {
        if (gridVertCount_ == 0) buildGrid(500, 10, 50);
        pUseProgram(line_.id);
        pUniformMatrix4fv(line_.u("uMVP"), 1, 0, vp_.m);
        GLint aPos = line_.a("aPos"), aCol = line_.a("aColor");
        pBindBuffer(gl::ARRAY_BUFFER, gridVbo_);
        pEnableVertexAttribArray(aPos);
        pEnableVertexAttribArray(aCol);
        pVertexAttribPointer(aPos, 3, gl::FLOAT, 0, sizeof(LineVert), (void*)0);
        pVertexAttribPointer(aCol, 4, gl::FLOAT, 0, sizeof(LineVert), (void*)(3 * sizeof(float)));
        pEnable(gl::BLEND);
        pBlendFunc(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);
        pDepthMask(0);
        pDrawArrays(gl::LINES, 0, gridVertCount_);
        pDepthMask(1);
        pDisableVertexAttribArray(aPos);
        pDisableVertexAttribArray(aCol);
        pDisable(gl::BLEND);
    }
    lines_.clear();
}

// 地面接触阴影
void Renderer::drawShadow(const Document& doc) {
    if (!st_.showShadow || !gl::hasFBO) return;
    // 模型贴合地面: 阴影半径按包围盒
    Bnd_Box bb;
    if (!doc.sceneBounds(bb)) return;
    double x0, y0, z0, x1, y1, z1;
    bb.Get(x0, y0, z0, x1, y1, z1);
    shadowRadius_ = (float)std::max(50.0, std::max(x1 - x0, y1 - y0) * 1.4);
    shadowDirty_ = true;
    if (shadowDirty_) renderShadowAlpha(doc, cam_);
}

void Renderer::renderShadowAlpha(const Document& doc, const Camera& cam) {
    // 正交俯视渲染 alpha, 模型压平到 y=0 平面(直接把顶点 y 置 0: 用 body 着色器输出 alpha=1 的黑色)
    int prevW = width_, prevH = height_;
    float r = shadowRadius_;
    Mat4 ortho = Mat4::ortho(-r, r, -r, r, -2000, 2000);
    Mat4 v = Mat4::lookAt({0, 800, 0}, {0, 0, 0}, {0, 0, -1});
    Mat4 vp = ortho * v;

    pBindFramebuffer(gl::FRAMEBUFFER, shadowFbo_);
    pFramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, shadowTexA_, 0);
    if (pCheckFramebufferStatus(gl::FRAMEBUFFER) != gl::FRAMEBUFFER_COMPLETE) {
        pBindFramebuffer(gl::FRAMEBUFFER, 0);
        return;
    }
    pViewport(0, 0, shadowSize_, shadowSize_);
    pClearColor(0, 0, 0, 0);
    pClear(gl::COLOR_BUFFER_BIT);
    pDisable(gl::DEPTH_TEST);
    pDisable(gl::CULL_FACE);
    pEnable(gl::BLEND);
    pBlendFunc(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);

    pUseProgram(body_.id);
    Mat4 model = Mat4::identity();
    Mat4 flat = model;
    flat.m[5] = 0; // y -> 0 (压平)
    Mat4 mvp = vp * flat;
    float nrm[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    pUniformMatrix4fv(body_.u("uMVP"), 1, 0, mvp.m);
    pUniformMatrix4fv(body_.u("uModel"), 1, 0, flat.m);
    pUniformMatrix3fv(body_.u("uNormalMat"), 1, 0, nrm);
    pUniform3f(body_.u("uEye"), 0, 800, 0);
    pUniform3f(body_.u("uBaseColor"), 1, 1, 1);
    pUniform1f(body_.u("uMetallic"), 0);
    pUniform1f(body_.u("uRoughness"), 1);
    pUniform1f(body_.u("uHighlight"), 0);
    pUniform3f(body_.u("uHighlightColor"), 0, 0, 0);
    pUniform1f(body_.u("uXray"), 0);
    // 让光照全黑: 直接把关键光颜色清零
    pUniform3f(body_.u("uKeyDir"), 0, 1, 0);
    pUniform3f(body_.u("uKeyColor"), 0, 0, 0);
    pUniform3f(body_.u("uFillDir"), 0, 1, 0);
    pUniform3f(body_.u("uFillColor"), 0, 0, 0);
    pUniform3f(body_.u("uRimDir"), 0, 1, 0);
    pUniform3f(body_.u("uRimColor"), 0, 0, 0);
    pUniform3f(body_.u("uAmbSky"), 0, 0, 0);
    pUniform3f(body_.u("uAmbGround"), 0, 0, 0);
    // 输出 alpha=1: 修改片元? 无专门着色器 -> 用 alpha 混合: 色为黑, alpha 由混合目标=1
    // 简化: 直接把颜色当 alpha 用不可行, 这里将 clear 后背景 alpha=0, 绘制时 gl_FragColor.a=1
    GLint aPos = body_.a("aPos"), aN = body_.a("aNormal");
        for (auto& b : doc.bodies) {
            const MeshData* md = doc.mesh(b.id);
            if (!md || md->verts.empty() || !b.visible) continue;
        pBindBuffer(gl::ARRAY_BUFFER, 0);
        // 上传临时(可缓存)
        static thread_local GLuint tmpVbo = 0;
        if (!tmpVbo) pGenBuffers(1, &tmpVbo);
        pBindBuffer(gl::ARRAY_BUFFER, tmpVbo);
        pBufferData(gl::ARRAY_BUFFER, (GLsizeiptr)(md->verts.size() * sizeof(float)), md->verts.data(), gl::DYNAMIC_DRAW);
        pEnableVertexAttribArray(aPos);
        pVertexAttribPointer(aPos, 3, gl::FLOAT, 0, 0, 0);
        pDisableVertexAttribArray(aN);
        pDrawArrays(gl::TRIANGLES, 0, (GLsizei)(md->verts.size() / 3));
        pDisableVertexAttribArray(aPos);
    }

    // 两趟高斯模糊 A->B->A
    struct QuadVert { float x, y, z, u, v; };
    QuadVert quad[4] = {{-1, -1, 0, 0, 0}, {1, -1, 0, 1, 0}, {1, 1, 0, 1, 1}, {-1, 1, 0, 0, 1}};
    GLushort idx[6] = {0, 1, 2, 0, 2, 3};
    pBindBuffer(gl::ARRAY_BUFFER, quadVbo_);
    pBufferData(gl::ARRAY_BUFFER, sizeof(quad), quad, gl::DYNAMIC_DRAW);
    static thread_local GLuint tmpIbo = 0;
    if (!tmpIbo) pGenBuffers(1, &tmpIbo);
    pBindBuffer(gl::ELEMENT_ARRAY_BUFFER, tmpIbo);
    pBufferData(gl::ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, gl::DYNAMIC_DRAW);

    Mat4 idm = Mat4::identity();
    pUseProgram(blur_.id);
    pUniformMatrix4fv(blur_.u("uMVP"), 1, 0, idm.m);
    GLint qp = blur_.a("aPos"), quv = blur_.a("aUV");
    pEnableVertexAttribArray(qp);
    pEnableVertexAttribArray(quv);
    pVertexAttribPointer(qp, 3, gl::FLOAT, 0, sizeof(QuadVert), 0);
    pVertexAttribPointer(quv, 2, gl::FLOAT, 0, sizeof(QuadVert), (void*)(3 * sizeof(float)));
    float texel = 1.0f / shadowSize_;
    // 横向 A->B
    pFramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, shadowTexB_, 0);
    pActiveTexture(gl::TEXTURE0);
    pBindTexture(gl::TEXTURE_2D, shadowTexA_);
    pUniform1i(blur_.u("uTex"), 0);
    pUniform2f(blur_.u("uDir"), texel, 0);
    pDrawElements(gl::TRIANGLES, 6, gl::UNSIGNED_SHORT, 0);
    // 纵向 B->A
    pFramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, shadowTexA_, 0);
    pBindTexture(gl::TEXTURE_2D, shadowTexB_);
    pUniform2f(blur_.u("uDir"), 0, texel);
    pDrawElements(gl::TRIANGLES, 6, gl::UNSIGNED_SHORT, 0);

    pDisableVertexAttribArray(qp);
    pDisableVertexAttribArray(quv);
    pBindFramebuffer(gl::FRAMEBUFFER, 0);
    pDisable(gl::BLEND);
    pEnable(gl::DEPTH_TEST);
    pEnable(gl::CULL_FACE);
    pViewport(0, 0, prevW, prevH);
    shadowDirty_ = false;
}

void Renderer::drawGroundQuad(const Camera& cam, float radius) {
    if (!st_.showShadow) return;
    bool useTex = gl::hasFBO && shadowTexA_ != 0;
    // 阴影贴图投影到地面
    struct QuadVert { float x, y, z, u, v; };
    float r = radius;
    QuadVert quad[4] = {{-r, 0, -r, 0, 0}, {r, 0, -r, 1, 0}, {r, 0, r, 1, 1}, {-r, 0, r, 0, 1}};
    GLushort idx[6] = {0, 1, 2, 0, 2, 3};
    pBindBuffer(gl::ARRAY_BUFFER, quadVbo_);
    pBufferData(gl::ARRAY_BUFFER, sizeof(quad), quad, gl::DYNAMIC_DRAW);
    static thread_local GLuint tmpIbo = 0;
    if (!tmpIbo) pGenBuffers(1, &tmpIbo);
    pBindBuffer(gl::ELEMENT_ARRAY_BUFFER, tmpIbo);
    pBufferData(gl::ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, gl::DYNAMIC_DRAW);

    pUseProgram(ground_.id);
    pUniformMatrix4fv(ground_.u("uMVP"), 1, 0, vp_.m);
    GLint qp = ground_.a("aPos"), quv = ground_.a("aUV");
    pEnableVertexAttribArray(qp);
    pEnableVertexAttribArray(quv);
    pVertexAttribPointer(qp, 3, gl::FLOAT, 0, sizeof(QuadVert), 0);
    pVertexAttribPointer(quv, 2, gl::FLOAT, 0, sizeof(QuadVert), (void*)(3 * sizeof(float)));
    pActiveTexture(gl::TEXTURE0);
    pBindTexture(gl::TEXTURE_2D, useTex ? shadowTexA_ : 0);
    pUniform1i(ground_.u("uTex"), 0);
    pUniform3f(ground_.u("uShadowColor"), st_.shadowColor.x, st_.shadowColor.y, st_.shadowColor.z);
    pUniform1f(ground_.u("uStrength"), st_.shadowStrength);
    pUniform1i(ground_.u("uUseTex"), useTex ? 1 : 0);
    pEnable(gl::BLEND);
    pBlendFunc(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);
    pDisable(gl::CULL_FACE);
    pDrawElements(gl::TRIANGLES, 6, gl::UNSIGNED_SHORT, 0);
    pEnable(gl::CULL_FACE);
    pDisable(gl::BLEND);
    pDisableVertexAttribArray(qp);
    pDisableVertexAttribArray(quv);
}

// ---------------- 实体 ----------------
void Renderer::drawBody(Id, const MeshData& mesh, const Material& mat, Highlight hl) {
    if (mesh.verts.empty()) return;
    pUseProgram(body_.id);
    Mat4 model = Mat4::identity();
    // 法线矩阵 = 单位(无缩放)
    float nrm[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    pUniformMatrix4fv(body_.u("uMVP"), 1, 0, vp_.m);
    pUniformMatrix4fv(body_.u("uModel"), 1, 0, model.m);
    pUniformMatrix3fv(body_.u("uNormalMat"), 1, 0, nrm);
    pUniform3f(body_.u("uEye"), eye_.x, eye_.y, eye_.z);
    pUniform3f(body_.u("uBaseColor"), mat.color[0], mat.color[1], mat.color[2]);
    pUniform1f(body_.u("uMetallic"), mat.metallic);
    pUniform1f(body_.u("uRoughness"), mat.roughness);
    float hlAmt = hl == Highlight::Selected ? 0.85f : hl == Highlight::Hover ? 0.35f : 0.f;
    pUniform1f(body_.u("uHighlight"), hlAmt);
    Vec3 hc = hl == Highlight::Selected ? Vec3{0.15f, 0.45f, 0.95f} : Vec3{0.4f, 0.6f, 0.9f};
    pUniform3f(body_.u("uHighlightColor"), hc.x, hc.y, hc.z);
    pUniform1f(body_.u("uXray"), 0);
    // 影棚三点光(世界空间)
    pUniform3f(body_.u("uKeyDir"), 0.45f, 0.35f, 0.82f);
    pUniform3f(body_.u("uKeyColor"), 1.05f, 1.02f, 0.96f);
    pUniform3f(body_.u("uFillDir"), -0.7f, -0.2f, 0.4f);
    pUniform3f(body_.u("uFillColor"), 0.30f, 0.33f, 0.40f);
    pUniform3f(body_.u("uRimDir"), -0.2f, -0.6f, -0.75f);
    pUniform3f(body_.u("uRimColor"), 0.55f, 0.62f, 0.75f);
    pUniform3f(body_.u("uAmbSky"), 0.42f, 0.46f, 0.52f);
    pUniform3f(body_.u("uAmbGround"), 0.20f, 0.19f, 0.18f);

    GLint aPos = body_.a("aPos"), aN = body_.a("aNormal");
    pBindBuffer(gl::ARRAY_BUFFER, 0);
    static thread_local GLuint vbo = 0, nbo = 0;
    if (!vbo) pGenBuffers(1, &vbo);
    if (!nbo) pGenBuffers(1, &nbo);
    pBindBuffer(gl::ARRAY_BUFFER, vbo);
    pBufferData(gl::ARRAY_BUFFER, (GLsizeiptr)(mesh.verts.size() * sizeof(float)), mesh.verts.data(), gl::DYNAMIC_DRAW);
    pEnableVertexAttribArray(aPos);
    pVertexAttribPointer(aPos, 3, gl::FLOAT, 0, 0, 0);
    pBindBuffer(gl::ARRAY_BUFFER, nbo);
    pBufferData(gl::ARRAY_BUFFER, (GLsizeiptr)(mesh.normals.size() * sizeof(float)), mesh.normals.data(), gl::DYNAMIC_DRAW);
    pEnableVertexAttribArray(aN);
    pVertexAttribPointer(aN, 3, gl::FLOAT, 0, 0, 0);
    pDrawArrays(gl::TRIANGLES, 0, (GLsizei)(mesh.verts.size() / 3));
    pDisableVertexAttribArray(aPos);
    pDisableVertexAttribArray(aN);
}

void Renderer::drawEdges(const MeshData& mesh, Vec3 color, float alpha, const std::vector<uint32_t>*) {
    if (mesh.edgeLines.empty()) return;
    lines_.clear();
    for (auto& e : mesh.edgeLines) {
        for (size_t i = 1; i < e.size() / 3; ++i) {
            const float* a = &e[(i - 1) * 3];
            const float* b = &e[i * 3];
            lines_.push_back({a[0], a[1], a[2], color.x, color.y, color.z, alpha});
            lines_.push_back({b[0], b[1], b[2], color.x, color.y, color.z, alpha});
        }
    }
    flushLines(cam_);
}

void Renderer::drawGrid(float, float, float) {} // 网格已在 beginFrame 绘制

void Renderer::drawAxes(float len) {
    (void)len;
    if (!st_.showAxes) return;
    pUseProgram(line_.id);
    pUniformMatrix4fv(line_.u("uMVP"), 1, 0, vp_.m);
    GLint aPos = line_.a("aPos"), aCol = line_.a("aColor");
    pBindBuffer(gl::ARRAY_BUFFER, axesVbo_);
    pEnableVertexAttribArray(aPos);
    pEnableVertexAttribArray(aCol);
    pVertexAttribPointer(aPos, 3, gl::FLOAT, 0, sizeof(LineVert), (void*)0);
    pVertexAttribPointer(aCol, 4, gl::FLOAT, 0, sizeof(LineVert), (void*)(3 * sizeof(float)));
    pDrawArrays(gl::LINES, 0, 6);
    pDisableVertexAttribArray(aPos);
    pDisableVertexAttribArray(aCol);
}

void Renderer::drawPolylines3D(const std::vector<std::vector<Vec3>>& ls, Vec4 color) {
    for (auto& l : ls) {
        for (size_t i = 1; i < l.size(); ++i) {
            lines_.push_back({l[i - 1].x, l[i - 1].y, l[i - 1].z, color.x, color.y, color.z, color.w});
            lines_.push_back({l[i].x, l[i].y, l[i].z, color.x, color.y, color.z, color.w});
        }
        if (l.size() == 1) {
            lines_.push_back({l[0].x, l[0].y, l[0].z, color.x, color.y, color.z, color.w});
            lines_.push_back({l[0].x, l[0].y, l[0].z, color.x, color.y, color.z, color.w});
        }
    }
}

void Renderer::flushLines(const Camera&) {
    if (lines_.empty()) return;
    pUseProgram(line_.id);
    pUniformMatrix4fv(line_.u("uMVP"), 1, 0, vp_.m);
    GLint aPos = line_.a("aPos"), aCol = line_.a("aColor");
    pBindBuffer(gl::ARRAY_BUFFER, lineVbo_);
    pBufferData(gl::ARRAY_BUFFER, (GLsizeiptr)(lines_.size() * sizeof(LineVert)), lines_.data(), gl::DYNAMIC_DRAW);
    pEnableVertexAttribArray(aPos);
    pEnableVertexAttribArray(aCol);
    pVertexAttribPointer(aPos, 3, gl::FLOAT, 0, sizeof(LineVert), (void*)0);
    pVertexAttribPointer(aCol, 4, gl::FLOAT, 0, sizeof(LineVert), (void*)(3 * sizeof(float)));
    pEnable(gl::BLEND);
    pBlendFunc(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);
    pDrawArrays(gl::LINES, 0, (GLsizei)lines_.size());
    pDisable(gl::BLEND);
    pDisableVertexAttribArray(aPos);
    pDisableVertexAttribArray(aCol);
    lines_.clear();
}

// ---------------- 草图 ----------------
void Renderer::drawSketch(const Document& doc, Id activeSketchId,
                          const std::vector<std::pair<Vec3, Vec3>>& rubberLines,
                          const std::vector<Vec3>& snapPoints) {
    Vec3 activeColor{0.10f, 0.42f, 0.95f};
    Vec3 normalColor{0.16f, 0.20f, 0.28f};
    for (auto& sk : doc.sketches) {
        if (!sk.visible) continue;
        auto edges = sk.buildEdges3D();
        for (auto& e : edges) {
            Vec3 c = (sk.id == activeSketchId) ? activeColor : normalColor;
            float a = (sk.id == activeSketchId) ? 1.0f : 0.35f;
            if (e.construction) {
                c = {0.5f, 0.35f, 0.1f};
                a = 0.5f;
            }
            for (size_t i = 1; i < e.pts.size(); ++i) {
                const gp_Pnt& A = e.pts[i - 1];
                const gp_Pnt& B = e.pts[i];
                lines_.push_back({(float)A.X(), (float)A.Y(), (float)A.Z(), c.x, c.y, c.z, a});
                lines_.push_back({(float)B.X(), (float)B.Y(), (float)B.Z(), c.x, c.y, c.z, a});
            }
        }
    }
    for (auto& rb : rubberLines) {
        lines_.push_back({rb.first.x, rb.first.y, rb.first.z, 0.95f, 0.45f, 0.1f, 1});
        lines_.push_back({rb.second.x, rb.second.y, rb.second.z, 0.95f, 0.45f, 0.1f, 1});
    }
    for (auto& sp : snapPoints) {
        float s = 2.5f;
        lines_.push_back({sp.x - s, sp.y, sp.z, 0.95f, 0.30f, 0.05f, 1});
        lines_.push_back({sp.x + s, sp.y, sp.z, 0.95f, 0.30f, 0.05f, 1});
        lines_.push_back({sp.x, sp.y, sp.z - s, 0.95f, 0.30f, 0.05f, 1});
        lines_.push_back({sp.x, sp.y, sp.z + s, 0.95f, 0.30f, 0.05f, 1});
        lines_.push_back({sp.x, sp.y - s, sp.z, 0.95f, 0.30f, 0.05f, 1});
        lines_.push_back({sp.x, sp.y + s, sp.z, 0.95f, 0.30f, 0.05f, 1});
    }
    flushLines(cam_);
}

void Renderer::endFrame() {
    // 绘制帧内累积的杂项线条(测量线等)
    flushLines(cam_);
}

// ---------------- 投影/射线 ----------------
Vec3 Renderer::project(const Vec3& p, const Camera& cam, float w, float h) const {
    float aspect = w / std::max(1.f, h);
    Mat4 m = cam.proj(aspect) * cam.view();
    float x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
    float y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
    float cw = m.m[3] * p.x + m.m[7] * p.y + m.m[11] * p.z + m.m[15];
    if (cw <= 1e-6f) return {-1e6, -1e6, 0};
    return {(x / cw * 0.5f + 0.5f) * w, (1 - (y / cw * 0.5f + 0.5f)) * h, 0};
}

void Renderer::rayAt(float sx, float sy, const Camera& cam, Vec3& origin, Vec3& dir, float w, float h) const {
    float aspect = w / std::max(1.f, h);
    float ndcX = sx / w * 2 - 1;
    float ndcY = 1 - sy / h * 2;
    Vec3 f = cam.viewDir();
    Vec3 r = f.cross(Vec3{0, 0, 1}).normalized();
    Vec3 u = r.cross(f);
    if (cam.ortho) {
        origin = cam.eye() + r * (ndcX * cam.dist * aspect / 2.2f) + u * (ndcY * cam.dist / 2.2f);
        dir = f;
    } else {
        float tanF = std::tan(cam.fov / 2);
        origin = cam.eye();
        dir = (f + r * (ndcX * tanF * aspect) + u * (ndcY * tanF)).normalized();
    }
}

bool rayTriangle(const Vec3& o, const Vec3& d, const Vec3& a, const Vec3& b, const Vec3& c, float& tOut) {
    Vec3 e1 = b - a, e2 = c - a;
    Vec3 p = d.cross(e2);
    float det = e1.dot(p);
    if (std::fabs(det) < 1e-12f) return false;
    float inv = 1 / det;
    Vec3 t = o - a;
    float u = t.dot(p) * inv;
    if (u < 0 || u > 1) return false;
    Vec3 q = t.cross(e1);
    float v = d.dot(q) * inv;
    if (v < 0 || u + v > 1) return false;
    float tt = e2.dot(q) * inv;
    if (tt > 1e-4f) {
        tOut = tt;
        return true;
    }
    return false;
}

// ---------------- 截图 ----------------
bool Renderer::screenshot(const std::string& path, int) {
    GLint vp[4];
    pGetIntegerv(gl::VIEWPORT, vp);
    int w = vp[2], h = vp[3];
    std::vector<uint8_t> px((size_t)w * h * 4);
    pPixelStorei(0x0CF5 /*GL_PACK_ALIGNMENT*/, 1);
    pReadPixels(0, 0, w, h, gl::RGBA, gl::UNSIGNED_BYTE, px.data());
    // 上下翻转
    std::vector<uint8_t> flipped((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        memcpy(&flipped[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
    return writePngRGBA(path, w, h, flipped.data());
}

} // namespace cad
