#pragma once
// 公共类型、数学工具与色板
#include "raylib-cpp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace sky {

// 逻辑分辨率（所有布局都基于这个坐标系）
constexpr float VW = 1920.0f;
constexpr float VH = 1080.0f;

// 相机：正交投影，从斜上方约 28° 俯视（与水平面夹角）→ 偏离竖直 62°
// 这样机体呈现"侧视图 + 能看到机翼顶面"，与横版背景的透视一致
constexpr float kCamElevation = 28.0f * DEG2RAD;
constexpr float kCamTilt = (90.0f - 28.0f) * DEG2RAD;
inline float CamCos() { return std::cos(kCamTilt); }
inline float CamSin() { return std::sin(kCamTilt); }

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline int   clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float approach(float cur, float target, float step) {
    if (cur < target) return std::min(cur + step, target);
    return std::max(cur - step, target);
}
// 0..1 的缓动
inline float easeOutCubic(float t) { t = clampf(t, 0, 1); float u = 1 - t; return 1 - u * u * u; }
inline float easeInCubic(float t) { t = clampf(t, 0, 1); return t * t * t; }
// 两头速度为 0 的 S 曲线：位移用它起步/收尾都不会"顿一下"
inline float easeInOutCubic(float t) {
    t = clampf(t, 0, 1);
    if (t < 0.5f) return 4.0f * t * t * t;
    float u = -2.0f * t + 2.0f;
    return 1.0f - (u * u * u) * 0.5f;
}
inline float easeOutBack(float t) {
    t = clampf(t, 0, 1);
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    float u = t - 1;
    return 1 + c3 * u * u * u + c1 * u * u;
}

// 像素阶梯：把连续时间量化成 n 档，用于"不要平滑"的像素动效
inline float stepped(float t, int steps) {
    if (t <= 0) return 0.0f;
    if (t >= 1) return 1.0f;
    return std::floor(t * steps) / (float)steps;
}

// 设计稿色板（取自素材）
namespace pal {
constexpr Color kCyan    = {69, 214, 232, 255};   // #45D6E8 主色/选中
constexpr Color kGold    = {255, 176, 58, 255};   // #FFB03A 强调/奖励
constexpr Color kDanger  = {255, 90, 71, 255};    // #FF5A47 危险/警告
constexpr Color kText    = {232, 241, 248, 255};  // #E8F1F8 正文
constexpr Color kDim     = {138, 155, 176, 255};  // #8A9BB0 次要文字
constexpr Color kPanelBg = {6, 12, 22, 209};      // rgba(6,12,22,0.82)
constexpr Color kInk     = {4, 8, 14, 255};       // 近黑
}  // namespace pal

inline Color WithAlpha(Color c, float a) {
    c.a = (unsigned char)clampf(a * 255.0f, 0, 255);
    return c;
}
inline Color Mix(Color a, Color b, float t) {
    t = clampf(t, 0, 1);
    return Color{(unsigned char)lerpf((float)a.r, b.r, t), (unsigned char)lerpf((float)a.g, b.g, t),
                 (unsigned char)lerpf((float)a.b, b.b, t), (unsigned char)lerpf((float)a.a, b.a, t)};
}
inline Color Scale(Color c, float k) {
    return Color{(unsigned char)clampf(c.r * k, 0, 255), (unsigned char)clampf(c.g * k, 0, 255),
                 (unsigned char)clampf(c.b * k, 0, 255), c.a};
}

// 简单随机（可复现）
struct Rng {
    uint32_t s = 0x9E3779B9u;
    void seed(uint32_t v) { s = v ? v : 1u; }
    uint32_t next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    float f() { return (next() >> 8) * (1.0f / 16777216.0f); }          // [0,1)
    float range(float a, float b) { return a + (b - a) * f(); }
    int   irange(int a, int b) { return a + (int)(next() % (uint32_t)std::max(1, b - a + 1)); }
};

}  // namespace sky
