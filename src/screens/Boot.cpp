// BOOT 启动屏 + 资源分帧加载
#include "Screens.hpp"

#include "App.hpp"
#include "Assets.hpp"
#include "Audio.hpp"
#include "Save.hpp"
#include "Ui.hpp"

namespace sky {

void BootScreen::enter() {
    save::load();
    app().volMaster = save::data().volMaster;
    app().volMusic = save::data().volMusic;
    app().volSfx = save::data().volSfx;
    app().optShake = save::data().shake;
    app().optReduceFlash = save::data().reduceFlash;
    app().applyVolume();
    assets::players[0].unlocked = true;
}

void BootScreen::update(float dt) {
    t_ += dt;

    // 分帧加载：每帧一个机型，避免长时间卡住
    int total = assets::modelJobCount();
    if (loaded_ < total) {
        assets::loadModelJob(loaded_);
        loaded_++;
        if (loaded_ >= total) doneT_ = t_;
    }

    if (loaded_ >= total && !leaving_) {
        if (doneT_ < 0) doneT_ = t_;
        bool timeout = (t_ - doneT_ > 0.75f);
        bool skip = app().in.anyKey && t_ > 0.6f;
        if (timeout || skip) {
            leaving_ = true;
            // 不走进场转场：同一帧换成主菜单，logo 停在现在这个位置，
            // 由 TitleScreen 的入场动画把它抬到菜单标题位（中间没有黑场）
            app().beginIntro();
        }
    }
}

void BootScreen::drawWorld() { DrawRectangle(0, 0, (int)VW, (int)VH, pal::kInk); }

void BootScreen::draw() {
    DrawRectangle(0, 0, (int)VW, (int)VH, pal::kInk);

    // Logo：缓入放大到满尺寸，结束时正好是 intro::kBootLogoW / kBootLogoY，
    // 与主菜单入场动画的起点严丝合缝（这里是整段开场的第一拍，不量化成阶梯，
    // 否则和后面平滑的平移接不上）。
    float appear = easeOutCubic(t_ / 0.45f);
    float scale = lerpf(0.96f, 1.0f, appear);
    float alpha = clampf(t_ / 0.28f, 0, 1);

    float w = intro::kBootLogoW * scale, h = intro::kBootLogoW * intro::kLogoAspect * scale;
    if (assets::logoTex.id) {
        Rectangle src{0, 0, (float)assets::logoTex.width, (float)assets::logoTex.height};
        Rectangle dst{VW * 0.5f - w * 0.5f, intro::kBootLogoY + (1.0f - appear) * 12.0f, w, h};
        DrawTexturePro(assets::logoTex, src, dst, {0, 0}, 0, WithAlpha(WHITE, alpha));
    }

    int total = assets::modelJobCount();
    float prog = total > 0 ? (float)loaded_ / total : 0.0f;
    if (prog < 1.0f) {
        float bx = VW * 0.5f - 220, by = 700;
        DrawRectangle((int)bx, (int)by, 440, 4, WithAlpha(WHITE, 0.15f));
        DrawRectangle((int)bx, (int)by, (int)(440 * prog), 4, pal::kCyan);
        ui::TextC("载入中 " + std::to_string((int)(prog * 100)) + "%", VW * 0.5f, by + 20, 20,
                  WithAlpha(pal::kDim, 0.9f));
    } else {
        float a = 0.35f + 0.35f * ui::Pulse(0.9f);
        ui::TextC("按任意键继续", VW * 0.5f, 706, 24, WithAlpha(pal::kDim, a));
    }

    ui::Text("v0.1.0", 48, VH - 56, 20, WithAlpha(pal::kDim, 0.75f));
    ui::TextR("(C) 2026", VW - 48, VH - 56, 20, WithAlpha(pal::kDim, 0.75f));
}

}  // namespace sky
