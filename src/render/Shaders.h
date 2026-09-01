// Shaders.h —— GLSL 着色器(ES 100 / 桌面 1.10+ 双兼容)
#pragma once

namespace cad::shaders {

// ---- 实体着色: PBR 风格影棚光照(Shapr3D 视口质感) ----
static const char* kBodyVS = R"(
attribute vec3 aPos;
attribute vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;
varying vec3 vNormal;
varying vec3 vWorld;
void main() {
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vNormal = normalize(uNormalMat * aNormal);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kBodyFS = R"(
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif
varying vec3 vNormal;
varying vec3 vWorld;
uniform vec3 uEye;
uniform vec3 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform float uHighlight;      // 选择/悬停高亮 0..1
uniform vec3 uHighlightColor;
uniform float uXray;           // 半透明
// 影棚三点光 + 半球环境
uniform vec3 uKeyDir, uKeyColor;
uniform vec3 uFillDir, uFillColor;
uniform vec3 uRimDir, uRimColor;
uniform vec3 uAmbSky, uAmbGround;

vec3 shade(vec3 N, vec3 V, vec3 L, vec3 lc, vec3 F0, vec3 albedo, float rough, float metal) {
    vec3 H = normalize(L + V);
    float nl = max(dot(N, L), 0.0);
    float nh = max(dot(N, H), 0.0);
    float vh = max(dot(V, H), 0.0);
    // GGX 近似(简化) + Schlick 菲涅尔
    float a = rough * rough;
    float d = nh * nh * (a * a - 1.0) + 1.0;
    float D = a * a / (3.14159 * d * d);
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float gv = 1.0 / (nh * (1.0 - k) + k);   // 简化遮蔽
    float F = F0.r + (1.0 - F0.r) * pow(1.0 - vh, 5.0);
    vec3 spec = vec3(D * F * gv * nl);
    vec3 diff = albedo * nl * (1.0 - metal) / 3.14159;
    return lc * (diff + spec * mix(vec3(1.0), albedo, metal * 0.7));
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uEye - vWorld);
    if (dot(N, V) < 0.0) N = -N; // 双面
    vec3 albedo = uBaseColor;
    vec3 F0 = mix(vec3(0.04), albedo, uMetallic);
    float rough = clamp(uRoughness, 0.03, 1.0);

    vec3 col = mix(uAmbGround, uAmbSky, N.z * 0.5 + 0.5) * albedo * (1.0 - uMetallic * 0.55);
    col += shade(N, V, normalize(uKeyDir), uKeyColor, F0, albedo, rough, uMetallic);
    col += shade(N, V, normalize(uFillDir), uFillColor, F0, albedo, rough, uMetallic);
    // 轮廓缘光
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    col += uRimColor * rim * 0.25 * mix(vec3(1.0), albedo, 0.5);

    // 高亮染色
    col = mix(col, uHighlightColor, uHighlight * 0.45);

    // 简易色调映射 + gamma
    col = col / (col + vec3(1.0));
    col = pow(col, vec3(1.0 / 2.2));
    gl_FragColor = vec4(col, uXray > 0.5 ? 0.45 : 1.0);
}
)";

// ---- 线条(棱线/草图/网格线/标注) ----
static const char* kLineVS = R"(
attribute vec3 aPos;
attribute vec4 aColor;
uniform mat4 uMVP;
varying vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kLineFS = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec4 vColor;
void main() { gl_FragColor = vColor; }
)";

// ---- 地面阴影(投影贴图) ----
static const char* kGroundVS = R"(
attribute vec3 aPos;
attribute vec2 aUV;
uniform mat4 uMVP;
varying vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kGroundFS = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec3 uShadowColor;
uniform float uStrength;
uniform int uUseTex;
void main() {
    float a = uUseTex == 1 ? texture2D(uTex, vUV).a : 0.35;
    a = clamp(a, 0.0, 1.0) * uStrength;
    gl_FragColor = vec4(uShadowColor, a);
}
)";

// ---- 模糊(阴影柔化) ----
static const char* kBlurVS = R"(
attribute vec3 aPos;
attribute vec2 aUV;
uniform mat4 uMVP;
varying vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kBlurFS = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDir; // (1/w, 0) 或 (0, 1/h)
void main() {
    float a = 0.0;
    a += texture2D(uTex, vUV - uDir * 4.0).a * 0.05;
    a += texture2D(uTex, vUV - uDir * 3.0).a * 0.09;
    a += texture2D(uTex, vUV - uDir * 2.0).a * 0.12;
    a += texture2D(uTex, vUV - uDir * 1.0).a * 0.15;
    a += texture2D(uTex, vUV).a * 0.18;
    a += texture2D(uTex, vUV + uDir * 1.0).a * 0.15;
    a += texture2D(uTex, vUV + uDir * 2.0).a * 0.12;
    a += texture2D(uTex, vUV + uDir * 3.0).a * 0.09;
    a += texture2D(uTex, vUV + uDir * 4.0).a * 0.05;
    gl_FragColor = vec4(0.0, 0.0, 0.0, a);
}
)";

} // namespace cad::shaders
