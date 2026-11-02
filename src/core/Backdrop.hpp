#pragma once
// 视差背景：黄昏 / 海洋 / 雷暴 三套，可交叉淡化
#include "Common.hpp"

namespace sky {

class Backdrop {
public:
    enum Preset { Dusk, Ocean, Storm };

    void setup(Preset p);
    void update(float dt);
    void draw();          // 天空 → 远景云 → 海面
    void drawProps();     // 海面上漂的岛屿/航母
    void drawNearClouds();  // 近景云：压在 props 之上的最前层

    void  setSpeed(float s) { speed_ = s; }
    void  setAlpha(float a) { alpha_ = clampf(a, 0, 1); }
    float speed() const { return speed_; }
    void  reset();

    Preset preset() const { return preset_; }

    // 战斗用：海里漂浮的物件
    bool  propsEnabled = true;
    float propRate = 2.9f;  // 秒/个

private:
    Preset preset_ = Ocean;
    float  speed_ = 220.0f;
    float  alpha_ = 1.0f;
    float  offSky_ = 0, offFar_ = 0, offNear_ = 0, offSea_ = 0, offRainX_ = 0, offRainY_ = 0;
    float  time_ = 0;

    struct Prop {
        int   kind = 0;  // 0 islandA 1 islandB 2 carrier
        float x = 0, y = 0, scale = 1.0f, speedMul = 1.0f;
        bool  active = false;
    };
    Prop  props_[6]{};
    float propTimer_ = 0;
    Rng   rng_;
};

}  // namespace sky
