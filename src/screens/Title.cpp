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
const char* kMenu[6] = {"开始游戏", "继续游戏", "作战复盘", "排    行", "设    置", "退    出"};
constexpr int   kMenuCount = 6;
constexpr float kMenuTop = 512.0f;
constexpr float kMenuLH = 68.0f;

// 菜单项的命中矩形只写一份：绘制与点击共用，避免"看得见点不到"
Rectangle MenuItemRect(int i) { return {VW * 0.5f - 260, kMenuTop + i * kMenuLH - 6, 520, kMenuLH - 8}; }
}  // namespace

void TitleScreen::enter() {
    bg_.setup(Backdrop::Dusk);
    t_ = 0;
    focus_ = 0;
    mouseMode_ = false;

    // 刚从启动屏过来：画面还是全黑、logo 停在启动屏那个位置，从这里接着演入场。
    // 其它入口（机库返回 / 调试直达 --screen=title）直接就位，不重播开场。
    intro_ = app().introPending ? 0.0f : 1.0f;
    app().introPending = false;
    bg_.setAlpha(intro_ >= 1.0f ? 1.0f : 0.0f);
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
            app().pushOverlay(std::make_unique<HistoryOverlay>());
            break;
        case 3:
            audio::play(audio::SfxUiSelect, 0.8f);
            app().pushOverlay(std::make_unique<RecordsOverlay>());
            break;
        case 4:
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

    if (intro_ < 1.0f) {
        intro_ = clampf(intro_ + dt / intro::kDur, 0.0f, 1.0f);
        // 任意输入可以跳过开场，但这一次不再传给菜单。
        // 不然"按任意键继续"的那一下会直接把"开始游戏"按掉。
        auto& in = app().in;
        if (in.anyKey || in.lmbPressed || in.ok() || in.back()) intro_ = 1.0f;
    }
    // 背景跟着开场一起淡入：前 70% 的时间就铺满，收尾时已经完全亮起来
    bg_.setAlpha(easeOutCubic(intro_ / 0.70f));
    bg_.update(dt);

    if (intro_ < 1.0f) return;  // 开场期间不吃菜单输入

    bool canContinue = save::data().runs > 0;
    int prev = focus_;

    if (app().in.navUp()) {
        focus_ = (focus_ + kMenuCount - 1) % kMenuCount;
        mouseMode_ = false;
    }
    if (app().in.navDown()) {
        focus_ = (focus_ + 1) % kMenuCount;
        mouseMode_ = false;
    }
    if (focus_ == 1 && !canContinue && app().in.navUp()) focus_ = 0;
    if (focus_ == 1 && !canContinue && app().in.navDown()) focus_ = 2;
    if (focus_ != prev) audio::play(audio::SfxUiMove, 0.5f);

    // 鼠标
    for (int i = 0; i < kMenuCount; i++) {
        if (ui::Hover(MenuItemRect(i))) {
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
    // 开场衔接：logo 从启动屏的位置（720 宽 / 顶边 400）平移到菜单标题位（900 / 180）。
    // 起点和 BootScreen 用的是同一组常量，交接那一帧完全重合，看不出换屏。
    float k = easeInOutCubic(intro_);
    float lw = lerpf(intro::kBootLogoW, intro::kTitleLogoW, k);
    float ly = lerpf(intro::kBootLogoY, intro::kTitleLogoY, k);
    float lgh = lw * intro::kLogoAspect;

    // 信息条 / 菜单淡入：比 logo 晚一点起步，和 logo 同时收尾；顺带整体上浮一点
    float uiA = clampf((intro_ - 0.32f) / 0.55f, 0.0f, 1.0f);
    uiA = uiA * uiA * (3.0f - 2.0f * uiA);
    float rise = (1.0f - uiA) * 18.0f;
    auto F = [uiA](Color c) {
        c.a = (unsigned char)clampf(c.a * uiA, 0.0f, 255.0f);
        return c;
    };

    // 菜单区域压暗，保证文字在亮云上可读。
    // 关键是两端都收成 alpha 0：DrawRectangleGradientV 只有一端是透明的，
    // 另一端会留下一条横贯画面的硬边（看起来就像"贴了一层浅色遮罩"）。
    auto band = [&](float y0, float y1, float a0, float a1) {
        DrawRectangleGradientV(0, (int)y0, (int)VW, (int)(y1 - y0), F(WithAlpha(BLACK, a0)),
                               F(WithAlpha(BLACK, a1)));
    };
    band(0, 130, 0.45f, 0.0f);     // 顶部信息条
    band(452, 540, 0.0f, 0.52f);   // 淡入
    band(540, 900, 0.52f, 0.52f);  // 菜单区保持
    band(900, 1020, 0.52f, 0.0f);  // 淡出，露出底部云海

    // 顶部信息
    char buf[64];
    snprintf(buf, sizeof buf, "HI-SCORE %07d", save::data().hiScore);
    ui::Text(buf, 56, 44, 22, F(WithAlpha(pal::kCyan, 0.85f)));
    ui::TextR("v0.1.0", VW - 56, 44, 22, F(WithAlpha(pal::kDim, 0.7f)));

    // Logo
    if (assets::logoTex.id) {
        Rectangle src{0, 0, (float)assets::logoTex.width, (float)assets::logoTex.height};
        DrawTexturePro(assets::logoTex, src, {VW * 0.5f - lw * 0.5f, ly, lw, lgh}, {0, 0}, 0, WHITE);
    }

    // 菜单
    bool canContinue = save::data().runs > 0;
    for (int i = 0; i < kMenuCount; i++) {
        bool sel = (focus_ == i);
        bool enabled = !(i == 1 && !canContinue);
        float y = kMenuTop + rise + i * kMenuLH;
        // 浮动位移量化到整像素：亚像素位移会让 40px 的字和两侧三角每帧重采样一次，
        // 看起来就是"字在抖"。像素游戏里按整像素跳反而更干净。
        float float_ = sel ? std::round(std::sin((float)GetTime() * 5.2f) * 4.0f) : 0.0f;
        float tx = VW * 0.5f + float_;
        Color c = !enabled ? Color{96, 108, 122, 255} : (sel ? pal::kCyan : Color{186, 202, 218, 255});
        if (sel) {
            // 选中发光：三层同色描边字
            for (int kk = 3; kk >= 1; kk--)
                ui::TextC(kMenu[i], tx, y + 2, 40 + kk, F(WithAlpha(pal::kCyan, 0.10f)), true);
            ui::Triangle(tx - ui::TextW(kMenu[i], 40, true) * 0.5f - 46 + float_ * 0.3f, y + 26, 20, 1,
                         F(WithAlpha(pal::kCyan, 0.9f)));
            ui::Triangle(tx + ui::TextW(kMenu[i], 40, true) * 0.5f + 46 - float_ * 0.3f, y + 26, 20, -1,
                         F(WithAlpha(pal::kCyan, 0.9f)));
        }
        ui::TextOutline(kMenu[i], tx - ui::TextW(kMenu[i], 40, sel) * 0.5f, y, 40, F(c),
                        F(WithAlpha(BLACK, 0.6f)), sel);
    }

    // 底部提示
    ui::TextC("上下键 选择     ENTER 确认", VW * 0.5f, VH - 74, 20, F(WithAlpha(pal::kDim, 0.55f)));
    ui::TextC("2.5D 空战弹幕", VW * 0.5f, VH - 44, 18, F(WithAlpha(pal::kDim, 0.4f)));
}

// ---------------------------------------------------------------- 确认框
ConfirmOverlay::ConfirmOverlay(std::string title, std::string body, std::function<void()> onYes,
                               std::string yes, std::string no)
    : title_(std::move(title)), body_(std::move(body)), yes_(std::move(yes)), no_(std::move(no)),
      onYes_(std::move(onYes)) {}

namespace {
Rectangle ConfirmPanelRect() { return {VW * 0.5f - 280, VH * 0.5f - 150, 560, 300}; }
// 两个按钮的矩形只写一份：update() 的悬停命中和 draw() 的绘制必须完全一致，
// 否则会出现"指针放在按钮上但焦点不跟着走"。
Rectangle ConfirmBtnRect(int i) {
    Rectangle r = ConfirmPanelRect();
    return {r.x + (i == 0 ? 76.0f : 304.0f), r.y + 200, 180, 66};
}
}  // namespace

void ConfirmOverlay::update(float dt) {
    t_ += dt;
    if (app().in.navLeft()) { focus_ = 0; audio::play(audio::SfxUiMove, 0.5f); }
    if (app().in.navRight()) { focus_ = 1; audio::play(audio::SfxUiMove, 0.5f); }
    // 鼠标悬停直接接管焦点（和暂停菜单、阵亡界面的行为保持一致）
    for (int i = 0; i < 2; i++) {
        if (ui::Hover(ConfirmBtnRect(i)) && focus_ != i) {
            focus_ = i;
            audio::play(audio::SfxUiMove, 0.45f);
        }
    }
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
    Rectangle r = ConfirmPanelRect();
    // 弹窗用不透明底：半透明面板会把背后的动态画面透出来，边框跟着一起"呼吸"
    ui::Panel(r, Color{6, 12, 22, 255}, WithAlpha(pal::kCyan, 0.7f), 12, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.35f), 12);
    ui::TextC(title_, VW * 0.5f, r.y + 52, 36, pal::kText, true);
    ui::TextC(body_, VW * 0.5f, r.y + 126, 24, WithAlpha(pal::kDim, 0.95f));
    Rectangle b1 = ConfirmBtnRect(0);
    Rectangle b2 = ConfirmBtnRect(1);
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
    ui::Panel(r, Color{6, 12, 22, 255}, WithAlpha(pal::kCyan, 0.6f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 14);

    ui::TextC("最 高 分 排 行", VW * 0.5f, r.y + 40, 48, pal::kText, true);
    ui::Rect({r.x + 60, r.y + 118, r.width - 120, 2}, WithAlpha(pal::kCyan, 0.25f));

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

        ui::Rect({r.x + 60, y + 62, r.width - 120, 1}, WithAlpha(WHITE, 0.07f));
        y += 96;
    }

    // 汇总
    float sy = r.y + 470;
    ui::Rect({r.x + 60, sy, r.width - 120, 2}, WithAlpha(pal::kCyan, 0.25f));
    char buf[96];
    snprintf(buf, sizeof buf, "最高分  %07d", save::data().hiScore);
    ui::TextC(buf, VW * 0.5f, sy + 30, 34, pal::kGold, true);
    snprintf(buf, sizeof buf, "出击 %d 回      击毁 %d 个", save::data().runs, save::data().kills);
    ui::TextC(buf, VW * 0.5f, sy + 86, 24, WithAlpha(pal::kDim, 0.9f));

    ui::TextC("ESC / ENTER 返回", VW * 0.5f, r.y + r.height - 56, 20, WithAlpha(pal::kDim, 0.5f));
}

// ---------------------------------------------------------------- 设置
namespace {
// 设置面板几何：绘制与命中共用同一套函数，避免两边各写一份导致点不准
Rectangle SettingsRect() { return {VW * 0.5f - 460, 150, 920, 700}; }
constexpr float kSetRowH = 104;
constexpr float kSetRowTop = 236;  // 相对面板顶
Rectangle SettingsTabRect(int i) {
    Rectangle r = SettingsRect();
    return {r.x + 60 + i * 212, r.y + 112, 200, 54};
}
Rectangle SettingsRowRect(int i) {
    Rectangle r = SettingsRect();
    return {r.x + 40, r.y + kSetRowTop + i * kSetRowH - 16, r.width - 80, kSetRowH - 18};
}
// 音量条的可点区域比视觉高度高一些，方便点中
Rectangle SettingsSliderRect(int i) {
    Rectangle r = SettingsRect();
    return {r.x + 330, r.y + kSetRowTop + i * kSetRowH + 24, 380, 26};
}
}  // namespace

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

    // ---- 鼠标 ----
    // 分页按钮
    for (int i = 0; i < 3; i++) {
        if (ui::Hover(SettingsTabRect(i)) && in.lmbPressed) {
            if (page_ != i) {
                page_ = i;
                row_ = 0;
                audio::play(audio::SfxUiMove, 0.5f);
            }
        }
    }

    // 三行设置项：悬停选中（只在鼠标真的动了的时候接管，免得和方向键打架）
    for (int i = 0; i < 3; i++) {
        Rectangle row = SettingsRowRect(i);
        bool hov = ui::Hover(row);
        if (hov && (in.mouseMoved || in.lmbPressed) && row_ != i) {
            row_ = i;
            audio::play(audio::SfxUiMove, 0.4f);
        }
        if (!hov) continue;

        if (page_ == 0) {
            // 音量：点/拖进度条直接赋值（拖动过程中不写盘，松手再存）
            Rectangle bar = SettingsSliderRect(i);
            if (in.lmb && ui::Hover(bar)) {
                float* v = (i == 0) ? &app().volMaster : (i == 1 ? &app().volMusic : &app().volSfx);
                *v = std::round(clampf((in.mouse.x - bar.x) / bar.width, 0.0f, 1.0f) * 100.0f);
                app().applyVolume();
            }
        } else if (page_ == 1) {
            if (in.lmbPressed) adjust(1);  // 开关：点一下切换
        } else {
            if (in.lmbPressed) adjust(0);  // 改键：点一下进入等待按键
        }
    }

    if (in.lmbReleased) apply();  // 拖动结束才落盘
}

void SettingsOverlay::draw() {
    ui::Dim(0.7f, Color{4, 8, 14, 255});
    Rectangle r = SettingsRect();
    ui::Panel(r, Color{6, 12, 22, 255}, WithAlpha(pal::kCyan, 0.6f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 14);

    ui::TextC("设    置", VW * 0.5f, r.y + 34, 44, pal::kText, true);

    const char* tabs[3] = {"音频", "画面", "按键"};
    for (int i = 0; i < 3; i++) {
        Rectangle tr = SettingsTabRect(i);
        bool sel = (page_ == i);
        ui::Panel(tr, sel ? WithAlpha(pal::kCyan, 0.18f) : WithAlpha(WHITE, 0.04f),
                  sel ? pal::kCyan : WithAlpha(pal::kDim, 0.3f), 8, 2);
        ui::TextC(tabs[i], tr.x + tr.width * 0.5f, tr.y + 10, 28, sel ? pal::kText : pal::kDim, sel);
    }
    ui::Rect({r.x + 60, r.y + 190, r.width - 120, 2}, WithAlpha(pal::kCyan, 0.16f));

    auto rowBg = [&](int i) {
        if (row_ != i) return;
        Rectangle row = SettingsRowRect(i);
        ui::Rect(row, WithAlpha(pal::kCyan, 0.08f));
    };
    // 行内容的左上角（与命中区同源：命中区比内容高 16px，左右各宽 40）
    auto rowY = [&](int i) { return SettingsRowRect(i).y + 16; };

    if (page_ == 0) {
        const char* labels[3] = {"主音量", "音  乐", "音  效"};
        float vals[3] = {app().volMaster, app().volMusic, app().volSfx};
        for (int i = 0; i < 3; i++) {
            rowBg(i);
            float ry = rowY(i);
            ui::Text(labels[i], r.x + 80, ry + 18, 30, row_ == i ? pal::kText : pal::kDim, row_ == i);
            Rectangle bar = SettingsSliderRect(i);
            float tx = bar.x, tw = bar.width;
            ui::Rect({tx, ry + 32, tw, 10}, WithAlpha(WHITE, 0.12f));
            ui::Rect({tx, ry + 32, tw * vals[i] / 100.0f, 10}, row_ == i ? pal::kCyan : WithAlpha(pal::kCyan, 0.55f));
            for (int k = 0; k <= 20; k++)
                ui::Rect({tx + tw * k / 20.0f, ry + 28, 2, 18}, WithAlpha(WHITE, 0.2f));
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
            float ry = rowY(i);
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
            float ry = rowY(i);
            ui::Text(labels[i], r.x + 80, ry + 18, 30, row_ == i ? pal::kText : pal::kDim, row_ == i);
            bool editing = rebind_ && rebindRow_ == i;
            ui::Panel({r.x + 380, ry + 6, 260, 56},
                      editing ? WithAlpha(pal::kGold, 0.2f) : WithAlpha(WHITE, 0.05f),
                      editing ? pal::kGold : WithAlpha(pal::kDim, 0.4f), 8, 2);
            ui::TextC(editing ? "按任意键" : keys[i], r.x + 510, ry + 16, 26,
                      editing ? pal::kGold : pal::kText, true);
        }
    }

    ui::TextC("Q / E 换面板    上下键 选择    左右键 / 鼠标 调整    ESC 返回", VW * 0.5f, r.y + r.height - 52, 20,
              WithAlpha(pal::kDim, 0.6f));
}

}  // namespace sky
