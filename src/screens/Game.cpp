// GAMEPLAY 战斗屏 + 暂停 / 阵亡 / 对战结算 + 屏幕工厂
#include "Screens.hpp"

#include "App.hpp"
#include "Assets.hpp"
#include "Audio.hpp"
#include "Match.hpp"
#include "Net.hpp"
#include "Save.hpp"
#include "Ui.hpp"
#include "World.hpp"

#include <cmath>
#include <cstdio>

namespace sky {

int g_hangarMode = 0;  // 0 = 选机, 1 = 结算
bool g_forceResult = false;

static World  g_world;
static bool   g_continue = false;

void StartNewRun(bool continueRun) { g_continue = continueRun; }

static uint32_t quant(float v) {
    return (uint32_t)(int32_t)std::llround((double)v * 16.0);
}

// 锁步校验：只取会影响后续模拟的状态，浮点先量化到 1/16 像素
static uint32_t worldChecksum() {
    uint32_t h = 2166136261u;
    auto mix = [&](uint32_t v) {
        h ^= v;
        h *= 16777619u;
    };
    for (int i = 0; i < g_world.playerCount(); i++) {
        const Fighter& f = g_world.fighter(i);
        mix(quant(f.pos.x));
        mix(quant(f.pos.y));
        mix(quant(f.hp));
        mix(quant(f.invuln));
        mix((uint32_t)f.weapon);
        mix((uint32_t)f.bombs);
        mix((uint32_t)f.down);
        mix((uint32_t)f.dashT);
    }
    mix((uint32_t)g_world.score());
    mix((uint32_t)g_world.kills());
    mix((uint32_t)g_world.waveIndex());
    mix((uint32_t)g_world.enemiesLeft());
    mix(quant(g_world.bossHp01()));
    mix(quant(g_world.elapsed()));
    return h;
}

// 键盘 / 手柄 → Ctrl（只回传"按住"状态，按下沿由 World 自己算）
static Ctrl sampleCtrl(int slot) {
    Ctrl c;
    auto& in = app().in;
    bool solo = match::cfg().players() == 1;
    bool online = match::cfg().online;

    // 联机：本机只控制一名玩家（主机 0 / 客户端 1），两端必须用同一套键位，
    // 绝不能按 slot 区分 1P/2P——否则主机 WASD、客户端方向键，操作体验不一致。
    // 这里忽略 slot，两端都走标准 1P 键位（WASD + 空格/L + Shift 闪身）。
    if (online) {
        c.left = in.down(KEY_A);
        c.right = in.down(KEY_D);
        c.up = in.down(KEY_W);
        c.down = in.down(KEY_S);
        c.fire = in.down(save::data().keyFire) || in.down(KEY_SPACE);
        c.bomb = in.down(save::data().keyBomb) || in.down(KEY_L);
        bool shift = in.down(KEY_LEFT_SHIFT) || in.down(KEY_RIGHT_SHIFT);
        c.dash = in.down(KEY_L) || (shift && (c.left || c.right || c.up || c.down));
        return c;
    }

    if (slot == 0) {
        c.left = in.down(KEY_A);
        c.right = in.down(KEY_D);
        c.up = in.down(KEY_W);
        c.down = in.down(KEY_S);
        if (solo) {  // 单人时方向键也给 1P
            c.left |= in.down(KEY_LEFT);
            c.right |= in.down(KEY_RIGHT);
            c.up |= in.down(KEY_UP);
            c.down |= in.down(KEY_DOWN);
        }
        c.fire = in.down(save::data().keyFire) || in.down(KEY_SPACE);
        if (solo) c.fire |= in.lmb;
        c.bomb = in.down(save::data().keyBomb) || in.down(KEY_L);
        bool shift = in.down(KEY_LEFT_SHIFT) || in.down(KEY_RIGHT_SHIFT);
        c.dash = in.down(KEY_L) || (shift && (c.left || c.right || c.up || c.down));
    } else {
        // 2P：方向键 + 小键盘 1/2/3（或 . , /），外接手柄也认
        c.left = IsKeyDown(KEY_LEFT);
        c.right = IsKeyDown(KEY_RIGHT);
        c.up = IsKeyDown(KEY_UP);
        c.down = IsKeyDown(KEY_DOWN);
        c.fire = IsKeyDown(KEY_KP_1) || IsKeyDown(KEY_PERIOD) || IsKeyDown(KEY_RIGHT_CONTROL);
        c.bomb = IsKeyDown(KEY_KP_2) || IsKeyDown(KEY_COMMA);
        c.dash = IsKeyDown(KEY_KP_3) || IsKeyDown(KEY_SLASH);
        if (IsGamepadAvailable(0)) {
            float ax = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
            float ay = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
            c.left |= ax < -0.35f;
            c.right |= ax > 0.35f;
            c.up |= ay < -0.35f;
            c.down |= ay > 0.35f;
            c.fire |= IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
            c.bomb |= IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
            c.dash |= IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        }
    }
    return c;
}

// ---------------------------------------------------------------- 战斗屏
void GameScreen::enter() {
    auto& c = match::cfg();
    RunSetup s;
    s.mode = (int)c.mode;
    s.online = c.online;
    s.seed = c.seed;
    for (int i = 0; i < 2; i++) {
        s.craft[i] = c.craft[i];
        s.skin[i] = c.skin[i];
    }
    g_world.start(s, g_continue);
    g_continue = false;

    t_ = 0;
    acc_ = 0;
    tick_ = 0;
    netStall_ = 0;
    resultShown_ = false;
    online_ = c.online;
    lastSentTick_ = 0;
    localIn_.clear();

    // 两人的机体 / 配色都要先解码，避免开局第一帧才 150ms 卡一下
    for (int i = 0; i < g_world.playerCount(); i++) {
        int cr = clampi(g_world.fighter(i).craft, 0, 5);
        assets::ensureLoaded(assets::playerJob(cr));
        assets::ensureSkin(assets::players[cr], g_world.fighter(i).skin);
    }

    if (online_) {
        net::poll();
        net::beginMatch();
        if (getenv("SKY_NET_LOG")) {
            printf("[game] enter online slot=%d t=%.2f\n", net::localSlot(), (double)GetTime());
            fflush(stdout);
        }
        // 输入由 stepNet 按 tick 连续填充；这里不再预填，否则会漏掉追赶帧的中间 tick
    }
}

void GameScreen::stepLocal(float dt) {
    acc_ += std::min(dt, 0.1f);
    int steps = 0;
    while (acc_ >= kSimStep && steps < 3) {
        TickInput ti;
        ti.p[0] = sampleCtrl(0);
        if (g_world.playerCount() > 1) ti.p[1] = sampleCtrl(1);
        if (g_world.playerCount() == 1) {
            ti.mouseAim = app().in.lmb;
            ti.mouse = app().in.mouse;
        }
        g_world.step(kSimStep, ti, app().demoMode);
        acc_ -= kSimStep;
        steps++;
    }
}

void GameScreen::stepNet(float dt) {
    int slot = net::localSlot();
    Ctrl cur = sampleCtrl(slot);
    uint16_t bits = cur.bits();

    // 按 tick 连续发送：一帧可能追上两帧模拟，只发"当前帧 + 延迟"会漏掉中间 tick，
    // 对方就会永远等那一帧的输入（锁步直接卡死）。
    uint32_t target = tick_ + 4;
    while (lastSentTick_ <= target) {
        localIn_[lastSentTick_] = bits;
        net::sendInput(lastSentTick_, bits);
        lastSentTick_++;
    }

    acc_ += std::min(dt, 0.05f);
    if (acc_ > kSimStep * 4.0f) acc_ = kSimStep * 4.0f;

    int steps = 0;
    while (acc_ >= kSimStep && steps < 2) {
        uint16_t rb = 0;
        if (!net::inputFor(tick_, rb)) break;
        auto it = localIn_.find(tick_);
        uint16_t lb = (it != localIn_.end()) ? it->second : bits;
        TickInput ti;
        ti.p[slot] = Ctrl::fromBits(lb);
        ti.p[1 - slot] = Ctrl::fromBits(rb);
        g_world.step(kSimStep, ti, false);
        localIn_.erase(tick_);
        tick_++;
        acc_ -= kSimStep;
        steps++;
        if (getenv("SKY_NET_LOG") && (tick_ % 30u) == 0u) {
            printf("[game] advanced tick=%u bits0=%u bits1=%u\n", tick_, ti.p[0].bits(), ti.p[1].bits());
            fflush(stdout);
        }
        if ((tick_ % 60u) == 0u) net::sendChecksum(tick_, worldChecksum());
        if (localIn_.size() > 80) localIn_.erase(localIn_.begin());
    }

    // 等对方输入时累加；超过 6 秒直接判掉线
    if (steps == 0 && acc_ >= kSimStep) netStall_ += dt;
    else netStall_ = 0;

    if (getenv("SKY_NET_LOG") && netStall_ > 0.5f) {
        static float lastLog = -1;
        if (t_ - lastLog > 1.0f) {
            lastLog = t_;
            printf("[game] stall t=%.2f tick=%u pending=%d local=%d sent=%u stall=%.1f\n",
                   (double)GetTime(), tick_, net::pendingInputs(), (int)localIn_.size(), lastSentTick_,
                   netStall_);
            fflush(stdout);
        }
    }

    if (netStall_ > 6.0f) {
        resultShown_ = true;
        net::disconnect();
        app().pushOverlay(std::make_unique<DuelOverlay>("连接断开", "对方已断开。", false));
    }
}

void GameScreen::finishRun() {
    if (resultShown_) return;
    resultShown_ = true;
    auto& c = match::cfg();
    char sub[128];

    if (c.versus()) {
        int w = g_world.winner();
        int win = (w == 1) ? 2 : 1;
        snprintf(sub, sizeof sub, "得分 %07d    击毁 %d", g_world.score(), g_world.kills());
        if (c.online) net::disconnect();
        char title[24];
        snprintf(title, sizeof title, "%dP  WIN", win);
        app().pushOverlay(std::make_unique<DuelOverlay>(title, sub, !c.online));
        return;
    }

    if (c.online) {
        net::disconnect();
        snprintf(sub, sizeof sub, "得分 %07d    击毁 %d", g_world.score(), g_world.kills());
        app().pushOverlay(std::make_unique<DuelOverlay>(g_world.cleared() ? "任务完成" : "任务失败", sub, false));
        return;
    }

    if (!g_world.cleared()) {
        app().pushOverlay(std::make_unique<GameOverOverlay>());
        return;
    }

    // 本地通关：写档 + 结算面板（联机对战不写，避免把双人成绩混进单人榜）
    auto& d = HangarScreen::lastResult();
    d.kills = g_world.kills();
    d.score = g_world.score();
    d.maxCombo = g_world.maxCombo();
    d.time = g_world.elapsed();
    d.stars = g_world.stars();
    d.win = true;

    auto& p = save::data();
    p.runs++;
    p.kills += g_world.kills();
    p.lastScore = g_world.score();
    d.newRecord = g_world.score() > p.hiScore;
    if (d.newRecord) p.hiScore = g_world.score();
    int si = 0;
    p.stars[si] = std::max(p.stars[si], d.stars);
    p.stageScore[si] = std::max(p.stageScore[si], g_world.score());
    p.stageKills[si] = std::max(p.stageKills[si], g_world.kills());
    p.stageTime[si] = (int)g_world.elapsed();
    save::save();

    g_hangarMode = 1;
    app().go(Sid::Hangar);
}

void GameScreen::update(float dt) {
    t_ += dt;
    if (resultShown_) return;
    auto& in = app().in;

    if (online_) {
        net::poll();
        if (net::takeDesync()) {
            resultShown_ = true;
            net::disconnect();
            app().pushOverlay(std::make_unique<DuelOverlay>("状态不同步", "联机已断开。", false));
            return;
        }
        if (net::disconnected()) {
            resultShown_ = true;
            net::disconnect();
            app().pushOverlay(std::make_unique<DuelOverlay>("连接断开", "对方已断开。", false));
            return;
        }
        bool paused = false;
        if (!app().hasOverlay() && net::takePauseEvent(paused) && paused) {
            app().pushOverlay(std::make_unique<PauseOverlay>(true));
            return;
        }
    }

    if (g_world.finished()) {
        finishRun();
        return;
    }

    if (in.back()) {
        audio::play(audio::SfxUiSelect, 0.6f, 1.2f);
        if (online_ && net::connected()) net::sendPause(true);
        app().pushOverlay(std::make_unique<PauseOverlay>());
        return;
    }

    if (online_) stepNet(dt);
    else stepLocal(dt);

    if (g_world.finished()) finishRun();
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
    bool online = net::connected();

    if (online) {
        net::poll();
        bool pv = false;
        if (net::takePauseEvent(pv) && !pv) {
            app().popOverlay();
            return;
        }
        if (net::disconnected()) {
            net::disconnect();
            app().popOverlay();
            app().go(Sid::Title);
            return;
        }
    }

    if (in.back()) {
        if (online) net::sendPause(false);
        app().popOverlay();
        audio::play(audio::SfxUiMove, 0.5f);
        return;
    }

    int count = online ? 2 : 5;
    if (in.navUp()) { focus_ = (focus_ + count - 1) % count; audio::play(audio::SfxUiMove, 0.5f); }
    if (in.navDown()) { focus_ = (focus_ + 1) % count; audio::play(audio::SfxUiMove, 0.5f); }

    float by = VH * 0.5f - (online ? 90.0f : 130.0f);
    for (int i = 0; i < count; i++) {
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
        if (focus_ == 0) {
            if (online) net::sendPause(false);
            app().popOverlay();
        } else if (online) {
            net::disconnect();
            app().clearOverlays();
            app().go(Sid::Title);
        } else {
            switch (focus_) {
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
}

void PauseOverlay::draw() {
    ui::Dim(0.62f, Color{4, 8, 14, 255});
    bool online = net::connected();
    float ph = online ? 440.0f : 600.0f;
    Rectangle r{VW * 0.5f - 300, VH * 0.5f - ph * 0.5f, 600, ph};
    ui::Panel(r, Color{6, 12, 22, 255}, WithAlpha(pal::kCyan, 0.65f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 14);
    ui::TextC("PAUSED", VW * 0.5f, r.y + 40, 52, pal::kText, true);
    ui::TextC(remote_ ? "对方暂停" : "暂    停", VW * 0.5f, r.y + 104, 22,
              WithAlpha(pal::kDim, 0.75f));
    ui::Rect({r.x + 60, r.y + 148, r.width - 120, 2}, WithAlpha(pal::kCyan, 0.2f));

    const char* items[5] = {"继    续", "选择战机", "设    置", "重开关卡", "返回主菜单"};
    const char* onlineItems[2] = {"继    续", "返回主菜单"};
    int count = online ? 2 : 5;
    float by = VH * 0.5f - (online ? 90.0f : 130.0f);
    for (int i = 0; i < count; i++) {
        Rectangle rr{VW * 0.5f - 230, by + i * 74, 460, 62};
        ui::Button(rr, online ? onlineItems[i] : items[i], focus_ == i, true, 30);
    }
    ui::TextC(online ? "联机中暂停会同步给对方    ESC 返回" : "ESC 返回    上下键 选择",
              VW * 0.5f, r.y + r.height - 52, 20, WithAlpha(pal::kDim, 0.55f));
}

// ---------------------------------------------------------------- 对战结算 / 掉线
static Rectangle DuelBtn(int i, bool two) {
    float w = 300, h = 84;
    float total = two ? (w * 2 + 40) : w;
    float x = VW * 0.5f - total * 0.5f;
    return {x + i * (w + 40), 744, w, h};
}

DuelOverlay::DuelOverlay(std::string title, std::string sub, bool rematch)
    : title_(std::move(title)), sub_(std::move(sub)), rematch_(rematch) {}

void DuelOverlay::update(float dt) {
    t_ += dt;
    if (t_ < 0.55f) return;
    auto& in = app().in;
    if (in.back()) {
        app().clearOverlays();
        app().go(Sid::Title);
        return;
    }
    if (rematch_ && in.pressed(KEY_R)) {
        app().clearOverlays();
        g_continue = false;
        app().go(Sid::Game);
        return;
    }
    int count = rematch_ ? 2 : 1;
    if (in.navLeft()) focus_ = 0;
    if (in.navRight()) focus_ = count - 1;
    for (int i = 0; i < count; i++) {
        if (ui::Hover(DuelBtn(i, rematch_))) focus_ = i;
        if (ui::Hover(DuelBtn(i, rematch_)) && in.lmbPressed) {
            if (i == 0 && rematch_) {
                app().clearOverlays();
                g_continue = false;
                app().go(Sid::Game);
            } else {
                app().clearOverlays();
                app().go(Sid::Title);
            }
            return;
        }
    }
    if (in.ok()) {
        if (rematch_ && focus_ == 0) {
            app().clearOverlays();
            g_continue = false;
            app().go(Sid::Game);
        } else {
            app().clearOverlays();
            app().go(Sid::Title);
        }
    }
}

void DuelOverlay::draw() {
    ui::Dim(0.66f, Color{4, 8, 14, 255});
    Rectangle r{VW * 0.5f - 520, 220, 1040, 640};
    ui::Panel(r, Color{6, 12, 22, 255}, WithAlpha(pal::kCyan, 0.6f), 16, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 16);

    float a = clampf((t_ - 0.1f) / 0.35f, 0, 1);
    ui::TextC(title_, VW * 0.5f, r.y + 70, 76, WithAlpha(pal::kText, a), true);
    ui::TextC(sub_, VW * 0.5f, r.y + 220, 26, WithAlpha(pal::kDim, a));

    if (t_ > 0.55f) {
        if (rematch_) {
            ui::Button(DuelBtn(0, true), "再    战", focus_ == 0, true, 32);
            ui::Button(DuelBtn(1, true), "返    回", focus_ == 1, true, 32);
        } else {
            ui::Button(DuelBtn(0, false), "返    回", true, true, 32);
        }
    }
    ui::TextC(rematch_ ? "R 再战    ESC 返回" : "ENTER / ESC 返回", VW * 0.5f, r.y + 468, 20,
              WithAlpha(pal::kDim, 0.55f));
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
    if (n == "duel") return std::make_unique<DuelOverlay>("1P  WIN", "得分 0000000", true);
    if (n == "confirm")
        return std::make_unique<ConfirmOverlay>("退出游戏", "确定要退出游戏。", []() {});
    return nullptr;
}

std::unique_ptr<Screen> MakeScreen(Sid id) {
    switch (id) {
        case Sid::Boot: return std::make_unique<BootScreen>();
        case Sid::Title: return std::make_unique<TitleScreen>();
        case Sid::Lobby: return std::make_unique<LobbyScreen>();
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
