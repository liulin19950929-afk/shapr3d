// Common.h —— 基础类型 / 日志 / 数学小工具
#pragma once
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <cassert>

namespace cad {

using Id = uint64_t;
constexpr Id kInvalidId = 0;

// ---------- 日志 ----------
enum class LogLevel { Debug, Info, Warn, Error };
void logMessage(LogLevel lv, const char* fmt, ...);

#define LOGD(...) ::cad::logMessage(::cad::LogLevel::Debug, __VA_ARGS__)
#define LOGI(...) ::cad::logMessage(::cad::LogLevel::Info,  __VA_ARGS__)
#define LOGW(...) ::cad::logMessage(::cad::LogLevel::Warn,  __VA_ARGS__)
#define LOGE(...) ::cad::logMessage(::cad::LogLevel::Error, __VA_ARGS__)

// ---------- 二维向量(草图) ----------
struct Vec2 {
    double x = 0, y = 0;
    Vec2() = default;
    Vec2(double X, double Y) : x(X), y(Y) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(double s) const { return {x * s, y * s}; }
    Vec2 operator-() const { return {-x, -y}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    double dot(const Vec2& o) const { return x * o.x + y * o.y; }
    double cross(const Vec2& o) const { return x * o.y - y * o.x; }
    double length() const { return std::hypot(x, y); }
    Vec2 normalized() const { double l = length(); return l > 1e-12 ? Vec2{x / l, y / l} : Vec2{}; }
};

inline double dist(const Vec2& a, const Vec2& b) { return (a - b).length(); }

// ---------- 三维向量(渲染/交互, 模型数据一律走 OCCT gp_*) ----------
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float length() const { return std::sqrt(dot(*this)); }
    Vec3 normalized() const { float l = length(); return l > 1e-12f ? *this * (1.0f / l) : Vec3{}; }
};

// ---------- 工具 ----------
inline std::string fmtLength(double mm) {
    char buf[64];
    if (std::fabs(mm) >= 1000.0) snprintf(buf, sizeof(buf), "%.3f m", mm / 1000.0);
    else snprintf(buf, sizeof(buf), "%.3f mm", mm);
    return buf;
}
inline std::string fmtArea(double mm2) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.3f mm²", mm2);
    return buf;
}
inline std::string fmtVolume(double mm3) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.3f cm³", mm3 / 1000.0);
    return buf;
}

// 简易秒表(性能统计)
struct Stopwatch {
    double startMs = 0, elapsedMs = 0;
    void start();
    double stop();          // 返回毫秒
    static double nowMs();  // 单调时钟毫秒
};

} // namespace cad
