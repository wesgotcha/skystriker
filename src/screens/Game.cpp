// GAMEPLAY 战斗屏 + 暂停 / 阵亡 + 屏幕工厂
#include "Screens.hpp"

#include "App.hpp"
#include "Assets.hpp"
#include "Audio.hpp"
#include "Save.hpp"
#include "Ui.hpp"
#include "World.hpp"

#include <cstdio>

namespace sky {

int g_hangarMode = 0;  // 0 = 选机, 1 = 结算
bool g_forceResult = false;

static World  g_world;
static bool   g_continue = false;

void StartNewRun(bool continueRun) { g_continue = continueRun; }

// ---------------------------------------------------------------- 战斗屏
void GameScreen::enter() {
    g_world.start(g_continue);
    t_ = 0;
}

void GameScreen::update(float dt) {
    t_ += dt;
    auto& in = app().in;

    if (!g_world.finished()) {
        if (in.back()) {
            audio::play(audio::SfxUiSelect, 0.6f, 1.2f);
            app().pushOverlay(std::make_unique<PauseOverlay>());
            return;
        }
        g_world.update(dt, app().demoMode);
        if (g_world.finished()) {
            auto& d = HangarScreen::lastResult();
            d.kills = g_world.kills();
            d.score = g_world.score();
            d.maxCombo = g_world.maxCombo();
            d.time = g_world.elapsed();
            d.stars = g_world.stars();
            d.win = g_world.cleared();

            auto& p = save::data();
            p.runs++;
            p.kills += g_world.kills();
            p.lastScore = g_world.score();
            d.newRecord = g_world.score() > p.hiScore;
            if (d.newRecord) p.hiScore = g_world.score();
            int si = 0;
            if (g_world.cleared()) {
                p.stars[si] = std::max(p.stars[si], d.stars);
                p.stageScore[si] = std::max(p.stageScore[si], g_world.score());
                p.stageKills[si] = std::max(p.stageKills[si], g_world.kills());
                p.stageTime[si] = (int)g_world.elapsed();
            }
            save::save();

            if (g_world.cleared()) {
                g_hangarMode = 1;
                app().go(Sid::Hangar);
            } else {
                app().pushOverlay(std::make_unique<GameOverOverlay>());
            }
        }
    }
}

void GameScreen::drawWorld() { g_world.drawBackground(); }

std::string GameDebugInfo() { return g_world.debugInfo(); }

void GameDebugForceFail() { g_world.killPlayerNow(); }

void GameScreen::draw3D() {
    cameraRig().begin();
    g_world.draw3D();
    cameraRig().end();
}

void GameScreen::draw() {
    g_world.drawFore();
    g_world.drawHud();
}

// ---------------------------------------------------------------- 暂停
void PauseOverlay::update(float dt) {
    t_ += dt;
    auto& in = app().in;
    if (in.back()) {
        app().popOverlay();
        audio::play(audio::SfxUiMove, 0.5f);
        return;
    }
    if (in.navUp()) { focus_ = (focus_ + 4) % 5; audio::play(audio::SfxUiMove, 0.5f); }
    if (in.navDown()) { focus_ = (focus_ + 1) % 5; audio::play(audio::SfxUiMove, 0.5f); }

    float by = VH * 0.5f - 130;
    for (int i = 0; i < 5; i++) {
        Rectangle r{VW * 0.5f - 230, by + i * 74, 460, 62};
        if (ui::Hover(r)) {
            if (focus_ != i) audio::play(audio::SfxUiMove, 0.4f);
            focus_ = i;
        }
    }
    bool click = false;
    Rectangle rr{VW * 0.5f - 230, by + focus_ * 74, 460, 62};
    if (ui::Hover(rr) && in.lmbPressed) click = true;

    if (in.ok() || click) {
        audio::play(audio::SfxUiSelect, 0.8f);
        switch (focus_) {
            case 0: app().popOverlay(); break;
            case 1:
                app().clearOverlays();
                g_hangarMode = 0;
                app().go(Sid::Hangar);
                break;
            case 2: app().pushOverlay(std::make_unique<SettingsOverlay>()); break;
            case 3:
                app().pushOverlay(std::make_unique<ConfirmOverlay>("重开关卡", "当前进度不保存，重开。", []() {
                    app().clearOverlays();
                    g_continue = false;
                    app().go(Sid::Game);
                }));
                break;
            default:
                app().pushOverlay(std::make_unique<ConfirmOverlay>("返回主菜单", "当前进度不保存，返回主菜单。", []() {
                    app().clearOverlays();
                    app().go(Sid::Title);
                }));
                break;
        }
    }
}

void PauseOverlay::draw() {
    ui::Dim(0.62f, Color{4, 8, 14, 255});
    Rectangle r{VW * 0.5f - 300, VH * 0.5f - 300, 600, 600};
    ui::Panel(r, Color{6, 12, 22, 255}, WithAlpha(pal::kCyan, 0.65f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 14);
    ui::TextC("PAUSED", VW * 0.5f, r.y + 40, 52, pal::kText, true);
    ui::TextC("暂    停", VW * 0.5f, r.y + 104, 22, WithAlpha(pal::kDim, 0.75f));
    ui::Rect({r.x + 60, r.y + 148, r.width - 120, 2}, WithAlpha(pal::kCyan, 0.2f));

    const char* items[5] = {"继    续", "选择战机", "设    置", "重开关卡", "返回主菜单"};
    float by = VH * 0.5f - 130;
    for (int i = 0; i < 5; i++) {
        Rectangle rr{VW * 0.5f - 230, by + i * 74, 460, 62};
        ui::Button(rr, items[i], focus_ == i, true, 30);
    }
    ui::TextC("ESC 返回    上下键 选择", VW * 0.5f, r.y + r.height - 52, 20, WithAlpha(pal::kDim, 0.55f));
}

// ---------------------------------------------------------------- 阵亡
void GameOverOverlay::update(float dt) {
    t_ += dt;
    auto& in = app().in;
    newRecord_ = save::data().lastScore >= save::data().hiScore && save::data().lastScore > 0;
    if (t_ < 1.2f) return;
    if (in.pressed(KEY_R) || (in.ok() && focus_ == 0)) {
        audio::play(audio::SfxUiSelect, 0.8f);
        app().clearOverlays();
        g_continue = false;
        app().go(Sid::Game);
        return;
    }
    if (in.pressed(KEY_H) || in.back() || (in.ok() && focus_ == 1)) {
        audio::play(audio::SfxUiSelect, 0.8f);
        app().clearOverlays();
        g_hangarMode = 0;
        app().go(Sid::Hangar);
        return;
    }
    Rectangle b1{VW * 0.5f - 320, 720, 280, 84};
    Rectangle b2{VW * 0.5f + 40, 720, 280, 84};
    if (ui::Hover(b1)) focus_ = 0;
    if (ui::Hover(b2)) focus_ = 1;
    if (ui::Hover(b1) && in.lmbPressed) {
        app().clearOverlays();
        g_continue = false;
        app().go(Sid::Game);
    }
    if (ui::Hover(b2) && in.lmbPressed) {
        app().clearOverlays();
        g_hangarMode = 0;
        app().go(Sid::Hangar);
    }
}

void GameOverOverlay::draw() {
    DrawRectangle(0, 0, (int)VW, (int)VH, WithAlpha(Color{80, 19, 19, 255}, 0.45f));
    ui::Dim(0.45f, Color{8, 4, 4, 255});

    const char* txt = "GAME OVER";
    int n = 9;
    float totalW = ui::TextW(txt, 96, true);
    float x0 = VW * 0.5f - totalW * 0.5f;
    for (int i = 0; i < n; i++) {
        float at = clampf((t_ - i * 0.08f) / 0.18f, 0, 1);
        if (at <= 0) continue;
        char ch[2] = {txt[i], 0};
        float x = x0 + ui::TextW(std::string(txt, i), 96, true);
        float jitter = (at < 1.0f) ? (float)GetRandomValue(-3, 3) : 0.0f;
        ui::Text(ch, x, 320 + jitter, 96, WithAlpha(pal::kText, at), true);
    }

    if (t_ > 0.9f) {
        char buf[64];
        snprintf(buf, sizeof buf, "得分   %07d", save::data().lastScore);
        ui::TextC(buf, VW * 0.5f, 500, 34, pal::kText);
        snprintf(buf, sizeof buf, "最高   %07d", save::data().hiScore);
        ui::TextC(buf, VW * 0.5f, 556, 28, WithAlpha(pal::kDim, 0.9f));
        if (newRecord_) {
            float p = ui::Pulse(2.2f);
            ui::TextC("新 最 高 分", VW * 0.5f, 612, 28, WithAlpha(pal::kGold, 0.5f + 0.5f * p), true);
        }
    }

    if (t_ > 1.2f) {
        ui::Button({VW * 0.5f - 320, 720, 280, 84}, "重试  [R]", focus_ == 0, true, 30);
        ui::Button({VW * 0.5f + 40, 720, 280, 84}, "选择战机  [H]", focus_ == 1, true, 30);
    }
}

// ---------------------------------------------------------------- 工厂
std::unique_ptr<Screen> MakeOverlay(const std::string& n) {
    if (n == "pause") return std::make_unique<PauseOverlay>();
    if (n == "settings") return std::make_unique<SettingsOverlay>();
    if (n == "records") return std::make_unique<RecordsOverlay>();
    if (n == "briefing") return std::make_unique<BriefingOverlay>();
    if (n == "gameover") return std::make_unique<GameOverOverlay>();
    if (n == "confirm")
        return std::make_unique<ConfirmOverlay>("退出游戏", "确定要退出游戏。", []() {});
    return nullptr;
}

std::unique_ptr<Screen> MakeScreen(Sid id) {
    switch (id) {
        case Sid::Boot: return std::make_unique<BootScreen>();
        case Sid::Title: return std::make_unique<TitleScreen>();
        case Sid::Hangar: {
            auto hs = std::make_unique<HangarScreen>((g_hangarMode == 1 || g_forceResult) ? HangarScreen::Result
                                                                                         : HangarScreen::Select);
            if (g_forceResult) {
                auto& d = HangarScreen::lastResult();
                d.kills = 38; d.score = 184320; d.maxCombo = 24; d.time = 134.0f;
                d.stars = 3; d.win = true; d.newRecord = true;
            }
            return hs;
        }
        case Sid::Game: return std::make_unique<GameScreen>();
    }
    return nullptr;
}

}  // namespace sky
