#pragma once
// 素材库：贴图、字体、3D 机体、着色器
#include "Common.hpp"
#include "../game/Pose.hpp"

#include <string>
#include <vector>

namespace sky::assets {

enum SkinIndex { SKIN_BLUE = 0, SKIN_GREEN, SKIN_ORANGE, SKIN_PURPLE, SKIN_RED, SKIN_COUNT };
extern const char* kSkinName[SKIN_COUNT];

struct Craft {
    std::string id;      // 英文机型名
    std::string role;    // 中文定位（均衡型 / 突击型 …）
    std::string desc;    // 一句话描述
    int stat[5] = {5, 5, 5, 5, 5};  // 速度 火力 装甲 机动 射速（0..10）
    bool unlocked = false;

    Model       model{};
    BoundingBox bb{};
    float       len = 1.0f;    // 机身长度（世界单位，沿 +Z）
    float       span = 1.0f;   // 翼展（世界单位，沿 X）
    Texture2D   skin[SKIN_COUNT]{};
    bool        loaded = false;
};

// 场景贴图
extern Texture2D logoTex;
extern Texture2D oceanSky, oceanSea, oceanCloudFar, oceanCloudNear, islandA, islandB, carrierSea;
extern Texture2D duskSky, duskCloudFar, duskCloudNear, carrierDusk;
extern Texture2D stormSky, stormCloudFar, stormCloudNear, stormRain, fortress;
extern Texture2D hangarBg;

extern Shader unlit;

extern std::vector<Craft> players;  // 6 架
extern std::vector<Craft> enemies;  // 4 架
extern Craft              boss;

void init();
void shutdown();

// 机体分帧加载（BOOT 屏用）
int  modelJobCount();
bool modelJobLoaded(int i);
void loadModelJob(int i);

// 字体：按需求字号取最接近且不小于它的图集
Font& body(int size);
Font& title(int size);

// 3D 绘制：以包围盒中心为轴心。
// facing = +1 机头朝 +X（玩家），-1 机头朝 -X（敌机，姿态自动镜像）
// 机体先按 baseYaw 摆正到 ±X，再叠加姿态（绕 X 滚转、绕 Z 俯仰），绕 Y 偏航恒为 0
void drawCraft(const Craft& c, float sx, float sy, int skin, const pose::Pose& p, float scalePx, Color tint,
               int facing = 1);

}  // namespace sky::assets
