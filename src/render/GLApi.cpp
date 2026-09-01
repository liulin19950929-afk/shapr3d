// GLApi.cpp
#include "GLApi.h"
#include "../core/Common.h"
#include <cstring>

namespace gl {

bool loaded = false;
bool isES = false;
int majorV = 0, minorV = 0;
bool hasFBO = false;

GLboolean (*pEnable)(GLenum) = nullptr;
void (*pDisable)(GLenum) = nullptr;
void (*pBlendFunc)(GLenum, GLenum) = nullptr;
void (*pClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*pClear)(GLbitfield) = nullptr;
void (*pViewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (*pDepthFunc)(GLenum) = nullptr;
void (*pDepthMask)(GLboolean) = nullptr;
void (*pCullFace)(GLenum) = nullptr;
void (*pFrontFace)(GLenum) = nullptr;
void (*pColorMask)(GLboolean, GLboolean, GLboolean, GLboolean) = nullptr;
void (*pReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
void (*pPixelStorei)(GLenum, GLint) = nullptr;
GLenum (*pGetError)() = nullptr;
const GLubyte* (*pGetString)(GLenum) = nullptr;
void (*pGetIntegerv)(GLenum, GLint*) = nullptr;
void (*pLineWidth)(GLfloat) = nullptr;
void (*pPolygonOffset)(GLfloat, GLfloat) = nullptr;
void (*pScissor)(GLint, GLint, GLsizei, GLsizei) = nullptr;

GLuint (*pCreateShader)(GLenum) = nullptr;
void (*pShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*) = nullptr;
void (*pCompileShader)(GLuint) = nullptr;
void (*pGetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
void (*pGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*pDeleteShader)(GLuint) = nullptr;
GLuint (*pCreateProgram)() = nullptr;
void (*pAttachShader)(GLuint, GLuint) = nullptr;
void (*pLinkProgram)(GLuint) = nullptr;
void (*pGetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
void (*pGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*pDeleteProgram)(GLuint) = nullptr;
void (*pUseProgram)(GLuint) = nullptr;
GLint (*pGetUniformLocation)(GLuint, const GLchar*) = nullptr;
GLint (*pGetAttribLocation)(GLuint, const GLchar*) = nullptr;
void (*pUniform1f)(GLint, GLfloat) = nullptr;
void (*pUniform2f)(GLint, GLfloat, GLfloat) = nullptr;
void (*pUniform3f)(GLint, GLfloat, GLfloat, GLfloat) = nullptr;
void (*pUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*pUniform1i)(GLint, GLint) = nullptr;
void (*pUniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
void (*pUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;

void (*pGenBuffers)(GLsizei, GLuint*) = nullptr;
void (*pDeleteBuffers)(GLsizei, const GLuint*) = nullptr;
void (*pBindBuffer)(GLenum, GLuint) = nullptr;
void (*pBufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
void (*pBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*) = nullptr;
void (*pEnableVertexAttribArray)(GLuint) = nullptr;
void (*pDisableVertexAttribArray)(GLuint) = nullptr;
void (*pVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
void (*pDrawArrays)(GLenum, GLint, GLsizei) = nullptr;
void (*pDrawElements)(GLenum, GLsizei, GLenum, const void*) = nullptr;

void (*pGenTextures)(GLsizei, GLuint*) = nullptr;
void (*pDeleteTextures)(GLsizei, const GLuint*) = nullptr;
void (*pBindTexture)(GLenum, GLuint) = nullptr;
void (*pTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
void (*pTexParameteri)(GLenum, GLenum, GLint) = nullptr;
void (*pActiveTexture)(GLenum) = nullptr;

void (*pGenFramebuffers)(GLsizei, GLuint*) = nullptr;
void (*pDeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
void (*pBindFramebuffer)(GLenum, GLuint) = nullptr;
void (*pFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
GLenum (*pCheckFramebufferStatus)(GLenum) = nullptr;

template <typename T>
static bool loadFn(T& fn, void* (*proc)(const char*), const char* name, const char* alt = nullptr) {
    fn = reinterpret_cast<T>(proc(name));
    if (!fn && alt) fn = reinterpret_cast<T>(proc(alt));
    return fn != nullptr;
}

bool load(void* (*proc)(const char*)) {
    if (!proc) return false;

    bool ok = true;
    ok &= loadFn(pEnable, proc, "glEnable");
    ok &= loadFn(pDisable, proc, "glDisable");
    ok &= loadFn(pBlendFunc, proc, "glBlendFunc");
    ok &= loadFn(pClearColor, proc, "glClearColor");
    ok &= loadFn(pClear, proc, "glClear");
    ok &= loadFn(pViewport, proc, "glViewport");
    ok &= loadFn(pDepthFunc, proc, "glDepthFunc");
    ok &= loadFn(pDepthMask, proc, "glDepthMask");
    ok &= loadFn(pCullFace, proc, "glCullFace");
    ok &= loadFn(pFrontFace, proc, "glFrontFace");
    ok &= loadFn(pColorMask, proc, "glColorMask");
    ok &= loadFn(pReadPixels, proc, "glReadPixels");
    ok &= loadFn(pPixelStorei, proc, "glPixelStorei");
    ok &= loadFn(pGetError, proc, "glGetError");
    ok &= loadFn(pGetString, proc, "glGetString");
    ok &= loadFn(pGetIntegerv, proc, "glGetIntegerv");
    ok &= loadFn(pLineWidth, proc, "glLineWidth");
    ok &= loadFn(pScissor, proc, "glScissor");
    ok &= loadFn(pPolygonOffset, proc, "glPolygonOffset");

    ok &= loadFn(pCreateShader, proc, "glCreateShader");
    ok &= loadFn(pShaderSource, proc, "glShaderSource");
    ok &= loadFn(pCompileShader, proc, "glCompileShader");
    ok &= loadFn(pGetShaderiv, proc, "glGetShaderiv");
    ok &= loadFn(pGetShaderInfoLog, proc, "glGetShaderInfoLog");
    ok &= loadFn(pDeleteShader, proc, "glDeleteShader");
    ok &= loadFn(pCreateProgram, proc, "glCreateProgram");
    ok &= loadFn(pAttachShader, proc, "glAttachShader");
    ok &= loadFn(pLinkProgram, proc, "glLinkProgram");
    ok &= loadFn(pGetProgramiv, proc, "glGetProgramiv");
    ok &= loadFn(pGetProgramInfoLog, proc, "glGetProgramInfoLog");
    ok &= loadFn(pDeleteProgram, proc, "glDeleteProgram");
    ok &= loadFn(pUseProgram, proc, "glUseProgram");
    ok &= loadFn(pGetUniformLocation, proc, "glGetUniformLocation");
    ok &= loadFn(pGetAttribLocation, proc, "glGetAttribLocation");
    ok &= loadFn(pUniform1f, proc, "glUniform1f");
    ok &= loadFn(pUniform2f, proc, "glUniform2f");
    ok &= loadFn(pUniform3f, proc, "glUniform3f");
    ok &= loadFn(pUniform4f, proc, "glUniform4f");
    ok &= loadFn(pUniform1i, proc, "glUniform1i");
    ok &= loadFn(pUniformMatrix3fv, proc, "glUniformMatrix3fv");
    ok &= loadFn(pUniformMatrix4fv, proc, "glUniformMatrix4fv");

    ok &= loadFn(pGenBuffers, proc, "glGenBuffers");
    ok &= loadFn(pDeleteBuffers, proc, "glDeleteBuffers");
    ok &= loadFn(pBindBuffer, proc, "glBindBuffer");
    ok &= loadFn(pBufferData, proc, "glBufferData");
    ok &= loadFn(pBufferSubData, proc, "glBufferSubData");
    ok &= loadFn(pEnableVertexAttribArray, proc, "glEnableVertexAttribArray");
    ok &= loadFn(pDisableVertexAttribArray, proc, "glDisableVertexAttribArray");
    ok &= loadFn(pVertexAttribPointer, proc, "glVertexAttribPointer");
    ok &= loadFn(pDrawArrays, proc, "glDrawArrays");
    ok &= loadFn(pDrawElements, proc, "glDrawElements");

    ok &= loadFn(pGenTextures, proc, "glGenTextures");
    ok &= loadFn(pDeleteTextures, proc, "glDeleteTextures");
    ok &= loadFn(pBindTexture, proc, "glBindTexture");
    ok &= loadFn(pTexImage2D, proc, "glTexImage2D");
    ok &= loadFn(pTexParameteri, proc, "glTexParameteri");
    ok &= loadFn(pActiveTexture, proc, "glActiveTexture");

    // FBO: ES2 核心 / 桌面 GL3+ 或 EXT 扩展
    loadFn(pGenFramebuffers, proc, "glGenFramebuffers", "glGenFramebuffersEXT");
    loadFn(pDeleteFramebuffers, proc, "glDeleteFramebuffers", "glDeleteFramebuffersEXT");
    loadFn(pBindFramebuffer, proc, "glBindFramebuffer", "glBindFramebufferEXT");
    loadFn(pFramebufferTexture2D, proc, "glFramebufferTexture2D", "glFramebufferTexture2DEXT");
    loadFn(pCheckFramebufferStatus, proc, "glCheckFramebufferStatus", "glCheckFramebufferStatusEXT");
    hasFBO = pGenFramebuffers && pBindFramebuffer && pFramebufferTexture2D && pCheckFramebufferStatus;

    const char* ver = (const char*)pGetString(0x1F02 /*GL_VERSION*/);
    const char* glslVer = (const char*)pGetString(0x8B8C /*GL_SHADING_LANGUAGE_VERSION*/);
    if (ver) {
        isES = strncmp(ver, "OpenGL ES", 9) == 0;
        sscanf(isES ? ver + 10 : ver, "%d.%d", &majorV, &minorV);
    }
    loaded = ok;
    LOGI("GL 初始化: %s | GLSL %s | FBO=%d", ver ? ver : "?", glslVer ? glslVer : "?", (int)hasFBO);
    return ok;
}

} // namespace gl
