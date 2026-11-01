#pragma once
// 存档：最高分、设置、已选机型
#include <string>

namespace sky::save {

struct Profile {
    int   hiScore = 0;
    int   lastScore = 0;
    int   stars[3] = {0, 0, 0};  // 三个关卡的最好星数
    int   stageScore[3] = {0, 0, 0};
    int   stageKills[3] = {0, 0, 0};
    int   stageTime[3] = {0, 0, 0};
    int   kills = 0;
    int   runs = 0;
    int   craft = 0;
    int   skin = 0;
    float volMaster = 80.0f;
    float volMusic = 45.0f;
    float volSfx = 85.0f;
    bool  shake = true;
    bool  reduceFlash = false;
    int   keyFire = 'J';
    int   keyBomb = 'K';
};

Profile& data();
void     load();
void     save();
void     reset();

}  // namespace sky::save
