#include "Backdrop.hpp"

#include "Assets.hpp"

namespace sky {

// 全局 alpha 乘子（用于海洋→雷暴交叉淡化）
static float g_alpha = 1.0f;
static Color A(Color c) {
    c.a = (unsigned char)(c.a * g_alpha);
    return c;
}

static void DrawTiledX(Texture2D tex, float y, float destH, float offset, Color tint) {
    if (tex.id == 0) return;
    float k = destH / (float)tex.height;
    float w = tex.width * k;
    float x = -std::fmod(offset, w);
    if (x > 0) x -= w;
    for (; x < VW; x += w) {
        Rectangle src{0, 0, (float)tex.width, (float)tex.height};
        Rectangle dst{x, y, w, destH};
        DrawTexturePro(tex, src, dst, {0, 0}, 0, tint);
    }
}

// 平铺整屏（雨丝）
static void DrawTiledXY(Texture2D tex, float ox, float oy, float scale, Color tint) {
    if (tex.id == 0) return;
    float w = tex.width * scale, h = tex.height * scale;
    float sx = -std::fmod(ox, w);
    float sy = -std::fmod(oy, h);
    if (sx > 0) sx -= w;
    if (sy > 0) sy -= h;
    for (float y = sy; y < VH; y += h)
        for (float x = sx; x < VW; x += w) {
            DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height}, {x, y, w, h}, {0, 0}, 0, tint);
        }
}

void Backdrop::reset() {
    offSky_ = offFar_ = offNear_ = offSea_ = offRainX_ = offRainY_ = 0;
    for (auto& p : props_) p.active = false;
    propTimer_ = 0;
    rng_.seed(0xC0FFEE);
}

void Backdrop::setup(Preset p) {
    preset_ = p;
    reset();
    if (p == Ocean) {
        speed_ = 220.0f;
        // 初始铺几个物件，避免开场空荡
        props_[0] = {0, 1500, 780, 1.0f, 1.0f, true};
        props_[1] = {1, 700, 900, 1.15f, 1.0f, true};
        props_[2] = {2, 2400, 600, 0.62f, 0.6f, true};
    } else if (p == Dusk) {
        speed_ = 60.0f;
    } else {
        speed_ = 260.0f;
    }
}

void Backdrop::update(float dt) {
    time_ += dt;
    float v = speed_ * dt;
    offFar_ += v * 0.16f;
    offNear_ += v * 0.42f;
    offSea_ += v * 1.0f;
    if (preset_ == Storm) {
        offRainX_ += v * 2.1f;
        // 雨往下落：offset 减小时 DrawTiledXY 的起始 y 才会逐帧增大（向下平移）。
        // 之前是 +=，整层雨在往上飘。
        offRainY_ -= v * 2.6f;
    }
    if (preset_ != Ocean || !propsEnabled) return;

    // 物件循环
    propTimer_ -= dt;
    float maxX = -1e9f;
    for (auto& p : props_) {
        if (!p.active) continue;
        p.x -= speed_ * p.speedMul * dt;
        maxX = std::max(maxX, p.x);
    }
    for (auto& p : props_) {
        if (p.active && p.x < -900) p.active = false;
    }
    if (propTimer_ <= 0 && maxX < VW + 200) {
        for (auto& p : props_) {
            if (p.active) continue;
            int kind = rng_.irange(0, 9) < 9 ? rng_.irange(0, 1) : 2;
            p.kind = kind;
            p.x = VW + rng_.range(120, 420);
            if (kind == 2) {
                p.y = rng_.range(540, 640);
                p.scale = rng_.range(0.5f, 0.68f);
                p.speedMul = 0.55f;
            } else {
                p.y = rng_.range(700, 960);
                p.scale = 0.55f + (p.y - 700) / 260.0f * 0.55f;
                p.speedMul = 1.0f;
            }
            p.active = true;
            break;
        }
        propTimer_ = propRate * rng_.range(0.6f, 1.4f);
    }
}

void Backdrop::draw() {
    g_alpha = alpha_;
    if (preset_ == Dusk) {
        // 构图对齐 assets/previews/preview_02_dusk.png（逐层做过像素级拟合，
        // 合成误差 < 2/255）：天空 1:1 铺满，云层 1:1 不缩放，航母贴在 (1304,562)
        if (assets::duskSky.id) DrawTexture(assets::duskSky, 0, 0, A(WHITE));
        DrawTiledX(assets::duskCloudFar, 250, (float)assets::duskCloudFar.height, offFar_, A(WHITE));
        DrawTiledX(assets::duskCloudNear, 762, (float)assets::duskCloudNear.height, offNear_, A(WHITE));
        // 静态航母剪影
        if (assets::carrierDusk.id) {
            DrawTexturePro(assets::carrierDusk,
                           {0, 0, (float)assets::carrierDusk.width, (float)assets::carrierDusk.height},
                           {1304, 562, (float)assets::carrierDusk.width, (float)assets::carrierDusk.height},
                           {0, 0}, 0, A(WHITE));
        }
        return;
    }

    if (preset_ == Ocean) {
        if (assets::oceanSky.id) DrawTexture(assets::oceanSky, 0, 0, A(WHITE));
        DrawTiledX(assets::oceanCloudFar, 60, assets::oceanCloudFar.height * 1.0f, offFar_, A(WHITE));
        // 海面。海平线不需要额外的白色雾带：sea_tile 自带渐变，硬边的雾条反而会在
        // 天空上横着糊出一条白带（以前踩过）。
        float seaTop = 300.0f, seaH = VH - seaTop;
        DrawTiledX(assets::oceanSea, seaTop, seaH, offSea_, A(WHITE));
        // 近景云不在这里画：它要压在海面上的岛屿/航母之上，由 drawNearClouds()
        // 在 drawProps() 之后单独调用。
        return;
    }

    // Storm
    if (assets::stormSky.id) DrawTexture(assets::stormSky, 0, 0, A(WHITE));
    DrawTiledX(assets::stormCloudFar, 40, assets::stormCloudFar.height * 1.2f, offFar_, A(WithAlpha(WHITE, 0.85f)));
    DrawTiledX(assets::stormCloudNear, 150, assets::stormCloudNear.height * 1.25f, offNear_,
               A(WithAlpha(WHITE, 0.9f)));
    DrawTiledXY(assets::stormRain, offRainX_, offRainY_, 1.15f, A(WithAlpha(WHITE, 0.34f)));
}

void Backdrop::drawNearClouds() {
    // 近景云：整个场景里最靠镜头的一层，压在海面与海上的岛屿/航母之上。
    // 顺序由调用方保证（见 World::drawBackground）：海面 → drawProps() → 这里。
    if (preset_ != Ocean) return;
    g_alpha = alpha_;
    DrawTiledX(assets::oceanCloudNear, 130, assets::oceanCloudNear.height * 1.15f, offNear_,
               A(WithAlpha(WHITE, 0.96f)));
}

void Backdrop::drawProps() {
    g_alpha = alpha_;
    if (preset_ != Ocean) return;

    // 按 y 从小到大画：越靠屏幕下方离镜头越近，后画的盖住前面的。
    // 不排序的话，远海航母可能压住更近的岛（遮挡关系反了）。
    constexpr int kMax = (int)(sizeof(props_) / sizeof(props_[0]));
    int order[kMax], n = 0;
    for (int i = 0; i < kMax; i++)
        if (props_[i].active) order[n++] = i;
    for (int i = 1; i < n; i++) {
        int cur = order[i], j = i - 1;
        while (j >= 0 && props_[order[j]].y > props_[cur].y) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = cur;
    }

    for (int i = 0; i < n; i++) {
        const Prop& p = props_[order[i]];
        Texture2D t = (p.kind == 0) ? assets::islandA : (p.kind == 1 ? assets::islandB : assets::carrierSea);
        if (t.id == 0) continue;
        float w = t.width * p.scale, h = t.height * p.scale;
        DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, {p.x, p.y, w, h}, {w * 0.5f, h * 0.5f}, 0,
                       A(WHITE));
    }
}

}  // namespace sky
