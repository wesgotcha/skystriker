// LOBBY 双人入口：同屏合作 / 同屏对战 / 联机开房 / 联机找房
#include "Screens.hpp"

#include "App.hpp"
#include "Assets.hpp"
#include "Audio.hpp"
#include "Net.hpp"
#include "Save.hpp"
#include "Ui.hpp"

#include <cstdio>
#include <cstdlib>

namespace sky {

namespace {
const char* kMenu[5] = {"同屏合作", "同屏对战", "联机合作", "联机对战", "返    回"};
const char* kRoles[3] = {"建立主机", "连接主机", "返    回"};

Rectangle LobbyPanel() { return {VW * 0.5f - 470, 150, 940, 780}; }
Rectangle LobbyBtn(int i, int count) {
    Rectangle r = LobbyPanel();
    float w = 560, h = 76, gap = 20;
    float total = count * h + (count - 1) * gap;
    float y0 = r.y + (r.height - total) * 0.5f + 20;
    return {r.x + (r.width - w) * 0.5f, y0 + i * (h + gap), w, h};
}
Rectangle HostRow(int i) {
    Rectangle r = LobbyPanel();
    return {r.x + 120, r.y + 250 + i * 74, r.width - 240, 62};
}
}  // namespace

void LobbyScreen::enter() {
    bg_.setup(Backdrop::Dusk);
    t_ = 0;
    phase_ = Menu;
    focus_ = 0;
    hostSel_ = 0;
    ipEdit_ = false;
    ip_ = "127.0.0.1";
    launched_ = false;
    msg_.clear();
    msgT_ = 0;

    // 命令行直连（自动化测试用）：--mp=coop --host / --join=IP
    if (app().autoNetMode >= 0) {
        netMode_ = (MatchMode)app().autoNetMode;
        if (app().autoHost) {
            startHost();
            app().autoNetStart = true;
        } else if (!app().autoJoinIp.empty()) {
            ip_ = app().autoJoinIp;
            net::setLoadout(clampi(save::data().craft, 0, 5), assets::normalizeSkin(save::data().skin));
            bool ok = net::join(ip_);
            if (getenv("SKY_NET_LOG")) { printf("[lobby] auto join %s -> %d\n", ip_.c_str(), (int)ok); fflush(stdout); }
            if (ok) phase_ = Connecting;
            else { msg_ = net::errorText(); msgT_ = 3.0f; }
        }
        app().autoNetMode = -1;
    }
}

void LobbyScreen::back() {
    switch (phase_) {
        case Menu:
            app().go(Sid::Title);
            break;
        case NetRole:
            phase_ = Menu;
            focus_ = 0;
            break;
        default:
            net::cancel();
            phase_ = NetRole;
            focus_ = 1;
            break;
    }
    audio::play(audio::SfxUiMove, 0.6f);
}

void LobbyScreen::launchLocal(MatchMode m) {
    match::setMode(m);
    auto& c = match::cfg();
    c.online = false;
    c.seed = (uint32_t)(GetTime() * 1000.0);
    c.craft[0] = clampi(save::data().craft, 0, 5);
    c.skin[0] = assets::normalizeSkin(save::data().skin);
    c.craft[1] = clampi(save::data().craft2, 0, 5);
    c.skin[1] = assets::normalizeSkin(save::data().skin2);
    audio::play(audio::SfxUiSelect, 0.85f);
    app().go(Sid::Hangar);
}

void LobbyScreen::startHost() {
    match::setMode(netMode_);
    auto& c = match::cfg();
    c.craft[0] = clampi(save::data().craft, 0, 5);
    c.skin[0] = assets::normalizeSkin(save::data().skin);
    net::setLoadout(c.craft[0], c.skin[0]);
    if (!net::host((int)netMode_)) {
        msg_ = net::errorText();
        msgT_ = 3.0f;
        audio::play(audio::SfxHit, 0.4f, 0.6f);
        return;
    }
    phase_ = Hosting;
    focus_ = 0;
    audio::play(audio::SfxUiSelect, 0.8f);
}

void LobbyScreen::startJoin() {
    if (!net::browse()) {
        msg_ = net::errorText();
        msgT_ = 3.0f;
        return;
    }
    match::cfg().craft[0] = clampi(save::data().craft, 0, 5);
    match::cfg().skin[0] = assets::normalizeSkin(save::data().skin);
    net::setLoadout(match::cfg().craft[0], match::cfg().skin[0]);
    phase_ = Browsing;
    hostSel_ = 0;
    focus_ = 0;
    audio::play(audio::SfxUiSelect, 0.8f);
}

void LobbyScreen::launchOnline() {
    // 转场期间这一帧还可能被调用：只用第一份开局数据，后面一律忽略
    if (launched_) return;
    launched_ = true;
    const auto& si = net::startInfo();
    auto& c = match::cfg();
    c.mode = (MatchMode)si.mode;
    c.online = true;
    c.seed = si.seed;
    for (int i = 0; i < 2; i++) {
        c.craft[i] = si.craft[i];
        c.skin[i] = si.skin[i];
    }
    audio::play(audio::SfxUiSelect, 0.9f);
    if (getenv("SKY_NET_LOG")) { printf("[lobby] client start t=%.2f\n", (double)GetTime()); fflush(stdout); }
    // 锁步开局：两端都用转场（相差一次 RTT），进入 Game 后立刻开始对齐首帧
    app().go(Sid::Game);
}

void LobbyScreen::startOnlineMatch() {
    if (launched_) return;
    launched_ = true;
    auto& c = match::cfg();
    int p1c = clampi(save::data().craft, 0, 5);
    int p1s = assets::normalizeSkin(save::data().skin);
    int p2c = clampi(net::peerCraft(), 0, 5);
    int p2s = assets::normalizeSkin(net::peerSkin());
    uint32_t seed = (uint32_t)(GetTime() * 1000.0) ^ 0x9E3779B9u;
    net::sendStart(seed, (int)c.mode, p1c, p1s, p2c, p2s);
    c.craft[0] = p1c;
    c.skin[0] = p1s;
    c.craft[1] = p2c;
    c.skin[1] = p2s;
    c.seed = seed;
    c.online = true;
    if (getenv("SKY_NET_LOG")) { printf("[lobby] host start t=%.2f\n", (double)GetTime()); fflush(stdout); }
    app().go(Sid::Game);  // 走转场，但两端相差只有一次 RTT
}

void LobbyScreen::update(float dt) {
    t_ += dt;
    if (msgT_ > 0) msgT_ -= dt;
    bg_.update(dt);
    auto& in = app().in;

    // ---- 手动输入 IP ----
    if (ipEdit_) {
        int ch = 0;
        while ((ch = GetCharPressed()) > 0) {
            if (((ch >= '0' && ch <= '9') || ch == '.') && ip_.size() < 15) ip_.push_back((char)ch);
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !ip_.empty()) ip_.pop_back();
        if (IsKeyPressed(KEY_ESCAPE)) {
            ipEdit_ = false;
        } else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            ipEdit_ = false;
            net::setLoadout(clampi(save::data().craft, 0, 5), assets::normalizeSkin(save::data().skin));
            if (net::join(ip_)) {
                phase_ = Connecting;
                audio::play(audio::SfxUiSelect, 0.8f);
            } else {
                msg_ = net::errorText();
                msgT_ = 3.0f;
                audio::play(audio::SfxHit, 0.4f, 0.6f);
            }
        }
        return;
    }

    if (in.back()) {
        back();
        return;
    }

    switch (phase_) {
        case Menu: {
            if (in.navUp()) { focus_ = (focus_ + 4) % 5; audio::play(audio::SfxUiMove, 0.45f); }
            if (in.navDown()) { focus_ = (focus_ + 1) % 5; audio::play(audio::SfxUiMove, 0.45f); }
            for (int i = 0; i < 5; i++) {
                Rectangle r = LobbyBtn(i, 5);
                if (ui::Hover(r)) {
                    if (focus_ != i) audio::play(audio::SfxUiMove, 0.4f);
                    focus_ = i;
                    if (in.lmbPressed) {
                        if (i == 0) launchLocal(MatchMode::CoopLocal);
                        else if (i == 1) launchLocal(MatchMode::VersusLocal);
                        else if (i == 2) { netMode_ = MatchMode::CoopLan; phase_ = NetRole; focus_ = 0; }
                        else if (i == 3) { netMode_ = MatchMode::VersusLan; phase_ = NetRole; focus_ = 0; }
                        else back();
                    }
                }
            }
            if (in.ok()) {
                switch (focus_) {
                    case 0: launchLocal(MatchMode::CoopLocal); break;
                    case 1: launchLocal(MatchMode::VersusLocal); break;
                    case 2: netMode_ = MatchMode::CoopLan; phase_ = NetRole; focus_ = 0;
                            audio::play(audio::SfxUiSelect, 0.7f); break;
                    case 3: netMode_ = MatchMode::VersusLan; phase_ = NetRole; focus_ = 0;
                            audio::play(audio::SfxUiSelect, 0.7f); break;
                    default: back(); break;
                }
            }
            break;
        }
        case NetRole: {
            if (in.navUp()) { focus_ = (focus_ + 2) % 3; audio::play(audio::SfxUiMove, 0.45f); }
            if (in.navDown()) { focus_ = (focus_ + 1) % 3; audio::play(audio::SfxUiMove, 0.45f); }
            for (int i = 0; i < 3; i++) {
                Rectangle r = LobbyBtn(i, 3);
                if (ui::Hover(r)) {
                    focus_ = i;
                    if (in.lmbPressed) {
                        if (i == 0) startHost();
                        else if (i == 1) startJoin();
                        else back();
                    }
                }
            }
            if (in.ok()) {
                if (focus_ == 0) startHost();
                else if (focus_ == 1) startJoin();
                else back();
            }
            break;
        }
        case Hosting: {
            net::poll();
            if (net::failed()) {
                msg_ = net::errorText();
                msgT_ = 4.0f;
                net::cancel();
                phase_ = NetRole;
                break;
            }
            if (net::disconnected()) {
                msg_ = "对方断开";
                msgT_ = 3.0f;
                net::cancel();
                phase_ = NetRole;
                break;
            }
            bool ready = net::connected() && net::peerReady();
            if (ready && !launched_ && (in.ok() || (app().autoNetStart && t_ > 0.5f))) {
                startOnlineMatch();
                return;
            }
            break;
        }
        case Browsing: {
            net::poll();
            if (net::failed()) {
                msg_ = net::errorText();
                msgT_ = 3.0f;
                net::cancel();
                phase_ = NetRole;
                break;
            }
            const auto& hs = net::hosts();
            if (!hs.empty()) {
                if (hostSel_ >= (int)hs.size()) hostSel_ = (int)hs.size() - 1;
                if (in.navUp()) { hostSel_ = (hostSel_ + (int)hs.size() - 1) % (int)hs.size();
                                  audio::play(audio::SfxUiMove, 0.4f); }
                if (in.navDown()) { hostSel_ = (hostSel_ + 1) % (int)hs.size();
                                    audio::play(audio::SfxUiMove, 0.4f); }
                for (int i = 0; i < (int)hs.size() && i < 6; i++) {
                    Rectangle r = HostRow(i);
                    if (ui::Hover(r)) hostSel_ = i;
                    if (ui::Hover(r) && in.lmbPressed) {
                        net::setLoadout(clampi(save::data().craft, 0, 5), assets::normalizeSkin(save::data().skin));
                        if (net::join(hs[i].ip)) { phase_ = Connecting; audio::play(audio::SfxUiSelect, 0.8f); }
                        else { msg_ = net::errorText(); msgT_ = 3.0f; }
                    }
                }
                if (in.ok() && !hs.empty()) {
                    int i = clampi(hostSel_, 0, (int)hs.size() - 1);
                    net::setLoadout(clampi(save::data().craft, 0, 5), assets::normalizeSkin(save::data().skin));
                    if (net::join(hs[i].ip)) { phase_ = Connecting; audio::play(audio::SfxUiSelect, 0.8f); }
                    else { msg_ = net::errorText(); msgT_ = 3.0f; }
                }
            }
            if (in.pressed(KEY_R)) {
                net::browse();
                hostSel_ = 0;
                audio::play(audio::SfxUiMove, 0.5f);
            }
            if (in.pressed(KEY_TAB)) {
                ipEdit_ = true;
                audio::play(audio::SfxUiMove, 0.5f);
            }
            break;
        }
        case Connecting: {
            net::poll();
            if (net::startReceived()) {
                launchOnline();
                return;
            }
            if (net::disconnected()) {
                msg_ = "对方断开";
                msgT_ = 3.5f;
                net::cancel();
                phase_ = NetRole;
                break;
            }
            if (net::failed()) {
                msg_ = net::errorText();
                msgT_ = 3.5f;
                net::cancel();
                phase_ = NetRole;
                break;
            }
            break;
        }
    }
}

void LobbyScreen::drawWorld() { bg_.draw(); }

void LobbyScreen::draw() {
    ui::Dim(0.42f, Color{4, 8, 14, 255});
    Rectangle r = LobbyPanel();
    ui::Panel(r, Color{6, 12, 22, 255}, WithAlpha(pal::kCyan, 0.55f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.28f), 14);

    ui::TextC("双    人", VW * 0.5f, r.y + 30, 46, pal::kText, true);
    ui::TextC(phase_ == Menu ? "同屏双人 / 联机对战"
                             : (netMode_ == MatchMode::CoopLan ? "联机合作" : "联机对战"),
              VW * 0.5f, r.y + 94, 22, WithAlpha(pal::kCyan, 0.8f));
    ui::Rect({r.x + 60, r.y + 140, r.width - 120, 2}, WithAlpha(pal::kCyan, 0.18f));

    auto hint = [&](const char* s) {
        ui::TextC(s, VW * 0.5f, r.y + r.height - 54, 19, WithAlpha(pal::kDim, 0.55f));
    };

    if (phase_ == Menu) {
        for (int i = 0; i < 5; i++) ui::Button(LobbyBtn(i, 5), kMenu[i], focus_ == i, true, 34);
        hint("上下键 选择    ENTER 确认    ESC 返回");
    } else if (phase_ == NetRole) {
        for (int i = 0; i < 3; i++) ui::Button(LobbyBtn(i, 3), kRoles[i], focus_ == i, true, 34);
        hint("主机建立后，对方选择 连接主机    ESC 返回");
    } else if (phase_ == Hosting) {
        bool ready = net::connected() && net::peerReady();
        ui::TextC("主机已就绪", VW * 0.5f, r.y + 210, 36, pal::kText, true);

        char buf[128];
        snprintf(buf, sizeof buf, "IP   %s", net::localIp().c_str());
        ui::TextC(buf, VW * 0.5f, r.y + 290, 40, pal::kCyan, true);

        snprintf(buf, sizeof buf, "PORT %d", (int)47800);
        ui::TextC(buf, VW * 0.5f, r.y + 352, 20, WithAlpha(pal::kDim, 0.7f));

        if (ready) {
            float p = ui::Pulse(2.2f);
            ui::TextC("2P 已接入", VW * 0.5f, r.y + 450, 30, WithAlpha(Color{120, 240, 150, 255}, 0.7f + 0.3f * p),
                      true);
            ui::Panel({r.x + 250, r.y + 520, r.width - 500, 86}, WithAlpha(pal::kCyan, 0.16f), pal::kCyan, 10, 2);
            ui::TextC("ENTER 开始", VW * 0.5f, r.y + 540, 30, pal::kText, true);
        } else {
            ui::TextC("对方未接入", VW * 0.5f, r.y + 450, 30, WithAlpha(pal::kDim, 0.85f));
            ui::TextC("对方选择  连接主机", VW * 0.5f, r.y + 510, 20,
                      WithAlpha(pal::kDim, 0.55f));
        }
        hint("ESC 取消");
    } else if (phase_ == Browsing) {
        ui::TextC("主机列表", VW * 0.5f, r.y + 176, 32, pal::kText, true);
        const auto& hs = net::hosts();
        if (hs.empty()) {
            float p = ui::Pulse(1.2f);
            ui::TextC("暂无主机", VW * 0.5f, r.y + 300, 34, WithAlpha(pal::kDim, 0.6f + 0.25f * p));
            ui::TextC("确认主机已建立，对方已选择连接", VW * 0.5f, r.y + 366, 21,
                      WithAlpha(pal::kDim, 0.55f));
        } else {
            for (int i = 0; i < (int)hs.size() && i < 6; i++) {
                Rectangle row = HostRow(i);
                bool sel = (i == hostSel_);
                ui::Panel(row, sel ? WithAlpha(pal::kCyan, 0.18f) : WithAlpha(WHITE, 0.04f),
                          sel ? pal::kCyan : WithAlpha(pal::kDim, 0.28f), 8, sel ? 2.0f : 1.0f);
                ui::Text(match::modeName((MatchMode)hs[i].mode), row.x + 24, row.y + 14, 24,
                         sel ? pal::kText : pal::kDim, sel);
                ui::TextR(hs[i].ip, row.x + row.width - 24, row.y + 14, 24,
                          sel ? pal::kCyan : WithAlpha(pal::kDim, 0.8f));
            }
        }
        if (ipEdit_) {
            Rectangle f{r.x + 180, r.y + 560, r.width - 360, 74};
            ui::Panel(f, WithAlpha(Color{10, 26, 38, 255}, 0.95f), pal::kGold, 8, 2);
            std::string shown = "IP  " + ip_;
            if (((int)(t_ * 2.0f)) % 2 == 0) shown += "_";
            ui::TextC(shown, f.x + f.width * 0.5f, f.y + 20, 28, pal::kText, true);
            hint("数字 / 点    BACKSPACE 退格    ENTER 连接    ESC 取消");
        } else {
            hint("上下键 选择    ENTER 连接    R 重试    TAB 键入 IP    ESC 返回");
        }
    } else {  // Connecting
        bool link = net::connected();
        float p = ui::Pulse(1.2f);
        ui::TextC(link ? "已接入主机" : "连接中", VW * 0.5f, r.y + 250, 38,
                  link ? pal::kText : WithAlpha(pal::kCyan, 0.55f + 0.45f * p), true);
        char buf[128];
        snprintf(buf, sizeof buf, "对方  %s", net::peerIp().c_str());
        ui::TextC(buf, VW * 0.5f, r.y + 322, 24, WithAlpha(pal::kDim, 0.9f));
        if (link) {
            ui::TextC("准备中", VW * 0.5f, r.y + 420, 26, WithAlpha(pal::kDim, 0.75f));
            ui::TextC("主机开始后自动出击", VW * 0.5f, r.y + 470, 20, WithAlpha(pal::kDim, 0.5f));
        }
        hint("ESC 断开");
    }

    if (msgT_ > 0 && !msg_.empty()) {
        float a = clampf(msgT_, 0, 1);
        ui::TextC(msg_, VW * 0.5f, r.y + r.height - 96, 22, WithAlpha(pal::kDanger, a));
    }
}

}  // namespace sky
