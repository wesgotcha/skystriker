// TITLE 主菜单 + 排行 / 设置 / 确认框
#include "Screens.hpp"

#include "App.hpp"
#include "Assets.hpp"
#include "Audio.hpp"
#include "Save.hpp"
#include "Ui.hpp"

#include <cstdio>

namespace sky {

namespace {
const char* kMenu[5] = {"开始游戏", "继续游戏", "排    行", "设    置", "退    出"};
}

void TitleScreen::enter() {
    bg_.setup(Backdrop::Dusk);
    t_ = 0;
    focus_ = 0;
}

void TitleScreen::activate(int idx) {
    switch (idx) {
        case 0:
            audio::play(audio::SfxUiSelect, 0.8f);
            app().go(Sid::Hangar);
            break;
        case 1:
            if (save::data().runs <= 0) {
                audio::play(audio::SfxHit, 0.4f, 0.6f);
                break;
            }
            audio::play(audio::SfxUiSelect, 0.8f);
            StartNewRun(true);
            app().go(Sid::Game);
            break;
        case 2:
            audio::play(audio::SfxUiSelect, 0.8f);
            app().pushOverlay(std::make_unique<RecordsOverlay>());
            break;
        case 3:
            audio::play(audio::SfxUiSelect, 0.8f);
            app().pushOverlay(std::make_unique<SettingsOverlay>());
            break;
        default:
            audio::play(audio::SfxUiSelect, 0.8f);
            app().pushOverlay(std::make_unique<ConfirmOverlay>(
                "退出游戏", "确定要退出游戏。", []() { app().requestExit = true; }));
            break;
    }
}

void TitleScreen::update(float dt) {
    t_ += dt;
    bg_.update(dt);

    bool canContinue = save::data().runs > 0;
    int prev = focus_;

    if (app().in.navUp()) {
        focus_ = (focus_ + 4) % 5;
        mouseMode_ = false;
    }
    if (app().in.navDown()) {
        focus_ = (focus_ + 1) % 5;
        mouseMode_ = false;
    }
    if (focus_ == 1 && !canContinue && app().in.navUp()) focus_ = 0;
    if (focus_ == 1 && !canContinue && app().in.navDown()) focus_ = 2;
    if (focus_ != prev) audio::play(audio::SfxUiMove, 0.5f);

    // 鼠标
    float startY = 536.0f, lh = 72.0f;
    for (int i = 0; i < 5; i++) {
        Rectangle r{VW * 0.5f - 260, startY + i * lh - 6, 520, lh - 8};
        if (ui::Hover(r)) {
            if (focus_ != i) {
                focus_ = i;
                audio::play(audio::SfxUiMove, 0.5f);
            }
            mouseMode_ = true;
            if (app().in.lmbPressed) activate(i);
        }
    }

    if (app().in.ok() && !app().hasOverlay()) activate(focus_);
}

void TitleScreen::drawWorld() { bg_.draw(); }

void TitleScreen::draw() {
    // 菜单区域压暗，保证文字在亮云上可读
    DrawRectangleGradientV(0, 470, (int)VW, 460, BLANK, WithAlpha(BLACK, 0.55f));
    DrawRectangleGradientV(0, 470, (int)VW, 200, BLANK, WithAlpha(BLACK, 0.3f));
    DrawRectangleGradientV(0, 0, (int)VW, 130, WithAlpha(BLACK, 0.45f), BLANK);

    // 顶部信息
    char buf[64];
    snprintf(buf, sizeof buf, "HI-SCORE %07d", save::data().hiScore);
    ui::Text(buf, 56, 44, 22, WithAlpha(pal::kCyan, 0.85f));
    ui::TextR("v0.1.0", VW - 56, 44, 22, WithAlpha(pal::kDim, 0.7f));

    // Logo：从上方落下（4 帧阶梯）
    float drop = stepped(clampf(t_ / 0.55f, 0, 1), 4);
    float ly = lerpf(120.0f, 180.0f, drop);
    float lw = 900.0f, lgh = 900.0f * (437.0f / 1299.0f);
    if (assets::logoTex.id) {
        Rectangle src{0, 0, (float)assets::logoTex.width, (float)assets::logoTex.height};
        DrawTexturePro(assets::logoTex, src, {VW * 0.5f - lw * 0.5f, ly, lw, lgh}, {0, 0}, 0, WHITE);
    }

    // 菜单
    float startY = 536.0f, lh = 72.0f;
    bool canContinue = save::data().runs > 0;
    for (int i = 0; i < 5; i++) {
        bool sel = (focus_ == i);
        bool enabled = !(i == 1 && !canContinue);
        float y = startY + i * lh;
        float float_ = sel ? std::sin((float)GetTime() * 5.2f) * 4.0f : 0.0f;
        float tx = VW * 0.5f + float_;
        Color c = !enabled ? Color{96, 108, 122, 255} : (sel ? pal::kCyan : Color{186, 202, 218, 255});
        if (sel) {
            // 选中发光：三层同色描边字
            for (int k = 3; k >= 1; k--)
                ui::TextC(kMenu[i], tx, y + 2, 40 + k, WithAlpha(pal::kCyan, 0.10f), true);
            ui::Triangle(tx - ui::TextW(kMenu[i], 40, true) * 0.5f - 46 + float_ * 0.3f, y + 26, 20, 1,
                         WithAlpha(pal::kCyan, 0.9f));
            ui::Triangle(tx + ui::TextW(kMenu[i], 40, true) * 0.5f + 46 - float_ * 0.3f, y + 26, 20, -1,
                         WithAlpha(pal::kCyan, 0.9f));
        }
        ui::TextShadow(kMenu[i], tx - ui::TextW(kMenu[i], 40, sel) * 0.5f, y, 40, c, WithAlpha(BLACK, 0.6f), sel);
    }

    // 底部提示
    ui::TextC("上下键 选择     ENTER 确认", VW * 0.5f, VH - 74, 20, WithAlpha(pal::kDim, 0.55f));
    ui::TextC("2.5D 空战弹幕", VW * 0.5f, VH - 44, 18, WithAlpha(pal::kDim, 0.4f));
}

// ---------------------------------------------------------------- 确认框
ConfirmOverlay::ConfirmOverlay(std::string title, std::string body, std::function<void()> onYes,
                               std::string yes, std::string no)
    : title_(std::move(title)), body_(std::move(body)), yes_(std::move(yes)), no_(std::move(no)),
      onYes_(std::move(onYes)) {}

void ConfirmOverlay::update(float dt) {
    t_ += dt;
    if (app().in.navLeft()) { focus_ = 0; audio::play(audio::SfxUiMove, 0.5f); }
    if (app().in.navRight()) { focus_ = 1; audio::play(audio::SfxUiMove, 0.5f); }
    if (app().in.back()) {
        app().popOverlay();
        return;
    }
    if (app().in.ok()) {
        audio::play(audio::SfxUiSelect, 0.8f);
        if (focus_ == 1) {
            auto cb = onYes_;
            app().popOverlay();
            if (cb) cb();
        } else {
            app().popOverlay();
        }
    }
}

void ConfirmOverlay::draw() {
    ui::Dim(0.62f, Color{4, 8, 14, 255});
    Rectangle r{VW * 0.5f - 280, VH * 0.5f - 150, 560, 300};
    ui::Panel(r, WithAlpha(Color{6, 12, 22, 255}, 0.96f), WithAlpha(pal::kCyan, 0.7f), 12, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.35f), 12);
    ui::TextC(title_, VW * 0.5f, r.y + 52, 36, pal::kText, true);
    ui::TextC(body_, VW * 0.5f, r.y + 126, 24, WithAlpha(pal::kDim, 0.95f));
    Rectangle b1{r.x + 76, r.y + 200, 180, 66};
    Rectangle b2{r.x + 304, r.y + 200, 180, 66};
    if (ui::Button(b1, no_, focus_ == 0, true, 26)) { app().popOverlay(); }
    if (ui::Button(b2, yes_, focus_ == 1, true, 26)) {
        auto cb = onYes_;
        app().popOverlay();
        if (cb) cb();
    }
}

// ---------------------------------------------------------------- 排行
void RecordsOverlay::update(float dt) {
    (void)dt;
    if (app().in.back() || app().in.ok() || app().in.rmbPressed) app().popOverlay();
}

void RecordsOverlay::draw() {
    ui::Dim(0.66f, Color{4, 8, 14, 255});
    Rectangle r{VW * 0.5f - 560, 130, 1120, 800};
    ui::Panel(r, WithAlpha(pal::kPanelBg, 0.95f), WithAlpha(pal::kCyan, 0.6f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 14);

    ui::TextC("最 高 分 排 行", VW * 0.5f, r.y + 40, 48, pal::kText, true);
    DrawRectangle((int)r.x + 60, (int)r.y + 118, (int)r.width - 120, 2, WithAlpha(pal::kCyan, 0.25f));

    const char* names[3] = {"01  远洋巡航", "02  高空作战", "03  雷暴要塞"};
    float y = r.y + 156;
    for (int i = 0; i < 3; i++) {
        bool has = save::data().stageScore[i] > 0;
        ui::Text(names[i], r.x + 70, y, 32, has ? pal::kText : WithAlpha(pal::kDim, 0.5f), true);

        // 星
        for (int s = 0; s < 3; s++) {
            float sx = r.x + 430 + s * 46;
            if (s < save::data().stars[i])
                ui::Star(sx, y + 20, 17, pal::kGold, true);
            else
                ui::Star(sx, y + 20, 17, WithAlpha(pal::kDim, 0.35f), false);
        }

        char buf[64];
        snprintf(buf, sizeof buf, "%07d", save::data().stageScore[i]);
        ui::TextR(buf, r.x + 830, y, 30, has ? pal::kCyan : WithAlpha(pal::kDim, 0.4f));
        snprintf(buf, sizeof buf, "击毁 %d", save::data().stageKills[i]);
        ui::TextR(buf, r.x + 960, y, 26, WithAlpha(pal::kDim, 0.85f));
        int tt = save::data().stageTime[i];
        snprintf(buf, sizeof buf, "%d:%02d", tt / 60, tt % 60);
        ui::TextR(buf, r.x + 1060, y, 26, WithAlpha(pal::kDim, 0.85f));

        DrawRectangle((int)(r.x + 60), (int)(y + 62), (int)(r.width - 120), 1, WithAlpha(WHITE, 0.07f));
        y += 96;
    }

    // 汇总
    float sy = r.y + 470;
    DrawRectangle((int)r.x + 60, (int)sy, (int)r.width - 120, 2, WithAlpha(pal::kCyan, 0.25f));
    char buf[96];
    snprintf(buf, sizeof buf, "最高分  %07d", save::data().hiScore);
    ui::TextC(buf, VW * 0.5f, sy + 30, 34, pal::kGold, true);
    snprintf(buf, sizeof buf, "出击 %d 回      击毁 %d 个", save::data().runs, save::data().kills);
    ui::TextC(buf, VW * 0.5f, sy + 86, 24, WithAlpha(pal::kDim, 0.9f));

    ui::TextC("ESC / ENTER 返回", VW * 0.5f, r.y + r.height - 56, 20, WithAlpha(pal::kDim, 0.5f));
}

// ---------------------------------------------------------------- 设置
SettingsOverlay::SettingsOverlay() = default;

void SettingsOverlay::enter() {
    page_ = 0;
    row_ = 0;
    rebind_ = false;
}

void SettingsOverlay::apply() {
    auto& d = save::data();
    d.volMaster = app().volMaster;
    d.volMusic = app().volMusic;
    d.volSfx = app().volSfx;
    d.shake = app().optShake;
    d.reduceFlash = app().optReduceFlash;
    app().applyVolume();
    save::save();
}

void SettingsOverlay::update(float dt) {
    (void)dt;
    auto& in = app().in;
    if (rebind_) {
        int key = GetKeyPressed();
        if (key > 0 && key != KEY_ESCAPE) {
            if (rebindRow_ == 0) save::data().keyFire = key;
            else if (rebindRow_ == 1) save::data().keyBomb = key;
            rebind_ = false;
            save::save();
            audio::play(audio::SfxUiSelect, 0.7f);
        } else if (key == KEY_ESCAPE) {
            rebind_ = false;
        }
        return;
    }
    if (in.back()) {
        apply();
        app().popOverlay();
        return;
    }
    if (in.pressed(KEY_TAB) || in.pressed(KEY_Q)) {
        page_ = (page_ + 2) % 3;
        row_ = 0;
        audio::play(audio::SfxUiMove, 0.5f);
    }
    if (in.pressed(KEY_E)) {
        page_ = (page_ + 1) % 3;
        row_ = 0;
        audio::play(audio::SfxUiMove, 0.5f);
    }

    int rows = 3;
    if (in.navUp()) { row_ = (row_ + rows - 1) % rows; audio::play(audio::SfxUiMove, 0.4f); }
    if (in.navDown()) { row_ = (row_ + 1) % rows; audio::play(audio::SfxUiMove, 0.4f); }

    auto adjust = [&](int dir) {
        if (page_ == 0) {
            float* v = (row_ == 0) ? &app().volMaster : (row_ == 1 ? &app().volMusic : &app().volSfx);
            *v = clampf(*v + dir * 5.0f, 0, 100);
            app().applyVolume();
            apply();
        } else if (page_ == 1) {
            if (row_ == 0) app().optShake = !app().optShake;
            else if (row_ == 1) app().optReduceFlash = !app().optReduceFlash;
            else app().toggleFullscreen();
            apply();
        } else {
            if (row_ == 0) { rebind_ = true; rebindRow_ = 0; }
            else if (row_ == 1) { rebind_ = true; rebindRow_ = 1; }
        }
    };
    if (in.navLeft() || in.navRight()) adjust(in.navLeft() ? -1 : 1);
    if (in.ok() && page_ == 2) adjust(0);

    // 鼠标：三个分页按钮
    float tabW = 200, tabX = VW * 0.5f - 460 + 60;
    for (int i = 0; i < 3; i++) {
        Rectangle r{tabX + i * (tabW + 12), 262, tabW, 54};
        if (ui::Hover(r) && in.lmbPressed) {
            page_ = i;
            row_ = 0;
            audio::play(audio::SfxUiMove, 0.5f);
        }
    }
}

void SettingsOverlay::draw() {
    ui::Dim(0.7f, Color{4, 8, 14, 255});
    Rectangle r{VW * 0.5f - 460, 150, 920, 700};
    ui::Panel(r, WithAlpha(Color{6, 12, 22, 255}, 0.95f), WithAlpha(pal::kCyan, 0.6f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 14);

    ui::TextC("设    置", VW * 0.5f, r.y + 34, 44, pal::kText, true);

    const char* tabs[3] = {"音频", "画面", "按键"};
    float tabW = 200, tabX = r.x + 60;
    for (int i = 0; i < 3; i++) {
        Rectangle tr{tabX + i * (tabW + 12), r.y + 112, tabW, 54};
        bool sel = (page_ == i);
        ui::Panel(tr, sel ? WithAlpha(pal::kCyan, 0.18f) : WithAlpha(WHITE, 0.04f),
                  sel ? pal::kCyan : WithAlpha(pal::kDim, 0.3f), 8, 2);
        ui::TextC(tabs[i], tr.x + tr.width * 0.5f, tr.y + 10, 28, sel ? pal::kText : pal::kDim, sel);
    }
    DrawRectangle((int)r.x + 60, (int)r.y + 190, (int)r.width - 120, 2, WithAlpha(pal::kCyan, 0.16f));

    float y = r.y + 236;
    const float rowH = 104;
    auto rowBg = [&](int i) {
        if (row_ != i) return;
        DrawRectangle((int)r.x + 40, (int)(y + i * rowH - 16), (int)r.width - 80, (int)rowH - 18,
                      WithAlpha(pal::kCyan, 0.08f));
    };

    if (page_ == 0) {
        const char* labels[3] = {"主音量", "音  乐", "音  效"};
        float vals[3] = {app().volMaster, app().volMusic, app().volSfx};
        for (int i = 0; i < 3; i++) {
            rowBg(i);
            float ry = y + i * rowH;
            ui::Text(labels[i], r.x + 80, ry + 18, 30, row_ == i ? pal::kText : pal::kDim, row_ == i);
            float tx = r.x + 330, tw = 380;
            DrawRectangle((int)tx, (int)ry + 32, (int)tw, 10, WithAlpha(WHITE, 0.12f));
            DrawRectangle((int)tx, (int)ry + 32, (int)(tw * vals[i] / 100.0f), 10,
                          row_ == i ? pal::kCyan : WithAlpha(pal::kCyan, 0.55f));
            for (int k = 0; k <= 20; k++)
                DrawRectangle((int)(tx + tw * k / 20.0f), (int)ry + 28, 2, 18, WithAlpha(WHITE, 0.2f));
            char buf[16];
            snprintf(buf, sizeof buf, "%d", (int)vals[i]);
            ui::Text(buf, tx + tw + 40, ry + 12, 30, row_ == i ? pal::kGold : pal::kDim, true);
            if (row_ == i) {
                ui::Triangle(tx - 36, ry + 37, 16, -1, pal::kCyan);
                ui::Triangle(tx + tw + 100, ry + 37, 16, 1, pal::kCyan);
            }
        }
    } else if (page_ == 1) {
        const char* labels[3] = {"屏幕移动", "闪烁关闭", "全  屏"};
        bool vals[3] = {app().optShake, app().optReduceFlash, app().optFullscreen};
        for (int i = 0; i < 3; i++) {
            rowBg(i);
            float ry = y + i * rowH;
            ui::Text(labels[i], r.x + 80, ry + 18, 30, row_ == i ? pal::kText : pal::kDim, row_ == i);
            float tx = r.x + 380;
            ui::Panel({tx, ry + 6, 160, 56}, vals[i] ? WithAlpha(pal::kCyan, 0.22f) : WithAlpha(WHITE, 0.05f),
                      vals[i] ? pal::kCyan : WithAlpha(pal::kDim, 0.4f), 8, 2);
            ui::TextC(vals[i] ? "开" : "关", tx + 80, ry + 16, 30, vals[i] ? pal::kCyan : pal::kDim, true);
        }
    } else {
        const char* labels[3] = {"射  击", "爆  弹", "暂  停"};
        char keys[3][32];
        auto keyName = [](int k) -> const char* {
            const char* n = GetKeyName(k);
            return n ? n : "?";
        };
        snprintf(keys[0], 32, "%s", keyName(save::data().keyFire));
        snprintf(keys[1], 32, "%s", keyName(save::data().keyBomb));
        snprintf(keys[2], 32, "%s", "ESC / P");
        for (int i = 0; i < 3; i++) {
            rowBg(i);
            float ry = y + i * rowH;
            ui::Text(labels[i], r.x + 80, ry + 18, 30, row_ == i ? pal::kText : pal::kDim, row_ == i);
            bool editing = rebind_ && rebindRow_ == i;
            ui::Panel({r.x + 380, ry + 6, 260, 56},
                      editing ? WithAlpha(pal::kGold, 0.2f) : WithAlpha(WHITE, 0.05f),
                      editing ? pal::kGold : WithAlpha(pal::kDim, 0.4f), 8, 2);
            ui::TextC(editing ? "按任意键" : keys[i], r.x + 510, ry + 16, 26,
                      editing ? pal::kGold : pal::kText, true);
        }
    }

    ui::TextC("Q / E 换面板    上下键 选择    左右键 调整    ESC 返回", VW * 0.5f, r.y + r.height - 52, 20,
              WithAlpha(pal::kDim, 0.6f));
}

}  // namespace sky
