// GLApi.h —— 零依赖 OpenGL ES 2.0 / 桌面 GL 2.x 动态加载器
// 不需要任何系统 GL 头文件; 通过窗口系统过程地址加载(glfwGetProcAddress)。
#pragma once
#include <cstddef>

namespace gl {

using GLint = int;
using GLsizei = int;
using GLuint = unsigned int;
using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLboolean = unsigned char;
using GLchar = char;
using GLubyte = unsigned char;
using GLfloat = float;
using GLclampf = float;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

// ---- 常量(ES2/桌面 通用) ----
constexpr GLenum FLOAT = 0x1406;
constexpr GLenum UNSIGNED_BYTE = 0x1401;
constexpr GLenum UNSIGNED_SHORT = 0x1403;
constexpr GLenum TRIANGLES = 0x0004;
constexpr GLenum TRIANGLE_STRIP = 0x0005;
constexpr GLenum TRIANGLE_FAN = 0x0006;
constexpr GLenum LINES = 0x0001;
constexpr GLenum LINE_STRIP = 0x0003;
constexpr GLenum POINTS = 0x0000;
constexpr GLenum COLOR_BUFFER_BIT = 0x4000;
constexpr GLenum DEPTH_BUFFER_BIT = 0x0100;
constexpr GLenum STENCIL_BUFFER_BIT = 0x0400;
constexpr GLenum DEPTH_TEST = 0x0B71;
constexpr GLenum BLEND = 0x0BE2;
constexpr GLenum CULL_FACE = 0x0B44;
constexpr GLenum SRC_ALPHA = 0x0302;
constexpr GLenum ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum LESS = 0x0201;
constexpr GLenum LEQUAL = 0x0203;
constexpr GLenum BACK = 0x0405;
constexpr GLenum FRONT = 0x0404;
constexpr GLenum FRONT_AND_BACK = 0x0408;
constexpr GLenum CW = 0x0900;
constexpr GLenum CCW = 0x0901;
constexpr GLenum ARRAY_BUFFER = 0x8892;
constexpr GLenum ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum STATIC_DRAW = 0x88E4;
constexpr GLenum DYNAMIC_DRAW = 0x88E8;
constexpr GLenum FRAGMENT_SHADER = 0x8B30;
constexpr GLenum VERTEX_SHADER = 0x8B31;
constexpr GLenum COMPILE_STATUS = 0x8B81;
constexpr GLenum LINK_STATUS = 0x8B82;
constexpr GLenum TEXTURE_2D = 0x0DE1;
constexpr GLenum RGBA = 0x1908;
constexpr GLenum RGB = 0x1907;
constexpr GLenum LUMINANCE = 0x1909;
constexpr GLenum TEXTURE0 = 0x84C0;
constexpr GLenum LINEAR = 0x2601;
constexpr GLenum NEAREST = 0x2600;
constexpr GLenum CLAMP_TO_EDGE = 0x812F;
constexpr GLenum TEXTURE_WRAP_S = 0x2802;
constexpr GLenum TEXTURE_WRAP_T = 0x2803;
constexpr GLenum TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum SCISSOR_TEST = 0x0C11;
constexpr GLenum SCISSOR_BOX = 0x0C10;
constexpr GLenum VIEWPORT = 0x0BA2;
constexpr GLenum LINE_WIDTH = 0x0B21;
constexpr GLenum FRAMEBUFFER = 0x8D40;
constexpr GLenum COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum DEPTH_COMPONENT16 = 0x81A5;
constexpr GLenum DEPTH_ATTACHMENT = 0x8D00;
constexpr GLenum POLYGON_OFFSET_FILL = 0x8037;

// ---- 函数指针 ----
extern GLboolean (*pEnable)(GLenum);
extern void (*pDisable)(GLenum);
extern void (*pBlendFunc)(GLenum, GLenum);
extern void (*pClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*pClear)(GLbitfield);
extern void (*pViewport)(GLint, GLint, GLsizei, GLsizei);
extern void (*pDepthFunc)(GLenum);
extern void (*pDepthMask)(GLboolean);
extern void (*pCullFace)(GLenum);
extern void (*pFrontFace)(GLenum);
extern void (*pColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
extern void (*pReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
extern void (*pPixelStorei)(GLenum, GLint);
extern GLenum (*pGetError)();
extern const GLubyte* (*pGetString)(GLenum);
extern void (*pGetIntegerv)(GLenum, GLint*);
extern void (*pLineWidth)(GLfloat);
extern void (*pPolygonOffset)(GLfloat, GLfloat);
extern void (*pScissor)(GLint, GLint, GLsizei, GLsizei);

extern GLuint (*pCreateShader)(GLenum);
extern void (*pShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
extern void (*pCompileShader)(GLuint);
extern void (*pGetShaderiv)(GLuint, GLenum, GLint*);
extern void (*pGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*pDeleteShader)(GLuint);
extern GLuint (*pCreateProgram)();
extern void (*pAttachShader)(GLuint, GLuint);
extern void (*pLinkProgram)(GLuint);
extern void (*pGetProgramiv)(GLuint, GLenum, GLint*);
extern void (*pGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*pDeleteProgram)(GLuint);
extern void (*pUseProgram)(GLuint);
extern GLint (*pGetUniformLocation)(GLuint, const GLchar*);
extern GLint (*pGetAttribLocation)(GLuint, const GLchar*);
extern void (*pUniform1f)(GLint, GLfloat);
extern void (*pUniform2f)(GLint, GLfloat, GLfloat);
extern void (*pUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
extern void (*pUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*pUniform1i)(GLint, GLint);
extern void (*pUniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat*);
extern void (*pUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);

extern void (*pGenBuffers)(GLsizei, GLuint*);
extern void (*pDeleteBuffers)(GLsizei, const GLuint*);
extern void (*pBindBuffer)(GLenum, GLuint);
extern void (*pBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void (*pBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
extern void (*pEnableVertexAttribArray)(GLuint);
extern void (*pDisableVertexAttribArray)(GLuint);
extern void (*pVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern void (*pDrawArrays)(GLenum, GLint, GLsizei);
extern void (*pDrawElements)(GLenum, GLsizei, GLenum, const void*);

extern void (*pGenTextures)(GLsizei, GLuint*);
extern void (*pDeleteTextures)(GLsizei, const GLuint*);
extern void (*pBindTexture)(GLenum, GLuint);
extern void (*pTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
extern void (*pTexParameteri)(GLenum, GLenum, GLint);
extern void (*pActiveTexture)(GLenum);

extern void (*pGenFramebuffers)(GLsizei, GLuint*);
extern void (*pDeleteFramebuffers)(GLsizei, const GLuint*);
extern void (*pBindFramebuffer)(GLenum, GLuint);
extern void (*pFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
extern GLenum (*pCheckFramebufferStatus)(GLenum);

extern bool loaded;   // 是否加载成功
extern bool isES;     // 是否运行在 OpenGL ES
extern int majorV, minorV;
extern bool hasFBO;   // FBO 可用

bool load(void* (*proc)(const char*)); // 传入 glfwGetProcAddress

// 便捷封装(与全局函数同名的小写)
inline void viewport(GLint x, GLint y, GLsizei w, GLsizei h) { pViewport(x, y, w, h); }
inline void clear(GLbitfield b) { pClear(b); }
inline void clearColor(GLfloat r, GLfloat g, GLfloat b2, GLfloat a) { pClearColor(r, g, b2, a); }
inline void enable(GLenum c) { pEnable(c); }
inline void disable(GLenum c) { pDisable(c); }

} // namespace gl
