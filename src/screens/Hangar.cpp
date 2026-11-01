// HANGAR 机库（选机 / 结算）+ BRIEFING 简报
#include "Screens.hpp"

#include "App.hpp"
#include "Assets.hpp"
#include "Audio.hpp"
#include "Save.hpp"
#include "Ui.hpp"

#include <cstdio>

namespace sky {

static const char* kStatName[5] = {"速度", "火力", "装甲", "机动", "射速"};
static const char* kSkinTag[4] = {"BLUE", "GREEN", "ORANGE", "PURPLE"};

// 机库属性面板与配色色卡的统一布局：绘制和鼠标命中必须用同一份坐标
static const Rectangle kHangarPanel{90, 168, 580, 574};
static constexpr float kSwatchSize = 54.0f;
static constexpr float kSwatchStep = 96.0f;

static Rectangle SkinSwatchRect(int i) {
    return {kHangarPanel.x + 110 + i * kSwatchStep, kHangarPanel.y + 470, kSwatchSize, kSwatchSize};
}

static Color SkinColor(int i) {
    switch (i) {
        case 0: return Color{70, 120, 220, 255};
        case 1: return Color{70, 190, 120, 255};
        case 2: return Color{240, 150, 60, 255};
        default: return Color{160, 100, 220, 255};
    }
}

HangarScreen::ResultData& HangarScreen::lastResult() {
    static ResultData d;
    return d;
}

HangarScreen::HangarScreen(Mode m) : mode_(m) {}

void HangarScreen::enter() {
    craft_ = clampi(save::data().craft, 0, 5);
    skin_ = clampi(save::data().skin, 0, 3);
    yaw_ = 60;
    t_ = 0;
    for (auto& a : appear_) a = 0;
    if (mode_ == Result) focus_ = 0;
}

void HangarScreen::changeCraft(int d) {
    int n = craft_ + d;
    if (n < 0) n = 5;
    if (n > 5) n = 0;
    if (n == craft_) return;
    craft_ = n;
    swapT_ = 0;
    for (auto& a : appear_) a = 0;
    audio::play(audio::SfxUiMove, 0.55f);
}

void HangarScreen::update(float dt) {
    t_ += dt;
    swapT_ = std::min(1.0f, swapT_ + dt * 5.0f);
    yaw_ += dt * 12.0f;

    // 属性条逐条增长
    for (int i = 0; i < 5; i++) {
        float target = clampf((swapT_ - i * 0.12f) / 0.35f, 0, 1);
        appear_[i] = std::max(appear_[i], target);
    }

    auto& in = app().in;

    if (mode_ == Result) {
        if (in.navLeft()) focus_ = 0;
        if (in.navRight()) focus_ = 1;
        if (in.back()) {
            app().go(Sid::Title);
            return;
        }
        if (in.ok()) {
            if (focus_ == 0) {
                audio::play(audio::SfxUiSelect, 0.8f);
                StartNewRun(false);
                app().go(Sid::Game);
            } else {
                audio::play(audio::SfxUiSelect, 0.8f);
                mode_ = Select;
                focus_ = 0;
            }
        }
        // 鼠标按钮
        Rectangle b1{470, 780, 300, 84};
        Rectangle b2{800, 780, 300, 84};
        if (ui::Hover(b1)) focus_ = 0;
        if (ui::Hover(b2)) focus_ = 1;
        if (ui::Hover(b1) && in.lmbPressed) {
            audio::play(audio::SfxUiSelect, 0.8f);
            StartNewRun(false);
            app().go(Sid::Game);
        }
        if (ui::Hover(b2) && in.lmbPressed) {
            audio::play(audio::SfxUiSelect, 0.8f);
            mode_ = Select;
        }
        return;
    }

    // 选择模式
    if (in.navLeft()) changeCraft(-1);
    if (in.navRight()) changeCraft(1);

    // 换色
    if (in.pressed(KEY_C) || in.pressed(KEY_TAB)) {
        skin_ = (skin_ + 1) % 4;
        save::data().skin = skin_;
        save::save();
        audio::play(audio::SfxUiMove, 0.6f);
    }

    // 卡片鼠标
    float stripX = 90, cardW = 212, gap = 14, cardY = 828, cardH = 118;
    for (int i = 0; i < 6; i++) {
        Rectangle r{stripX + i * (cardW + gap), cardY, cardW, cardH};
        if (ui::Hover(r)) {
            if (craft_ != i) changeCraft(i - craft_ > 0 ? 1 : -1), craft_ = i;
            if (in.lmbPressed) {
                if (assets::players[i].unlocked) {
                    save::data().craft = i;
                    save::save();
                }
            }
        }
    }
    // 色卡鼠标（坐标与 drawStatPanel 里的绘制完全一致）
    for (int i = 0; i < 4; i++) {
        if (ui::Hover(SkinSwatchRect(i)) && in.lmbPressed) {
            skin_ = i;
            save::data().skin = skin_;
            save::save();
            audio::play(audio::SfxUiMove, 0.6f);
        }
    }

    bool unlocked = assets::players[craft_].unlocked;
    if (in.ok()) {
        if (unlocked) {
            audio::play(audio::SfxUiSelect, 0.9f);
            app().pushOverlay(std::make_unique<BriefingOverlay>());
        } else {
            audio::play(audio::SfxHit, 0.4f, 0.55f);
        }
    }
    if (in.back()) app().go(Sid::Title);

    Rectangle go{1470, 828, 360, 118};
    if (ui::Hover(go) && in.lmbPressed) {
        if (unlocked) {
            audio::play(audio::SfxUiSelect, 0.9f);
            app().pushOverlay(std::make_unique<BriefingOverlay>());
        } else {
            audio::play(audio::SfxHit, 0.4f, 0.55f);
        }
    }
    Rectangle backBtn{56, 44, 150, 62};
    if (ui::Hover(backBtn) && in.lmbPressed) app().go(Sid::Title);
}

void HangarScreen::drawWorld() {
    if (assets::hangarBg.id) DrawTexture(assets::hangarBg, 0, 0, WHITE);
    // 机体在升降台上的投影
    if (mode_ != Result || true) {
        float k = 1.0f + std::sin(t_ * 1.6f) * 0.02f;
        DrawEllipse(1352, 690, (int)(300 * k), (int)(120 * k), WithAlpha(BLACK, 0.26f));
        DrawEllipse(1352, 690, (int)(220 * k), (int)(88 * k), WithAlpha(BLACK, 0.3f));
    }
    // 顶部压暗，保证文字可读
    DrawRectangleGradientV(0, 0, (int)VW, 260, WithAlpha(Color{4, 8, 14, 255}, 0.72f), BLANK);
    DrawRectangleGradientV(0, (int)(VH - 300), (int)VW, 300, BLANK, WithAlpha(Color{4, 8, 14, 255}, 0.88f));
}

void HangarScreen::draw3D() {
    const auto& c = assets::players[craft_];
    if (!c.loaded) return;
    cameraRig().begin();
    float bob = std::sin(t_ * 1.6f) * 7.0f;
    // 复用同一套姿态系统：机库里"选一个角度"即可，不再单独写死旋转
    float showcaseRoll = -0.14f + std::sin(t_ * 0.5f) * 0.05f;
    pose::Pose p = pose::FromRoll(showcaseRoll);
    assets::drawCraft(c, 1338, 658 + bob, skin_, p, 620.0f, WHITE, 1);
    cameraRig().end();
}

void HangarScreen::drawStatPanel() {
    const auto& c = assets::players[craft_];
    const Rectangle& r = kHangarPanel;
    ui::Panel(r, WithAlpha(Color{6, 12, 22, 255}, 0.93f), WithAlpha(pal::kCyan, 0.6f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.28f), 14);

    ui::Text(c.id, r.x + 30, r.y + 26, 42, pal::kText, true);
    ui::Text(c.role, r.x + 30, r.y + 82, 24, pal::kCyan);
    DrawRectangle((int)r.x + 30, (int)r.y + 122, (int)r.width - 60, 2, WithAlpha(pal::kCyan, 0.22f));

    for (int i = 0; i < 5; i++) {
        float y = r.y + 142 + i * 54;
        ui::Text(kStatName[i], r.x + 30, y + 2, 24, WithAlpha(pal::kDim, 0.95f));
        ui::StatBar(r.x + 130, y + 6, 380, 18, (c.stat[i] / 10.0f) * appear_[i],
                    appear_[i] > 0.99f ? pal::kCyan : WithAlpha(pal::kCyan, 0.9f), WithAlpha(WHITE, 0.09f));
        char buf[8];
        snprintf(buf, sizeof buf, "%d", c.stat[i]);
        ui::TextR(buf, r.x + r.width - 24, y, 22, WithAlpha(pal::kDim, 0.8f));
    }

    ui::Text(c.desc, r.x + 30, r.y + 424, 21, WithAlpha(pal::kDim, 0.95f));

    // 换色
    DrawRectangle((int)r.x + 30, (int)r.y + 452, (int)r.width - 60, 1, WithAlpha(WHITE, 0.08f));
    ui::Text("换色", r.x + 30, r.y + 484, 22, WithAlpha(pal::kDim, 0.9f));
    for (int i = 0; i < 4; i++) {
        Rectangle sr = SkinSwatchRect(i);
        bool hov = ui::Hover(sr);
        Color col = SkinColor(i);
        DrawRectangle((int)sr.x, (int)sr.y, (int)sr.width, (int)sr.height, WithAlpha(col, hov ? 1.0f : 0.86f));
        DrawRectangle((int)sr.x, (int)sr.y, (int)sr.width, 6, WithAlpha(WHITE, 0.16f));
        if (skin_ == i) {
            DrawRectangleLinesEx({sr.x - 4, sr.y - 4, sr.width + 8, sr.height + 8}, 3, pal::kCyan);
            ui::Triangle(sr.x + sr.width * 0.5f, sr.y + sr.height * 0.5f, 18, 1, Color{255, 255, 255, 235});
        } else if (hov) {
            DrawRectangleLinesEx(sr, 2, WithAlpha(pal::kCyan, 0.55f));
        }
        ui::TextC(kSkinTag[i], sr.x + sr.width * 0.5f, sr.y + sr.height + 7, 12, WithAlpha(pal::kDim, 0.8f));
    }
}

void HangarScreen::drawStrip() {
    float stripX = 90, cardW = 212, gap = 14, cardY = 828, cardH = 118;
    for (int i = 0; i < 6; i++) {
        Rectangle r{stripX + i * (cardW + gap), cardY, cardW, cardH};
        bool sel = (craft_ == i);
        bool unlocked = assets::players[i].unlocked;
        Color accent = sel ? pal::kCyan : WithAlpha(pal::kDim, 0.35f);
        ui::Panel(r, sel ? WithAlpha(Color{10, 32, 44, 255}, 0.92f) : WithAlpha(Color{6, 12, 22, 255}, 0.86f), accent, 10,
                  sel ? 2.5f : 1.5f);
        Color tc = !unlocked ? WithAlpha(pal::kDim, 0.45f) : (sel ? pal::kText : pal::kDim);
        ui::TextC(assets::players[i].id, r.x + r.width * 0.5f, r.y + 26, 24, tc, sel);

        char buf[16];
        snprintf(buf, sizeof buf, "%02d", i + 1);
        ui::Text(buf, r.x + 14, r.y + 10, 18, WithAlpha(pal::kDim, 0.6f));

        if (!unlocked) {
            // 锁
            float lx = r.x + r.width * 0.5f, ly = r.y + 76;
            DrawRectangle((int)lx - 12, (int)ly - 4, 24, 20, WithAlpha(pal::kDim, 0.5f));
            DrawRectangleLinesEx({lx - 7, ly - 16, 14, 14}, 3, WithAlpha(pal::kDim, 0.5f));
        } else if (save::data().craft == i) {
            ui::Triangle(r.x + r.width - 22, r.y + 22, 14, 1, pal::kCyan);
        }
        if (sel) {
            ui::TextC(assets::players[i].role, r.x + r.width * 0.5f, r.y + 74, 20,
                      unlocked ? WithAlpha(pal::kCyan, 0.85f) : WithAlpha(pal::kDim, 0.45f));
        }
    }
}

void HangarScreen::drawResultPanel() {
    auto& d = lastResult();
    Rectangle r{90, 178, 700, 540};
    ui::Panel(r, WithAlpha(pal::kPanelBg, 0.92f), WithAlpha(pal::kCyan, 0.6f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 14);

    ui::TextC(d.win ? "任务完成" : "任务失败", r.x + r.width * 0.5f, r.y + 34, 52,
              d.win ? pal::kText : pal::kDanger, true);

    // 星：逐个弹出
    for (int i = 0; i < 3; i++) {
        float t = clampf((t_ - 0.35f - i * 0.2f) / 0.22f, 0, 1);
        float sx = r.x + r.width * 0.5f + (i - 1) * 128;
        float sy = r.y + 158;
        if (t <= 0) continue;
        float s = 1.0f + (1.0f - easeOutBack(t)) * 0.9f;
        if (i < d.stars) {
            ui::Star(sx, sy, 38 * s, WithAlpha(pal::kGold, clampf(t * 2, 0, 1)), true);
        } else {
            ui::Star(sx, sy, 38, WithAlpha(pal::kDim, 0.35f), false);
        }
    }

    float y = r.y + 250;
    char buf[64];
    auto row = [&](const char* k, const char* v) {
        ui::Text(k, r.x + 70, y, 26, WithAlpha(pal::kDim, 0.9f));
        ui::TextR(v, r.x + r.width - 70, y, 26, pal::kText);
        DrawRectangle((int)r.x + 70, (int)y + 40, (int)r.width - 140, 1, WithAlpha(WHITE, 0.06f));
        y += 58;
    };
    snprintf(buf, sizeof buf, "%d", d.kills);
    row("击毁", buf);
    snprintf(buf, sizeof buf, "%07d", d.score);
    row("得分", buf);
    snprintf(buf, sizeof buf, "x%d", d.maxCombo);
    row("最大连击", buf);
    snprintf(buf, sizeof buf, "%d:%02d", (int)d.time / 60, (int)d.time % 60);
    row("用时", buf);

    if (d.newRecord) {
        float p = ui::Pulse(1.6f);
        ui::TextC("新 最 高 分", r.x + r.width * 0.5f, r.y + 486, 30, WithAlpha(pal::kGold, 0.55f + 0.45f * p), true);
    }
}

void HangarScreen::draw() {
    // 返回
    Rectangle backBtn{56, 44, 150, 62};
    ui::Button(backBtn, "返回", ui::Hover(backBtn), true, 26);

    ui::TextR("机    型", VW - 56, 52, 30, WithAlpha(pal::kDim, 0.55f), true);

    if (mode_ == Result) {
        drawResultPanel();
        Rectangle b1{470, 780, 300, 84};
        Rectangle b2{800, 780, 300, 84};
        ui::Button(b1, "下一关", focus_ == 0, true, 30);
        ui::Button(b2, "选择战机", focus_ == 1, true, 30);
        ui::TextC("左右键 选择   ENTER 确认   ESC 返回", VW * 0.5f, VH - 60, 20, WithAlpha(pal::kDim, 0.5f));
        return;
    }

    drawStatPanel();
    drawStrip();

    const auto& c = assets::players[craft_];
    bool unlocked = c.unlocked;

    // 出击按钮
    Rectangle go{1470, 828, 360, 118};
    bool hov = ui::Hover(go);
    ui::Panel(go, unlocked ? WithAlpha(pal::kCyan, hov ? 0.32f : 0.2f) : WithAlpha(WHITE, 0.04f),
              unlocked ? pal::kCyan : WithAlpha(pal::kDim, 0.4f), 12, 2.5f);
    ui::TextC("出    击", go.x + go.width * 0.5f - 18, go.y + 34, 38,
              unlocked ? pal::kText : WithAlpha(pal::kDim, 0.5f), true);
    if (unlocked) ui::Triangle(go.x + go.width - 54, go.y + go.height * 0.5f, 26, 1, pal::kCyan);

    if (!unlocked) {
        float bx = 1470, by = 700;
        ui::Panel({bx, by, 360, 104}, WithAlpha(pal::kPanelBg, 0.85f), WithAlpha(pal::kDanger, 0.55f), 10, 2);
        ui::TextC("已 锁 定", bx + 180, by + 14, 26, WithAlpha(pal::kDanger, 0.9f), true);
        ui::TextC("击毁 40 个后开启", bx + 180, by + 56, 20, WithAlpha(pal::kDim, 0.9f));
    }

    ui::TextC("左右键 换机体    C 换色    ENTER 出击    ESC 返回", VW * 0.5f, VH - 56, 20,
              WithAlpha(pal::kDim, 0.5f));
}

// ---------------------------------------------------------------- 简报
// 简报面板从右侧滑入，绘制与命中共用同一个 x
static float BriefingSlideX(float t) { return lerpf(VW, VW - 520, easeOutCubic(clampf(t / 0.25f, 0, 1))); }

void BriefingOverlay::update(float dt) {
    t_ += dt;
    auto& in = app().in;
    const float x = BriefingSlideX(t_);
    const Rectangle goBtn{x + 56, 764, 408, 96};
    if (in.back()) {
        app().popOverlay();
        return;
    }
    if (in.ok() || (ui::Hover(goBtn) && in.lmbPressed)) {
        audio::play(audio::SfxUiSelect, 0.9f);
        app().clearOverlays();
        StartNewRun(false);
        app().go(Sid::Game);
    }
}

void BriefingOverlay::draw() {
    ui::Dim(0.46f, Color{4, 8, 14, 255});

    float x = BriefingSlideX(t_);
    DrawRectangle((int)x, 0, 520, (int)VH, WithAlpha(Color{6, 12, 22, 255}, 0.95f));
    DrawRectangle((int)x, 0, 4, (int)VH, WithAlpha(pal::kCyan, 0.75f));
    DrawRectangleGradientV((int)x, 0, 520, 220, WithAlpha(pal::kCyan, 0.08f), BLANK);

    ui::Text("出击简报", x + 56, 96, 44, pal::kText, true);
    ui::Text("远洋巡航", x + 56, 156, 24, WithAlpha(pal::kCyan, 0.85f));
    DrawRectangle((int)x + 56, (int)208, 408, 2, WithAlpha(pal::kCyan, 0.25f));

    ui::Text("任务", x + 56, 244, 22, WithAlpha(pal::kCyan, 0.9f), true);
    ui::Text("击毁 40 个敌机，", x + 56, 284, 24, WithAlpha(pal::kText, 0.92f));
    ui::Text("并击破雷暴要塞。", x + 56, 320, 24, WithAlpha(pal::kText, 0.92f));

    ui::Text("标准", x + 56, 392, 22, WithAlpha(pal::kCyan, 0.9f), true);
    ui::Text("击毁 40 个", x + 56, 430, 21, WithAlpha(pal::kDim, 0.95f));
    ui::Text("生命 2 个以上", x + 56, 462, 21, WithAlpha(pal::kDim, 0.95f));
    for (int i = 0; i < 3; i++) ui::Star(x + 74 + i * 52, 530, 19, pal::kGold, true);

    ui::Text("机体", x + 56, 600, 22, WithAlpha(pal::kCyan, 0.9f), true);
    ui::Text(assets::players[clampi(save::data().craft, 0, 5)].id, x + 56, 638, 32, pal::kText, true);
    ui::Text(assets::players[clampi(save::data().craft, 0, 5)].role, x + 56, 682, 21,
             WithAlpha(pal::kDim, 0.9f));

    Rectangle go{x + 56, 764, 408, 96};
    bool hov = ui::Hover(go);
    ui::Panel(go, WithAlpha(pal::kCyan, hov ? 0.34f : 0.22f), pal::kCyan, 10, 2.5f);
    ui::TextC("出 发", go.x + go.width * 0.5f - 16, go.y + 28, 36, pal::kText, true);
    ui::Triangle(go.x + go.width - 52, go.y + go.height * 0.5f, 24, 1, pal::kCyan);

    ui::TextC("ENTER 出发      ESC 返回", x + 260, 900, 20, WithAlpha(pal::kDim, 0.6f));
}

}  // namespace sky
