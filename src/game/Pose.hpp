#pragma once
// 战机姿态（Pose）
//
// 设计原则：姿态是运动的结果，不是输入。
// 按键只改变速度，倾角由当前垂直速度反推 → 有滞后、有回正、有甩尾。
//
// 三轴分工：
//   绕 X 轴 —— 滚转（侧倾），唯一由速度驱动的轴
//   绕 Z 轴 —— 俯仰（机头抬/压），不独立控制，= 滚转 × 0.35 联动
//   绕 Y 轴 —— 恒为 0（横版视角下偏航会让机翼透视穿帮）
//
// 数值来源（规范）：
//   目标倾角 = 基准 − 垂直速度 × 系数，再钳制
//   当前倾角以指数衰减趋近目标，速率 9.0/秒（就是"跟手程度"）
//
// 规范里 vy 以「机体自身最高垂直速度 = 5.2」为单位；
// 本实现把 vy 归一化到 [-1, 1]，等价系数 = 5.2 × 0.075 = 0.39。

#include "Common.hpp"

namespace sky::pose {

struct Pose {
    float roll = 0.0f;   // 绕 X 轴，弧度
    float pitch = 0.0f;  // 绕 Z 轴，弧度
};

struct Spec {
    float base = -0.16f;       // 巡航基准倾角（常态就是微微倾斜）
    float vyGain = 0.39f;      // 垂直速度系数
    float minRoll = -0.55f;    // 拉升幅度（更夸张）
    float maxRoll = 0.30f;     // 俯冲幅度（更收敛）
    float rate = 9.0f;         // 指数趋近速率 / 秒
    float pitchRatio = 0.35f;  // 俯仰联动比例
    float initRoll = -0.16f;   // 出生初值，避免第一帧从 0 弹到目标
};

// 玩家：基准 -0.16，系数 0.075（→0.39），钳制 [-0.55, +0.30]，速率 9.0
inline const Spec kPlayer{-0.16f, 0.39f, -0.55f, 0.30f, 9.0f, 0.35f, -0.16f};
// 敌机：共用同一套公式，基准 -0.08，系数 0.08（→0.40），钳制 ±0.4
inline const Spec kEnemy{-0.08f, 0.40f, -0.40f, 0.40f, 7.0f, 0.35f, -0.08f};

// 由滚转角直接构造姿态（机库展示用：选一个角度即可复用）
inline Pose FromRoll(float roll, float pitchRatio = 0.35f) {
    Pose p;
    p.roll = roll;
    p.pitch = -roll * pitchRatio;  // 负号：滚转下沉侧对应机头抬起
    return p;
}

// 目标倾角：基准 − 垂直速度 × 系数，钳制在非对称区间内
inline float TargetRoll(const Spec& s, float vyNorm) {
    return clampf(s.base - vyNorm * s.vyGain, s.minRoll, s.maxRoll);
}

// 指数平滑趋近（不会瞬时等于目标）
inline void Update(Pose& p, const Spec& s, float vyNorm, float dt) {
    float target = TargetRoll(s, vyNorm);
    float k = 1.0f - std::exp(-s.rate * dt);
    p.roll += (target - p.roll) * k;
    p.pitch = -p.roll * s.pitchRatio;
}

// 出生瞬间给固定初值
inline void Reset(Pose& p, const Spec& s, float initRoll = 999.0f) {
    p.roll = (initRoll > 900.0f) ? s.initRoll : initRoll;
    p.pitch = -p.roll * s.pitchRatio;
}

}  // namespace sky::pose
