// Math3D.h —— 4x4 矩阵(列主序, 与 OpenGL 一致)
#pragma once
#include "../core/Common.h"

namespace cad {

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    static Mat4 identity() { return {}; }

    static Mat4 perspective(float fovYRad, float aspect, float zn, float zf) {
        Mat4 r;
        float f = 1.0f / std::tan(fovYRad / 2);
        std::fill(r.m, r.m + 16, 0.f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zf + zn) / (zn - zf);
        r.m[11] = -1;
        r.m[14] = 2 * zf * zn / (zn - zf);
        return r;
    }
    static Mat4 ortho(float l, float r_, float b, float t, float zn, float zf) {
        Mat4 m;
        std::fill(m.m, m.m + 16, 0.f);
        m.m[0] = 2 / (r_ - l);
        m.m[5] = 2 / (t - b);
        m.m[10] = -2 / (zf - zn);
        m.m[12] = -(r_ + l) / (r_ - l);
        m.m[13] = -(t + b) / (t - b);
        m.m[14] = -(zf + zn) / (zf - zn);
        return m;
    }
    static Mat4 lookAt(const Vec3& eye, const Vec3& at, const Vec3& up) {
        Vec3 f = (at - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);
        Mat4 r;
        r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -s.dot(eye);
        r.m[13] = -u.dot(eye);
        r.m[14] = f.dot(eye);
        return r;
    }
    static Mat4 translate(const Vec3& t) {
        Mat4 r;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }
    static Mat4 scale(float s) {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = s;
        return r;
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + rr] * o.m[c * 4 + k];
                r.m[c * 4 + rr] = s;
            }
        return r;
    }
    Vec3 transformPoint(const Vec3& p) const {
        return {
            m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
            m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
            m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
    }
    Vec3 transformDir(const Vec3& d) const {
        return {m[0] * d.x + m[4] * d.y + m[8] * d.z,
                m[1] * d.x + m[5] * d.y + m[9] * d.z,
                m[2] * d.x + m[6] * d.y + m[10] * d.z};
    }
};

// 带透明度的颜色/向量
struct Vec4 {
    float x = 0, y = 0, z = 0, w = 1;
    Vec4() = default;
    Vec4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
};

// 相机: 轨道球
struct Camera {
    Vec3 target{0, 0, 30};
    float yaw = -60 * float(M_PI) / 180, pitch = -25 * float(M_PI) / 180;
    float dist = 300;
    float fov = 45 * float(M_PI) / 180;
    bool ortho = false;

    Vec3 eye() const {
        float cp = std::cos(pitch), sp = std::sin(pitch);
        float cy = std::cos(yaw), sy = std::sin(yaw);
        Vec3 dir{cp * cy, cp * sy, sp};
        return target + dir * dist;
    }
    Vec3 viewDir() const { return (target - eye()).normalized(); }

    Mat4 view() const {
        Vec3 up{0, 0, 1};
        // 俯仰接近 ±90° 时避免奇异
        if (std::fabs(std::cos(pitch)) < 1e-3) up = Vec3{0, 1, 0};
        return Mat4::lookAt(eye(), target, up);
    }
    Mat4 proj(float aspect) const {
        return ortho ? Mat4::ortho(-dist * aspect / 2.2f, dist * aspect / 2.2f, -dist / 2.2f, dist / 2.2f, -4000, 4000)
                     : Mat4::perspective(fov, aspect, 1.0f, 20000.f);
    }
    void frame(const Vec3& mn, const Vec3& mx) {
        target = (mn + mx) * 0.5f;
        float dx = mx.x - mn.x, dy = mx.y - mn.y, dz = mx.z - mn.z;
        float r = std::sqrt(dx * dx + dy * dy + dz * dz);
        dist = std::max(20.f, r * 1.6f);
    }
};

} // namespace cad
