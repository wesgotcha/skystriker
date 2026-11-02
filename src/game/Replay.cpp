#include "Replay.hpp"

#include "Save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace sky::replay {

namespace fs = std::filesystem;

namespace {
const char* kDir = "replays";
const char  kMagic[6] = {'S', 'K', 'Y', 'R', 'P', 'L'};
const uint16_t kVer = 1;

std::string g_pending;
std::string g_lastSaved;

void fillClock(Meta& m) {
    std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    m.year = t.tm_year + 1900;
    m.month = t.tm_mon + 1;
    m.day = t.tm_mday;
    m.hour = t.tm_hour;
    m.minute = t.tm_min;
    m.second = t.tm_sec;
}

bool writeHead(std::FILE* f, const Meta& m, uint32_t count) {
    uint16_t craft = (uint16_t)m.craft, skin = (uint16_t)m.skin, ver = kVer;
    uint8_t  win = m.win, god = m.god, cont = m.cont, pad = 0;
    int32_t  score = m.score, kills = m.kills, combo = m.combo, stars = m.stars;
    float    time = m.time;
    uint16_t y = (uint16_t)m.year, mo = (uint16_t)m.month, d = (uint16_t)m.day;
    uint16_t h = (uint16_t)m.hour, mi = (uint16_t)m.minute, s = (uint16_t)m.second, pad1 = 0;
    if (std::fwrite(kMagic, 1, sizeof kMagic, f) != sizeof kMagic) return false;
    std::fwrite(&ver, sizeof ver, 1, f);
    std::fwrite(&craft, sizeof craft, 1, f);
    std::fwrite(&skin, sizeof skin, 1, f);
    std::fwrite(&win, 1, 1, f);
    std::fwrite(&god, 1, 1, f);
    std::fwrite(&cont, 1, 1, f);
    std::fwrite(&pad, 1, 1, f);
    std::fwrite(&score, sizeof score, 1, f);
    std::fwrite(&kills, sizeof kills, 1, f);
    std::fwrite(&combo, sizeof combo, 1, f);
    std::fwrite(&stars, sizeof stars, 1, f);
    std::fwrite(&time, sizeof time, 1, f);
    std::fwrite(&y, sizeof y, 1, f);
    std::fwrite(&mo, sizeof mo, 1, f);
    std::fwrite(&d, sizeof d, 1, f);
    std::fwrite(&h, sizeof h, 1, f);
    std::fwrite(&mi, sizeof mi, 1, f);
    std::fwrite(&s, sizeof s, 1, f);
    std::fwrite(&pad1, sizeof pad1, 1, f);
    std::fwrite(&count, sizeof count, 1, f);
    return true;
}

bool readHead(std::FILE* f, Meta& m, uint32_t& count) {
    char magic[sizeof kMagic]{};
    uint16_t ver = 0, craft = 0, skin = 0;
    uint8_t  win = 0, god = 0, cont = 0, pad = 0;
    int32_t  score = 0, kills = 0, combo = 0, stars = 0;
    float    time = 0;
    uint16_t y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0, pad1 = 0;
    if (std::fread(magic, 1, sizeof kMagic, f) != sizeof kMagic) return false;
    if (std::memcmp(magic, kMagic, sizeof kMagic) != 0) return false;
    if (std::fread(&ver, sizeof ver, 1, f) != 1 || ver != kVer) return false;
    if (std::fread(&craft, sizeof craft, 1, f) != 1) return false;
    if (std::fread(&skin, sizeof skin, 1, f) != 1) return false;
    if (std::fread(&win, 1, 1, f) != 1) return false;
    if (std::fread(&god, 1, 1, f) != 1) return false;
    if (std::fread(&cont, 1, 1, f) != 1) return false;
    if (std::fread(&pad, 1, 1, f) != 1) return false;
    if (std::fread(&score, sizeof score, 1, f) != 1) return false;
    if (std::fread(&kills, sizeof kills, 1, f) != 1) return false;
    if (std::fread(&combo, sizeof combo, 1, f) != 1) return false;
    if (std::fread(&stars, sizeof stars, 1, f) != 1) return false;
    if (std::fread(&time, sizeof time, 1, f) != 1) return false;
    if (std::fread(&y, sizeof y, 1, f) != 1) return false;
    if (std::fread(&mo, sizeof mo, 1, f) != 1) return false;
    if (std::fread(&d, sizeof d, 1, f) != 1) return false;
    if (std::fread(&h, sizeof h, 1, f) != 1) return false;
    if (std::fread(&mi, sizeof mi, 1, f) != 1) return false;
    if (std::fread(&s, sizeof s, 1, f) != 1) return false;
    if (std::fread(&pad1, sizeof pad1, 1, f) != 1) return false;
    if (std::fread(&count, sizeof count, 1, f) != 1) return false;
    if (count == 0 || count > (uint32_t)kMaxFrames * 4) return false;

    m.craft = craft;
    m.skin = skin;
    m.win = win != 0;
    m.god = god != 0;
    m.cont = cont != 0;
    m.score = score;
    m.kills = kills;
    m.combo = combo;
    m.stars = stars;
    m.time = time;
    m.year = y;
    m.month = mo;
    m.day = d;
    m.hour = h;
    m.minute = mi;
    m.second = s;
    m.frames = (int)count;
    return true;
}

// 方向轴在游戏里只有 5 种取值（0 / ±1 / ±0.7071）。写盘时压成 int8，
// 读回来按这 5 档还原，得到的浮点数和录制那一帧完全一致 —— 复盘才是"同一局"。
float snapAxis(float v) {
    if (v > 0.99f) return 1.0f;
    if (v < -0.99f) return -1.0f;
    if (std::fabs(v) < 0.01f) return 0.0f;
    return v > 0 ? 0.7071f : -0.7071f;
}

bool readMeta(const std::string& path, Meta& m) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint32_t count = 0;
    bool ok = readHead(f, m, count);
    std::fclose(f);
    if (ok) m.path = path;
    return ok;
}

// 超出 kMaxKeep 就删最旧的（文件名带时间戳，按名字排序即按时间排序）
void prune() {
    std::vector<std::string> names;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(kDir, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".rep") continue;
        names.push_back(e.path().filename().string());
    }
    if ((int)names.size() <= kMaxKeep) return;
    std::sort(names.begin(), names.end());
    int drop = (int)names.size() - kMaxKeep;
    for (int i = 0; i < drop; i++) {
        std::error_code del;
        fs::remove(fs::path(kDir) / names[i], del);
    }
}

}  // namespace

WorldInput Frame::toWorldInput() const {
    WorldInput wi;
    wi.dirX = dx;
    wi.dirY = dy;
    wi.mouseSteer = (flags & F_Mouse) != 0;
    wi.mouseX = mx;
    wi.mouseY = my;
    wi.fire = (flags & F_Fire) != 0;
    wi.bomb = (flags & F_Bomb) != 0;
    wi.dash = (flags & F_Dash) != 0;
    return wi;
}

long long Meta::stamp() const {
    return ((((long long)year * 100 + month) * 100 + day) * 100 + hour) * 10000LL + minute * 100 + second;
}

std::string Meta::when() const {
    char b[48];
    std::snprintf(b, sizeof b, "%04d.%02d.%02d  %02d:%02d", year, month, day, hour, minute);
    return b;
}

std::string Meta::length() const {
    int t = (int)(time + 0.5f);
    char b[24];
    std::snprintf(b, sizeof b, "%d:%02d", t / 60, t % 60);
    return b;
}

void Begin(Rec& r, bool cont, bool god) {
    r.meta = Meta{};
    r.frames.clear();
    r.frames.reserve(60 * 60 * 3);
    fillClock(r.meta);
    r.meta.craft = save::data().craft;
    r.meta.skin = save::data().skin;
    r.meta.cont = cont;
    r.meta.god = god;
}

void Push(Rec& r, const WorldInput& in, float dt) {
    if ((int)r.frames.size() >= kMaxFrames) return;
    Frame f;
    f.dt = dt;
    f.dx = in.dirX;
    f.dy = in.dirY;
    f.mx = in.mouseX;
    f.my = in.mouseY;
    if (in.fire) f.flags |= F_Fire;
    if (in.bomb) f.flags |= F_Bomb;
    if (in.dash) f.flags |= F_Dash;
    if (in.mouseSteer) f.flags |= F_Mouse;
    r.frames.push_back(f);
}

bool Finish(Rec& r, const World& w) {
    r.meta.score = w.score();
    r.meta.kills = w.kills();
    r.meta.combo = w.maxCombo();
    r.meta.stars = w.stars();
    r.meta.time = w.elapsed();
    r.meta.win = w.cleared();
    r.meta.frames = (int)r.frames.size();
    if (r.frames.empty()) return false;

    std::error_code ec;
    fs::create_directories(kDir, ec);

    char name[48];
    std::snprintf(name, sizeof name, "%04d%02d%02d-%02d%02d%02d", r.meta.year, r.meta.month, r.meta.day,
                  r.meta.hour, r.meta.minute, r.meta.second);
    std::string path = std::string(kDir) + "/" + name + ".rep";
    for (int n = 2; fs::exists(path, ec) && n < 100; n++) {
        path = std::string(kDir) + "/" + name + "-" + std::to_string(n) + ".rep";
    }

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = writeHead(f, r.meta, (uint32_t)r.frames.size());
    for (size_t i = 0; ok && i < r.frames.size(); i++) {
        const Frame& fr = r.frames[i];
        int8_t  dx = (int8_t)std::max(-127, std::min(127, (int)std::lround(fr.dx * 127.0f)));
        int8_t  dy = (int8_t)std::max(-127, std::min(127, (int)std::lround(fr.dy * 127.0f)));
        int16_t mx = (int16_t)std::max(-32000.0f, std::min(32000.0f, fr.mx));
        int16_t my = (int16_t)std::max(-32000.0f, std::min(32000.0f, fr.my));
        uint8_t pad = 0;
        if (std::fwrite(&fr.dt, sizeof fr.dt, 1, f) != 1) ok = false;
        if (ok && std::fwrite(&dx, 1, 1, f) != 1) ok = false;
        if (ok && std::fwrite(&dy, 1, 1, f) != 1) ok = false;
        if (ok && std::fwrite(&mx, sizeof mx, 1, f) != 1) ok = false;
        if (ok && std::fwrite(&my, sizeof my, 1, f) != 1) ok = false;
        if (ok && std::fwrite(&fr.flags, 1, 1, f) != 1) ok = false;
        if (ok && std::fwrite(&pad, 1, 1, f) != 1) ok = false;
    }
    std::fclose(f);
    if (!ok) {
        std::error_code rm;
        fs::remove(path, rm);
        return false;
    }

    r.meta.path = path;
    g_lastSaved = path;
    prune();
    return true;
}

std::vector<Meta> List() {
    std::vector<Meta> out;
    std::error_code ec;
    if (!fs::is_directory(kDir, ec)) return out;
    for (auto& e : fs::directory_iterator(kDir, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".rep") continue;
        Meta m;
        if (readMeta(e.path().string(), m)) out.push_back(std::move(m));
    }
    std::sort(out.begin(), out.end(), [](const Meta& a, const Meta& b) { return a.stamp() > b.stamp(); });
    return out;
}

bool Load(const std::string& path, Rec& out) {
    out.meta = Meta{};
    out.frames.clear();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint32_t count = 0;
    if (!readHead(f, out.meta, count)) {
        std::fclose(f);
        return false;
    }
    out.meta.path = path;
    out.frames.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        Frame fr;
        int8_t  dx = 0, dy = 0;
        int16_t mx = 0, my = 0;
        uint8_t pad = 0;
        if (std::fread(&fr.dt, sizeof fr.dt, 1, f) != 1) break;
        if (std::fread(&dx, 1, 1, f) != 1) break;
        if (std::fread(&dy, 1, 1, f) != 1) break;
        if (std::fread(&mx, sizeof mx, 1, f) != 1) break;
        if (std::fread(&my, sizeof my, 1, f) != 1) break;
        if (std::fread(&fr.flags, 1, 1, f) != 1) break;
        if (std::fread(&pad, 1, 1, f) != 1) break;
        fr.dx = snapAxis(dx / 127.0f);
        fr.dy = snapAxis(dy / 127.0f);
        fr.mx = mx;
        fr.my = my;
        out.frames.push_back(fr);
    }
    std::fclose(f);
    out.meta.frames = (int)out.frames.size();
    return !out.frames.empty();
}

void SetPending(const std::string& path) { g_pending = path; }

std::string TakePending() {
    std::string p = g_pending;
    g_pending.clear();
    return p;
}

const std::string& LastSaved() { return g_lastSaved; }

void ClearLastSaved() { g_lastSaved.clear(); }

bool HasAny() {
    std::error_code ec;
    if (!fs::is_directory(kDir, ec)) return false;
    for (auto& e : fs::directory_iterator(kDir, ec)) {
        if (ec) break;
        if (e.is_regular_file() && e.path().extension() == ".rep") return true;
    }
    return false;
}

}  // namespace sky::replay
