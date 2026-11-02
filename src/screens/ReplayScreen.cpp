// REPLAY 复盘播放：把录像里的输入逐帧灌回 World，重跑那一局
#include "Screens.hpp"

#include "App.hpp"
#include "Assets.hpp"
#include "Audio.hpp"
#include "Save.hpp"
#include "Ui.hpp"

#include <algorithm>
#include <cstdio>

namespace sky {

namespace {

const float kSpeed[4] = {0.5f, 1.0f, 2.0f, 4.0f};
const char* kSpeedName[4] = {"0.5x", "1.0x", "2.0x", "4.0x"};

// 底部控制条：居中偏下，避开左下「敌机」和右下「波次」两块 HUD
Rectangle CtlBar() { return {300, 918, 1200, 124}; }
// 0 暂停/继续  1 重放  2 返回
Rectangle CtlBtn(int i) {
    Rectangle r = CtlBar();
    return {r.x + 24 + i * 144.0f, r.y + 28, 132, 56};
}
Rectangle CtlTrack() {
    Rectangle r = CtlBar();
    return {r.x + 470, r.y + 34, 360, 10};
}
Rectangle CtlSpeedMinus() {
    Rectangle r = CtlBar();
    return {r.x + 860, r.y + 28, 44, 56};
}
Rectangle CtlSpeedPlus() {
    Rectangle r = CtlBar();
    return {r.x + 1056, r.y + 28, 44, 56};
}
Rectangle CtlSpeedLabel() {
    Rectangle r = CtlBar();
    return {r.x + 908, r.y + 28, 144, 56};
}

// 播完之后的收尾面板
Rectangle EndPanel() { return {VW * 0.5f - 260, 356, 520, 316}; }
// 0 重放  1 返回
Rectangle EndBtn(int i) {
    Rectangle r = EndPanel();
    return {r.x + (i == 0 ? 44.0f : 276.0f), r.y + 224, 200, 66};
}

void drawBarBg(Rectangle r) {
    ui::Panel(r, Color{6, 12, 22, 235}, WithAlpha(pal::kCyan, 0.45f), 10, 1.5f);
}

}  // namespace

void ReplayScreen::enter() {
    rec_ = replay::Rec{};
    std::string path = replay::TakePending();
    if (path.empty()) {
        // 直接从 --screen=replay 进来时没有指定文件：演最新的那一局
        auto all = replay::List();
        if (!all.empty()) path = all.front().path;
    }
    if (!path.empty()) replay::Load(path, rec_);

    // 复盘要按"当时"的机体放：先把模型和配色备好，再临时替换存档里的选择
    craftBak_ = save::data().craft;
    skinBak_ = save::data().skin;
    if (rec_.meta.craft >= 0 && rec_.meta.craft < (int)assets::players.size() &&
        assets::ensureLoaded(assets::playerJob(rec_.meta.craft))) {
        save::data().craft = rec_.meta.craft;
    }
    save::data().skin = assets::normalizeSkin(rec_.meta.skin);

    godBak_ = app().godMode;
    app().godMode = rec_.meta.god;
    restart();
}

void ReplayScreen::leave() {
    app().godMode = godBak_;
    save::data().craft = craftBak_;
    save::data().skin = skinBak_;
}

void ReplayScreen::restart() {
    idx_ = 0;
    acc_ = 0;
    played_ = 0;
    paused_ = false;
    logged_ = false;
    ended_ = rec_.frames.empty();
    world_.clearScriptedInput();
    world_.start(rec_.meta.cont);
}

void ReplayScreen::stepFrame() {
    if (idx_ >= rec_.frames.size()) {
        ended_ = true;
        return;
    }
    const replay::Frame& f = rec_.frames[idx_++];
    world_.setScriptedInput(f.toWorldInput());
    world_.update(f.dt, false);
    played_ += f.dt;
    // 世界自己判定终局（阵亡动画 / 通关演出也录在录像里），或者帧用完了
    if (world_.finished() || idx_ >= rec_.frames.size()) ended_ = true;
}

void ReplayScreen::update(float dt) {
    t_ += dt;
    auto& in = app().in;

    if (in.back()) {
        app().go(Sid::Title);
        return;
    }
    if (in.pressed(KEY_R)) {
        restart();
        audio::play(audio::SfxUiSelect, 0.7f);
        return;
    }
    if (in.pressed(KEY_SPACE)) {
        if (!ended_) {
            paused_ = !paused_;
            audio::play(audio::SfxUiMove, 0.6f);
        }
    }
    if (in.pressed(KEY_LEFT) && speedIdx_ > 0) {
        speedIdx_--;
        audio::play(audio::SfxUiMove, 0.5f);
    }
    if (in.pressed(KEY_RIGHT) && speedIdx_ < 3) {
        speedIdx_++;
        audio::play(audio::SfxUiMove, 0.5f);
    }

    // ---- 鼠标：控制条按钮（矩形与绘制同源）
    if (in.lmbPressed) {
        if (ui::Hover(CtlBtn(0)) && !ended_) { paused_ = !paused_; audio::play(audio::SfxUiMove, 0.6f); }
        if (ui::Hover(CtlBtn(1))) { restart(); audio::play(audio::SfxUiSelect, 0.7f); return; }
        if (ui::Hover(CtlBtn(2))) { app().go(Sid::Title); return; }
        if (ui::Hover(CtlSpeedMinus()) && speedIdx_ > 0) speedIdx_--;
        if (ui::Hover(CtlSpeedPlus()) && speedIdx_ < 3) speedIdx_++;
        if (ended_) {
            if (ui::Hover(EndBtn(0))) { restart(); audio::play(audio::SfxUiSelect, 0.7f); return; }
            if (ui::Hover(EndBtn(1))) { app().go(Sid::Title); return; }
        }
    }

    if (paused_ || ended_) {
        // --state-log 下把"重跑出来的结果"和"当时记下的结果"并排打出来，
        // 用来确认复盘真的是同一局（时间/得分/击毁一致）
        if (ended_ && app().stateLog && !logged_) {
            logged_ = true;
            std::printf("[replay] rerun t=%.3f score=%d kills=%d | rec t=%.3f score=%d kills=%d\n",
                        world_.elapsed(), world_.score(), world_.kills(), rec_.meta.time, rec_.meta.score,
                        rec_.meta.kills);
            std::fflush(stdout);
        }
        return;
    }

    // 按录像自己的 dt 推进：倍速只是把这段时间轴踩快一点
    acc_ += dt * kSpeed[speedIdx_];
    int guard = 0;
    while (!ended_ && idx_ < rec_.frames.size() && guard++ < 16) {
        float need = rec_.frames[idx_].dt;
        if (need <= 0.0f) {
            stepFrame();
            continue;
        }
        if (acc_ + 1e-5f < need) break;  // 留一点余量：dt 是浮点累加，卡得太死会掉成半速
        acc_ -= need;
        stepFrame();
    }
}

void ReplayScreen::drawWorld() { world_.drawBackground(); }

void ReplayScreen::draw3D() {
    cameraRig().begin();
    world_.draw3D();
    cameraRig().end();
}

void ReplayScreen::draw() {
    world_.drawFore();
    world_.drawHud();

    char buf[96];

    // 顶部角标：这不是真的在打，别让人误会
    float a = 0.55f + 0.45f * ui::Pulse(1.6f);
    ui::TextC("复 盘 回 放", VW * 0.5f, 22, 24, WithAlpha(pal::kCyan, a), true);

    // ---- 控制条
    Rectangle bar = CtlBar();
    drawBarBg(bar);

    bool pz = paused_;
    ui::Button(CtlBtn(0), pz ? "继    续" : "暂    停", false, !ended_, 26);
    ui::Button(CtlBtn(1), "重    放", false, true, 26);
    ui::Button(CtlBtn(2), "返    回", false, true, 26);

    // 进度
    Rectangle tr = CtlTrack();
    float total = rec_.frames.empty() ? 1.0f : (float)rec_.frames.size();
    float ratio = clampf((float)idx_ / total, 0, 1);
    ui::Rect(tr, WithAlpha(WHITE, 0.12f));
    ui::Rect({tr.x, tr.y, tr.width * ratio, tr.height}, pal::kCyan);
    ui::Rect({tr.x, tr.y, 3, tr.height}, WithAlpha(WHITE, 0.55f));
    ui::Rect({tr.x + tr.width * ratio - 2, tr.y - 4, 4, tr.height + 8}, pal::kGold);

    int cur = (int)(played_ + 0.5f);
    int all = (int)(rec_.meta.time + 0.5f);
    std::snprintf(buf, sizeof buf, "%d:%02d / %d:%02d", cur / 60, cur % 60, all / 60, all % 60);
    ui::Text(buf, tr.x + 4, tr.y + 24, 22, WithAlpha(pal::kDim, 0.95f));

    // 倍速
    ui::Button(CtlSpeedMinus(), "<", false, speedIdx_ > 0, 26);
    ui::Panel(CtlSpeedLabel(), WithAlpha(pal::kCyan, 0.10f), WithAlpha(pal::kCyan, 0.45f), 8, 1.5f);
    ui::TextC(kSpeedName[speedIdx_], CtlSpeedLabel().x + 72, CtlSpeedLabel().y + 14, 26, pal::kText, true);
    ui::Button(CtlSpeedPlus(), ">", false, speedIdx_ < 3, 26);
    ui::TextR("速 度", CtlSpeedMinus().x - 16, CtlBar().y + 42, 22, WithAlpha(pal::kDim, 0.9f));

    ui::TextR("SPACE 暂停    左右键 速度    R 重放    ESC 返回", bar.x + bar.width - 24, bar.y + bar.height + 10,
              20, WithAlpha(pal::kDim, 0.55f));

    // ---- 播完
    if (ended_) {
        ui::Dim(0.45f, Color{4, 8, 14, 255});
        Rectangle r = EndPanel();
        ui::Panel(r, Color{6, 12, 22, 246}, WithAlpha(pal::kCyan, 0.7f), 14, 2);
        ui::Frame(r, WithAlpha(pal::kCyan, 0.32f), 14);
        ui::TextC("回 放 完 成", VW * 0.5f, r.y + 40, 42, pal::kText, true);
        std::snprintf(buf, sizeof buf, "得分  %07d", rec_.meta.score);
        ui::TextC(buf, VW * 0.5f, r.y + 116, 30, pal::kGold, true);
        std::snprintf(buf, sizeof buf, "击毁 %d    时长 %s", rec_.meta.kills, rec_.meta.length().c_str());
        ui::TextC(buf, VW * 0.5f, r.y + 164, 24, WithAlpha(pal::kDim, 0.95f));
        ui::Button(EndBtn(0), "重    放", false, true, 26);
        ui::Button(EndBtn(1), "返    回", false, true, 26);
    }
}

}  // namespace sky
