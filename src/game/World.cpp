#include "World.hpp"

#include "App.hpp"
#include "Assets.hpp"
#include "Audio.hpp"
#include "Save.hpp"
#include "Ui.hpp"

#include <algorithm>
#include <cstdio>

namespace sky {

namespace {

// ---------------------------------------------------------------- 敌机档案
struct EnemyDef {
    int   craft;  // assets::enemies 下标
    float hp;
    float span;
    int   score;
    Color bar;
};

// C 侦察机（Spitfire）/ B 炮击机（Imperial）/ A 冲锋机（Insurgent）
const EnemyDef kEnemy[3] = {
    {3, 30.0f, 250.0f, 200, Color{126, 226, 255, 255}},
    {0, 130.0f, 300.0f, 600, Color{255, 196, 92, 255}},
    {1, 72.0f, 250.0f, 500, Color{255, 112, 96, 255}},
};

constexpr float kBossSpan = 620.0f;
constexpr float kBossHp = 1200.0f;
constexpr float kBossChargeTime = 1.15f;

constexpr float kPlayerSpan = 330.0f;
constexpr float kPlayerSpeed = 820.0f;
constexpr float kPlayerR = 11.0f;
constexpr float kPlayerMinX = 110.0f;
constexpr float kPlayerMaxX = 1150.0f;  // 玩家活动区固定在左侧，敌机在右半场

constexpr float kDashSpeed = 2050.0f;
constexpr float kDashDur = 0.20f;
constexpr float kDashCd = 0.80f;

// 伤害：普通弹 / 大炮弹（1/4）/ 冲撞（1/3）
constexpr float kDmgBullet = 9.0f;
constexpr float kDmgShell = 25.0f;
constexpr float kDmgRam = 34.0f;
constexpr float kDmgPvp = 6.0f;   // 对战中一发玩家弹的伤害

constexpr float kChargeTime = 1.05f;  // A 蓄力提示时长
constexpr float kRamSpeed = 1500.0f;

Color BulletColor(int kind) {
    switch (kind) {
        case 0: return Color{255, 74, 56, 255};
        case 1: return Color{255, 146, 26, 255};
        case 2: return Color{214, 84, 255, 255};
        default: return Color{255, 100, 80, 255};
    }
}

float EnemySpan(const Enemy& e) {
    return (e.cls == EC_BOSS) ? kBossSpan : kEnemy[e.cls].span * e.scale;
}

Color EnemyBarColor(int cls) { return (cls == EC_BOSS) ? pal::kDanger : kEnemy[cls].bar; }

// 分段血条
void SegBar(float x, float y, float w, float h, float ratio, int seg, Color fill, Color bg) {
    float gap = 2.0f;
    float sw = (w - gap * (seg - 1)) / seg;
    int lit = (int)std::round(clampf(ratio, 0, 1) * seg);
    for (int i = 0; i < seg; i++) {
        DrawRectangle((int)(x + i * (sw + gap)), (int)y, (int)std::ceil(sw), (int)h, (i < lit) ? fill : bg);
    }
}

// 粗线：DrawLineEx 在三角带路径上会掉亮度，这里用两个三角形自己拼
void ThickLine(Vector2 a, Vector2 b, float thick, Color c) {
    Vector2 d{b.x - a.x, b.y - a.y};
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.0001f || thick <= 0.0f) return;
    float nx = -d.y / len * thick * 0.5f, ny = d.x / len * thick * 0.5f;
    Vector2 p0{a.x + nx, a.y + ny}, p1{a.x - nx, a.y - ny};
    Vector2 p2{b.x - nx, b.y - ny}, p3{b.x + nx, b.y + ny};
    DrawTriangle(p0, p1, p2, c);
    DrawTriangle(p0, p2, p3, c);
}

// 冲锋瞄准线：从敌机沿锁定方向画到画面外，虚线 + 危险走廊
void DrawAimLine(Vector2 a, Vector2 dir, Color c, float alpha) {
    float len = 3000.0f;
    if (dir.x < -0.0001f) len = std::min(len, (-140.0f - a.x) / dir.x);
    if (dir.x > 0.0001f) len = std::min(len, (VW + 140.0f - a.x) / dir.x);
    if (dir.y < -0.0001f) len = std::min(len, (-140.0f - a.y) / dir.y);
    if (dir.y > 0.0001f) len = std::min(len, (VH + 140.0f - a.y) / dir.y);
    if (len < 10.0f) return;

    Vector2 b{a.x + dir.x * len, a.y + dir.y * len};
    ThickLine(a, b, 132.0f, WithAlpha(c, 0.10f * alpha));
    ThickLine(a, b, 56.0f, WithAlpha(c, 0.14f * alpha));
    ThickLine(a, b, 5.0f, WithAlpha(c, 0.45f * alpha));

    float off = (float)std::fmod((double)GetTime() * 320.0, 56.0);
    for (float s = 40.0f + off; s < len; s += 56.0f) {
        float e = std::min(s + 32.0f, len);
        ThickLine({a.x + dir.x * s, a.y + dir.y * s}, {a.x + dir.x * e, a.y + dir.y * e}, 10.0f,
                  WithAlpha(c, 0.95f * alpha));
        ThickLine({a.x + dir.x * s, a.y + dir.y * s}, {a.x + dir.x * e, a.y + dir.y * e}, 4.0f,
                  WithAlpha(Color{255, 240, 232, 255}, 0.85f * alpha));
    }
}

// 警示三角 + 感叹号
void DrawWarnMark(float cx, float cy, float size, float alpha) {
    Color c = WithAlpha(pal::kDanger, alpha);
    DrawTriangle({cx, cy - size}, {cx - size * 0.92f, cy + size * 0.72f}, {cx + size * 0.92f, cy + size * 0.72f}, c);
    DrawTriangle({cx, cy - size * 0.62f}, {cx - size * 0.58f, cy + size * 0.44f}, {cx + size * 0.58f, cy + size * 0.44f},
                 WithAlpha(Color{24, 6, 8, 255}, 0.75f * alpha));
    ui::TextC("!", cx, cy - size * 0.62f, (int)(size * 0.92f), WithAlpha(WHITE, alpha), true);
}

// 巡航：向轨道目标点做指数趋近，而不是直接把 pos 赋成轨道公式值。
// 直接赋值会在「入场结束」那一帧产生位移跳变（最多 90px），姿态跟着抽一下。
void OrbitTo(Vector2& pos, Vector2 target, float dt, float rate) {
    if (dt <= 0.0f) return;
    float k = 1.0f - std::exp(-rate * dt);
    pos.x += (target.x - pos.x) * k;
    pos.y += (target.y - pos.y) * k;
}

}  // namespace

// ---------------------------------------------------------------- 初始化
void World::reset() {
    bullets_.clear();
    enemies_.clear();
    particles_.clear();
    pickups_.clear();
    boss_ = Enemy{};
    score_ = 0;
    kills_ = 0;
    combo_ = 0;
    maxCombo_ = 0;
    comboT_ = 0;
    shake_ = 0;
    flash_ = 0;
    elapsed_ = 0;
    wave_ = 0;
    wavePhase_ = WP_Banner;
    waveT_ = 0;
    waveDrop_ = false;
    banner_ = "";
    bannerT_ = 0;
    bannerDur_ = 1.6f;
    stormMix_ = 0;
    state_ = WorldState::Playing;
    stateT_ = 0;
    finished_ = false;
    bossEnter_ = 0;
    bossEnterDur_ = 2.0f;
    bossPending_ = false;
    bossDelay_ = 0;
    versus_ = false;
    online_ = false;
    winner_ = -1;
    playerCount_ = 1;

    for (int i = 0; i < kMaxPlayers; i++) {
        Fighter f;
        f.active = (i == 0);
        f.slot = i;
        f.pos = (i == 0) ? Vector2{460, 720} : Vector2{760, 900};
        f.vel = {0, 0};
        f.pose = pose::FromRoll(pose::kPlayer.base);
        f.lastY = f.pos.y;
        f.vyN = 0;
        f.invuln = 1.5f;
        f.fireCd = 0;
        f.hp = kPlayerMaxHp;
        f.weapon = 1;
        f.bombs = 2;
        f.down = false;
        f.deathT = 0;
        f.respawnT = 0;
        f.dashT = 0;
        f.dashCd = 0;
        f.dashDir = {1, 0};
        f.ghostTimer = 0;
        f.prevBits = 0;
        f.hitFlash = 0;
        f.bombFlash = 0;
        f.kills = 0;
        f.face = 1;
        f.ghosts.clear();
        p_[i] = f;
    }

    rng_.seed(0x5EED);
    bg_.setup(Backdrop::Ocean);
    bgStorm_.setup(Backdrop::Storm);
}

void World::start(const RunSetup& setup, bool continueRun) {
    reset();
    rng_.seed(setup.seed);
    online_ = setup.online;
    versus_ = (setup.mode == 2 || setup.mode == 4);
    playerCount_ = (setup.mode == 0) ? 1 : 2;

    for (int i = 0; i < kMaxPlayers; i++) {
        Fighter& f = p_[i];
        f.active = (i < playerCount_);
        f.craft = clampi(setup.craft[i], 0, 5);
        f.skin = assets::normalizeSkin(setup.skin[i]);
    }

    if (versus_) {
        // 对战：左右半场各站一个，机头相对
        p_[0].pos = {520, 620};
        p_[0].face = 1;
        p_[1].pos = {1400, 620};
        p_[1].face = -1;
        p_[1].invuln = 2.5f;
        p_[0].invuln = 2.5f;
        showBanner("VS", 1.2f);
    } else {
        p_[0].pos = {460, 700};
        p_[1].pos = {800, 900};
        if (continueRun) {
            p_[0].weapon = 2;
            p_[0].bombs = 3;
        }
        wave_ = 0;
        wavePhase_ = WP_Banner;
        waveT_ = 1.5f;
        showBanner("WAVE 1", 1.5f);
    }
}

void World::showBanner(const std::string& s, float dur) {
    banner_ = s;
    bannerDur_ = dur;
    bannerT_ = dur;
}

// ---------------------------------------------------------------- 目标选择
// 双人时敌机只锁最近的那一个；两人都倒下时退回 P1 的位置（只是为了让弹道有意义，
// 此时战斗其实已经结束）。
bool World::anyPlayerAlive() const {
    for (int i = 0; i < playerCount_; i++)
        if (!p_[i].down) return true;
    return false;
}

int World::nearestPlayer(Vector2 from) const {
    int best = 0;
    float bestD = 1e18f;
    for (int i = 0; i < playerCount_; i++) {
        if (p_[i].down) continue;
        float dx = p_[i].pos.x - from.x, dy = p_[i].pos.y - from.y;
        float d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    if (bestD > 1e17f) {
        for (int i = 0; i < playerCount_; i++)
            if (p_[i].active) return i;
    }
    return best;
}

Vector2 World::aimTarget(Vector2 from) const { return p_[nearestPlayer(from)].pos; }

// ---------------------------------------------------------------- 工具
Vector2 World::shakeOffset() const {
    if (shake_ <= 0.01f || !app().optShake) return {0, 0};
    float a = shake_ * 18.0f;
    return {(float)GetRandomValue(-100, 100) / 100.0f * a, (float)GetRandomValue(-100, 100) / 100.0f * a};
}

float World::bossHp01() const {
    if (!boss_.alive || boss_.maxHp <= 0) return 0;
    return clampf(boss_.hp / boss_.maxHp, 0, 1);
}

float World::dashReady01() const { return clampf(1.0f - p_[0].dashCd / kDashCd, 0, 1); }

int World::enemiesLeft() const {
    int n = 0;
    for (const auto& e : enemies_)
        if (e.alive) n++;
    if (bossPending_) n++;
    if (boss_.alive) n++;
    return n;
}

int World::stars() const {
    if (state_ != WorldState::Cleared || versus_) return 0;
    // 双人：按两人剩余装甲的平均值评星，单人时就是自己的血量
    float sum = 0;
    for (int i = 0; i < playerCount_; i++) sum += clampf(p_[i].hp / kPlayerMaxHp, 0, 1);
    float h = (playerCount_ > 0) ? sum / playerCount_ : 0.0f;
    int s = 1;
    if (h >= 0.45f) s = 2;
    if (h >= 0.75f) s = 3;
    return s;
}

void World::addBullet(Vector2 pos, Vector2 vel, float r, Color col, int kind, float dmg, bool hostile, bool big,
                      int owner) {
    if ((int)bullets_.size() > 1200) return;
    Bullet b;
    b.pos = pos;
    b.vel = vel;
    b.r = r;
    b.col = col;
    b.kind = kind;
    b.dmg = dmg;
    b.hostile = hostile;
    b.big = big;
    b.owner = owner;
    b.life = 8.0f;
    b.alive = true;
    b.spin = 0;
    bullets_.push_back(b);
}

void World::addBurst(Vector2 pos, float count, float speed, float spread, Color col, float r, float baseAngle,
                     float dmg, bool hostile) {
    for (int i = 0; i < (int)count; i++) {
        float a = baseAngle + (count > 1 ? (-spread * 0.5f + spread * i / (count - 1)) : 0.0f);
        addBullet(pos, {std::cos(a) * speed, std::sin(a) * speed}, r, col, 0, dmg, hostile);
    }
}

void World::addExplosion(Vector2 pos, float size, Color col, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        float a = rng_.range(0, 2 * PI);
        float sp = rng_.range(60, 60 + size * 4.0f);
        p.pos = pos;
        p.vel = {std::cos(a) * sp, std::sin(a) * sp};
        p.maxLife = p.life = rng_.range(0.28f, 0.62f) * (0.7f + size / 120.0f);
        p.size = rng_.range(size * 0.06f, size * 0.18f);
        p.col = (i % 3 == 0) ? Color{255, 236, 190, 255} : col;
        p.kind = 0;
        p.alive = true;
        particles_.push_back(p);
    }
    for (int i = 0; i < count / 3 + 2; i++) {
        Particle p;
        float a = rng_.range(0, 2 * PI);
        p.pos = pos;
        p.vel = {std::cos(a) * rng_.range(20, 90), std::sin(a) * rng_.range(20, 90)};
        p.maxLife = p.life = rng_.range(0.5f, 1.1f);
        p.size = rng_.range(size * 0.1f, size * 0.22f);
        p.col = Color{60, 52, 48, 160};
        p.kind = 2;
        p.alive = true;
        particles_.push_back(p);
    }
}

void World::addHitSpark(Vector2 pos, Color col) {
    for (int i = 0; i < 4; i++) {
        Particle p;
        float a = rng_.range(0, 2 * PI);
        p.pos = pos;
        p.vel = {std::cos(a) * rng_.range(80, 240), std::sin(a) * rng_.range(80, 240)};
        p.maxLife = p.life = rng_.range(0.12f, 0.24f);
        p.size = rng_.range(2.0f, 4.5f);
        p.col = col;
        p.kind = 1;
        p.alive = true;
        particles_.push_back(p);
    }
}

void World::addTrail(Vector2 pos, Vector2 dir, Color col) {
    Particle p;
    p.pos = pos;
    p.vel = {-dir.x * rng_.range(40, 160) + rng_.range(-40, 40), -dir.y * rng_.range(40, 160) + rng_.range(-40, 40)};
    p.maxLife = p.life = rng_.range(0.16f, 0.34f);
    p.size = rng_.range(6.0f, 15.0f);
    p.col = col;
    p.kind = 0;
    p.alive = true;
    particles_.push_back(p);
}

void World::addScore(int base) {
    combo_++;
    comboT_ = 2.6f;
    maxCombo_ = std::max(maxCombo_, combo_);
    float mult = 1.0f + std::min(combo_, 40) * 0.05f;
    score_ += (int)(base * mult);
}

void World::spawnPickup(Vector2 pos, int kind) {
    Pickup p;
    p.pos = pos;
    p.kind = kind;
    p.t = 0;
    p.alive = true;
    // 出场就朝最近玩家一侧弹出去：敌机全在右半场，光靠原来的 -90px/s 漂移，
    // 道具会停在 x≈1300（玩家上限 1150）永远够不到。
    Vector2 tgt = aimTarget(pos);
    Vector2 d{tgt.x - pos.x, tgt.y - pos.y};
    float l = std::sqrt(d.x * d.x + d.y * d.y);
    if (l < 1.0f) { d = {-1, 0}; l = 1.0f; }
    float sp = 470.0f;
    p.vel = {d.x / l * sp + rng_.range(-30.0f, 30.0f), d.y / l * sp + rng_.range(-40.0f, 40.0f)};
    pickups_.push_back(p);
}

// 按概率掉落。压到 ~22%，并做保底：本波最后一架必掉，Boss 必掉，
// 万一整波都没掉（例如 Boss 波的杂兵）就在清波时补一颗。
void World::dropPickup(Vector2 pos) {
    int liveMinions = 0;
    for (const auto& e : enemies_)
        if (e.alive) liveMinions++;
    bool must = !waveDrop_ && liveMinions == 0;  // 本波最后一只（Boss 另算，它必掉）

    float r = rng_.f();
    int kind = -1;
    if (r < 0.075f) kind = 1;        // 爆弹
    else if (r < 0.150f) kind = 2;   // 修复
    else if (r < 0.220f) kind = 0;   // 火力
    if (kind < 0 && !must) return;
    if (kind < 0) kind = (rng_.f() < 0.5f) ? 0 : 2;

    waveDrop_ = true;
    spawnPickup(pos, kind);
}

// ---------------------------------------------------------------- 生成
void World::spawnEnemy(int cls, float delay) {
    Enemy e;
    e.cls = cls;
    e.delay = delay;
    e.st = ES_Wait;
    e.alive = true;
    e.scale = (cls == EC_B) ? 1.12f : (cls == EC_A ? 1.02f : 0.90f);
    e.maxHp = e.hp = kEnemy[cls].hp;
    e.score = kEnemy[cls].score;
    e.phase = rng_.range(0, 6.28f);
    e.fireT = rng_.range(1.0f, 2.0f);
    e.specialT = rng_.range(2.8f, 4.6f);
    e.hpTrail = 1.0f;

    // 巡航锚点全部落在画面右半边（x >= 1180）
    float x0 = 1250, x1 = 1690, y0 = 270, y1 = 850;
    if (cls == EC_B) { x0 = 1460; x1 = 1720; y0 = 300; y1 = 760; }
    if (cls == EC_A) { x0 = 1310; x1 = 1650; y0 = 290; y1 = 800; }
    // 候选点里挑离现有敌机最远的一个，避免同波次机体叠在一起
    Vector2 best{rng_.range(x0, x1), rng_.range(y0, y1)};
    float bestD = -1.0f;
    for (int i = 0; i < 14; i++) {
        Vector2 c{rng_.range(x0, x1), rng_.range(y0, y1)};
        float d = 1e9f;
        for (const auto& o : enemies_) {
            if (!o.alive) continue;
            float dx = c.x - o.home.x, dy = c.y - o.home.y;
            d = std::min(d, dx * dx + dy * dy);
        }
        if (d > bestD) { bestD = d; best = c; }
    }
    e.home = best;

    e.spawn = {VW + 240 + rng_.range(0, 240), e.home.y};
    e.pos = e.spawn;
    pose::Reset(e.pose, pose::kEnemy, cls == EC_B ? -0.05f : -0.08f);
    e.lastY = e.pos.y;
    e.vyN = 0;
    e.posJump = false;
    enemies_.push_back(e);
}

void World::spawnWave(int idx) {
    waveDrop_ = false;  // 新一波重新计算保底
    // 五波编队：C 小兵打头，B / A 稍后压上
    switch (idx) {
        case 0:
            for (int i = 0; i < 5; i++) spawnEnemy(EC_C, i * 0.22f);
            break;
        case 1:
            spawnEnemy(EC_C, 0.0f);
            spawnEnemy(EC_C, 0.28f);
            spawnEnemy(EC_B, 0.95f);
            break;
        case 2:
            for (int i = 0; i < 3; i++) spawnEnemy(EC_C, i * 0.22f);
            spawnEnemy(EC_A, 1.10f);
            break;
        case 3:
            for (int i = 0; i < 5; i++) spawnEnemy(EC_C, i * 0.18f);
            spawnEnemy(EC_B, 0.85f);
            spawnEnemy(EC_A, 1.45f);
            break;
        default:
            for (int i = 0; i < 3; i++) spawnEnemy(EC_C, i * 0.20f);
            spawnEnemy(EC_A, 0.65f);
            spawnEnemy(EC_A, 1.25f);
            bossPending_ = true;
            bossDelay_ = 1.7f;
            break;
    }
}

void World::activateBoss() {
    boss_ = Enemy{};
    boss_.cls = EC_BOSS;
    boss_.st = ES_Enter;
    boss_.alive = true;
    boss_.maxHp = boss_.hp = kBossHp;
    boss_.hpTrail = 1.0f;
    boss_.scale = 1.0f;
    boss_.score = 15000;
    boss_.spawn = {VW + 460, 540};
    boss_.home = {1480, 540};
    boss_.pos = boss_.spawn;
    boss_.t = 0;
    boss_.stateT = 0;
    boss_.fireT = 1.6f;
    pose::Reset(boss_.pose, pose::kEnemy);
    boss_.lastY = boss_.pos.y;
    boss_.vyN = 0;
    boss_.posJump = false;
    bossEnter_ = 0;
    bossEnterDur_ = 2.0f;
    showBanner("IRON FORTRESS", 2.4f);
    audio::play(audio::SfxWarn, 1.0f, 0.85f);
}

// ---------------------------------------------------------------- 玩家
void World::damagePlayer(int slot, float dmg) {
    Fighter& f = p_[slot];
    if (!f.active || f.down || f.deathT > 0 || state_ != WorldState::Playing) return;
    if (f.invuln > 0) return;
    if (!online_ && app().godMode) return;
    f.hp = std::max(0.0f, f.hp - dmg);
    f.hitFlash = 0.12f;
    audio::play(audio::SfxDamage, 0.95f, 1.0f);
    addExplosion(f.pos, 130, Color{255, 150, 100, 255}, 18);
    shake_ = std::max(shake_, 0.85f);
    flash_ = 0.5f;
    combo_ = 0;
    f.weapon = std::max(1, f.weapon - 1);
    if (f.hp <= 0) {
        killPlayer(slot);
    } else {
        f.invuln = 0.85f;
    }
}

// 对战专用伤害：不做武器降级、不炸整屏，只扣装甲 + 给一点点无敌帧，
// 否则一轮四发齐射会在同一帧里全部判定命中。
void World::damagePlayerPvp(int slot, float dmg) {
    Fighter& f = p_[slot];
    if (!f.active || f.down || state_ != WorldState::Playing) return;
    if (f.invuln > 0) return;
    if (!online_ && app().godMode) return;
    f.hp = std::max(0.0f, f.hp - dmg);
    f.hitFlash = 0.1f;
    addHitSpark(f.pos, Color{255, 220, 160, 255});
    shake_ = std::max(shake_, 0.25f);
    if (f.hp <= 0) {
        killPlayer(slot);
    } else {
        f.invuln = 0.06f;
    }
}

void World::killPlayer(int slot) {
    Fighter& f = p_[slot];
    f.down = true;
    f.hp = 0;
    f.deathT = 1.4f;
    f.dashT = 0;
    addExplosion(f.pos, 240, Color{255, 120, 80, 255}, 44);
    audio::play(audio::SfxBigExplode, 1.0f, 0.9f);
    shake_ = 1.2f;

    if (versus_) {
        winner_ = 1 - slot;
        state_ = WorldState::Cleared;
        stateT_ = 0;
    } else {
        // 组队：还有队友活着就不算失败，倒下的等倒计时复活
        bool anyAlive = false;
        for (int i = 0; i < playerCount_; i++)
            if (i != slot && !p_[i].down) anyAlive = true;
        if (anyAlive) {
            f.respawnT = 4.0f;
        } else {
            state_ = WorldState::Failed;
            stateT_ = 0;
        }
    }
}

void World::respawnPlayer(int slot) {
    Fighter& f = p_[slot];
    f.down = false;
    f.hp = 60.0f;
    f.deathT = 0;
    f.respawnT = 0;
    f.invuln = 2.0f;
    f.pos = (slot == 0) ? Vector2{460, 760} : Vector2{800, 860};
    f.lastY = f.pos.y;
    f.vyN = 0;
    f.dashT = 0;
    f.dashCd = 0;
    f.ghosts.clear();
    addExplosion(f.pos, 90, Color{150, 240, 255, 255}, 12);
}

std::string World::debugInfo() const {
    char b[640];
    int pb = 0, eb = 0;
    for (auto& x : bullets_) {
        if (!x.alive) continue;
        if (x.hostile) eb++; else pb++;
    }
    int pk = 0;
    for (auto& x : pickups_)
        if (x.alive) pk++;
    snprintf(b, sizeof b,
             "deathT=%.2f pLoad=%d pSkin=%d state=%d fin=%d wave=%d/%d phase=%d hp=%.0f lives=%d dash=%.2f/%.2f "
             "player=(%.0f,%.0f) p2=(%.0f,%.0f) hp2=%.0f down2=%d bullets=%d(p%d/e%d) enemies=%d left=%d boss=%d "
             "charging=%d score=%d bossSt=%d bossPos=(%.0f,%.0f) bossHp=%.2f pickups=%d drop=%d versus=%d winner=%d "
             "pcount=%d net=%d",
             p_[0].deathT, (int)assets::players[clampi(save::data().craft, 0, 5)].loaded,
             (int)assets::players[clampi(save::data().craft, 0, 5)].skin[save::data().skin].id,
             (int)state_, (int)finished_, std::min(wave_ + 1, kWaveCount), kWaveCount, wavePhase_, p_[0].hp,
             (int)p_[0].hp, p_[0].dashT, p_[0].dashCd, p_[0].pos.x, p_[0].pos.y, p_[1].pos.x, p_[1].pos.y,
             p_[1].hp, (int)p_[1].down, (int)bullets_.size(), pb, eb, (int)enemies_.size(), enemiesLeft(),
             (int)boss_.alive,
             (int)std::count_if(enemies_.begin(), enemies_.end(),
                                [](const Enemy& e) { return e.alive && e.st == ES_Charge; }),
             score_, boss_.st, boss_.pos.x, boss_.pos.y, bossHp01(), pk, (int)waveDrop_, (int)versus_, winner_,
             playerCount_, (int)online_);
    return b;
}

void World::killPlayerNow() {
    p_[0].invuln = 0;
    p_[0].hp = 1;
    damagePlayer(0, 5);
}

void World::startDash(int slot, Vector2 dir) {
    Fighter& f = p_[slot];
    float l = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (l < 0.0001f) { dir = {(float)f.face, 0}; l = 1.0f; }
    f.dashDir = {dir.x / l, dir.y / l};
    f.dashT = kDashDur;
    f.dashCd = kDashCd;
    f.invuln = std::max(f.invuln, kDashDur + 0.14f);
    f.ghostTimer = 0;
    audio::play(audio::SfxDash, 0.9f, 1.0f);
    for (int i = 0; i < 8; i++) {
        float back = rng_.range(0, 30);
        addTrail({f.pos.x - f.dashDir.x * back, f.pos.y - f.dashDir.y * back}, f.dashDir, pal::kCyan);
    }
}

void World::updatePlayer(int slot, float dt, const Ctrl& c, bool autopilot) {
    Fighter& f = p_[slot];
    if (!f.active) return;

    if (f.hitFlash > 0) f.hitFlash -= dt;
    if (f.bombFlash > 0) f.bombFlash -= dt;

    if (f.down) {
        if (f.deathT > 0) f.deathT -= dt;
        if (!versus_ && f.respawnT > 0) {
            f.respawnT -= dt;
            if (f.respawnT <= 0) respawnPlayer(slot);
        }
        f.prevBits = c.bits();
        if (slot == 0) lastInput_ = WorldInput{};
        return;
    }

    if (f.invuln > 0) f.invuln -= dt;
    if (f.dashCd > 0) f.dashCd = std::max(0.0f, f.dashCd - dt);

    // 按下沿由 World 自己算（联机只同步"按住"的 bit）
    uint16_t bits = c.bits();
    bool dashHit = (bits & 64) != 0 && (f.prevBits & 64) == 0;
    bool bombHit = (bits & 32) != 0 && (f.prevBits & 32) == 0;
    f.prevBits = bits;

    WorldInput wi;
    Vector2 dir{0, 0};

    if (scripted_) {
        // 复盘：本帧输入全部来自录像，绝不读键盘/鼠标
        wi = script_;
        dir = {wi.dirX, wi.dirY};
    } else if (autopilot) {
        // 自动演示：躲弹 + 躲冲锋 + 居中
        autopilotT_ -= dt;
        Vector2 repulse{0, 0};
        for (const auto& b : bullets_) {
            if (!b.alive) continue;
            float dx = f.pos.x - b.pos.x;
            if (dx < -40 || dx > 320) continue;
            float dy = f.pos.y - b.pos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > 210 * 210) continue;
            float d = std::sqrt(d2) + 0.01f;
            repulse.x += dx / d * (210 - d) * 0.6f;
            repulse.y += dy / d * (210 - d);
        }
        for (const auto& e : enemies_) {
            if (!e.alive || (e.st != ES_Charge && e.st != ES_Dash)) continue;
            float dx = f.pos.x - e.pos.x, dy = f.pos.y - e.pos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > 620 * 620) continue;
            float d = std::sqrt(d2) + 0.01f;
            repulse.x += dx / d * (620 - d) * 0.35f;
            repulse.y += dy / d * (620 - d) * 1.1f;
        }
        float targetY = clampf(f.pos.y + repulse.y * 0.06f, 320, 980);
        targetY = lerpf(targetY, 700, 0.35f);
        float targetX = clampf(460 + repulse.x * 0.02f, 240, 900);
        // 自动演示也要瞄准：否则永远打不到不同高度的敌机，波次卡死
        float aimY = -1e9f, bestD = 1e9f;
        for (const auto& e : enemies_) {
            if (!e.alive || e.delay > 0) continue;
            float d = std::fabs(e.pos.y - f.pos.y);
            if (d < bestD) { bestD = d; aimY = e.pos.y; }
        }
        if (boss_.alive) aimY = boss_.pos.y;
        if (aimY > -1e8f) targetY = clampf(lerpf(aimY, targetY, 0.25f), 320, 980);
        autoTarget_ = {targetX, targetY};

        Vector2 d{autoTarget_.x - f.pos.x, autoTarget_.y - f.pos.y};
        float l = std::sqrt(d.x * d.x + d.y * d.y);
        if (l > 2) { d.x /= l; d.y /= l; } else { d = {0, 0}; }
        dir = d;

        // 演示也会用闪身：站在锁定线上就横向闪开
        if (f.dashT <= 0 && f.dashCd <= 0) {
            for (const auto& e : enemies_) {
                if (!e.alive || e.st != ES_Charge) continue;
                Vector2 rel{f.pos.x - e.pos.x, f.pos.y - e.pos.y};
                float along = rel.x * e.lockDir.x + rel.y * e.lockDir.y;
                float perp = std::fabs(-rel.x * e.lockDir.y + rel.y * e.lockDir.x);
                if (along > 0 && perp < 80.0f) {
                    Vector2 pd{-e.lockDir.y, e.lockDir.x};
                    if (rel.x * pd.x + rel.y * pd.y < 0) { pd.x = -pd.x; pd.y = -pd.y; }
                    startDash(slot, pd);
                    break;
                }
            }
        }
    } else {
        if (c.left) dir.x -= 1;
        if (c.right) dir.x += 1;
        if (c.up) dir.y -= 1;
        if (c.down) dir.y += 1;
        if (dir.x != 0 && dir.y != 0) {
            dir.x *= 0.7071f;
            dir.y *= 0.7071f;
        }
        // 鼠标操控（仅单人）：按住左键时跟随光标；录下来的也是这个决定
        if (slot == 0 && playerCount_ == 1 && !online_ && app().in.lmb && app().in.mouseMoved) {
            wi.mouseSteer = true;
            wi.mouseX = app().in.mouse.x;
            wi.mouseY = app().in.mouse.y;
        }
        wi.fire = c.fire;
        wi.bomb = bombHit;
        wi.dash = dashHit;
    }

    // 鼠标跟随：实时与复盘共用这一段，跟随速率只由 dt 决定
    if (wi.mouseSteer) {
        float k = 1.0f - std::exp(-dt * 18.0f);
        f.pos.x = lerpf(f.pos.x, wi.mouseX, k);
        f.pos.y = lerpf(f.pos.y, wi.mouseY, k);
    }

    // ---- 闪身
    bool wantDash = scripted_ ? wi.dash : dashHit;
    if (!autopilot && wantDash && f.dashT <= 0 && f.dashCd <= 0) startDash(slot, dir);
    if (!scripted_) wi.dash = wantDash;  // 录进这一帧，复盘按同一帧重放

    if (f.dashT > 0) {
        f.dashT = std::max(0.0f, f.dashT - dt);
        f.pos.x += f.dashDir.x * kDashSpeed * dt;
        f.pos.y += f.dashDir.y * kDashSpeed * dt;
        f.ghostTimer -= dt;
        if (f.ghostTimer <= 0) {
            f.ghostTimer = 0.028f;
            f.ghosts.push_back({f.pos, f.pose, 0.30f, 0.30f, slot});
        }
    } else {
        f.pos.x += dir.x * kPlayerSpeed * dt;
        f.pos.y += dir.y * kPlayerSpeed * dt;
    }

    if (versus_) {
        // 对战：各守半场，中间留一条缓冲带
        if (slot == 0) f.pos.x = clampf(f.pos.x, 110, 900);
        else f.pos.x = clampf(f.pos.x, 1020, VW - 110);
    } else {
        f.pos.x = clampf(f.pos.x, kPlayerMinX, kPlayerMaxX);
    }
    f.pos.y = clampf(f.pos.y, 300, VH - 70);

    f.vel = {dir.x * kPlayerSpeed, dir.y * kPlayerSpeed};

    // 姿态 = 运动的结果：用真实位移反推垂直速度（键鼠 / 闪身一致）
    if (dt > 0.0001f) {
        float vyPx = (f.pos.y - f.lastY) / dt;
        float vyN = clampf(-vyPx / kPlayerSpeed, -1.0f, 1.0f);
        f.vyN = lerpf(f.vyN, vyN, 1.0f - std::exp(-dt * 18.0f));
    }
    f.lastY = f.pos.y;
    pose::Update(f.pose, pose::kPlayer, f.vyN, dt);

    // 射击
    bool firing = autopilot || (scripted_ ? wi.fire : c.fire);
    f.fireCd -= dt;
    if (firing && f.fireCd <= 0) {
        f.fireCd = 0.115f;
        int lvl = clampi(f.weapon, 1, 4);
        float bx = f.pos.x + 96.0f * f.face;
        Color bc = (slot == 1) ? Color{255, 196, 96, 255} : Color{140, 245, 255, 255};
        audio::play(audio::SfxShot, 0.28f, 0.94f + rng_.range(0, 0.12f));
        auto shot = [&](float oy, float vy) {
            addBullet({bx, f.pos.y + oy}, {1980.0f * f.face, vy}, 7.0f, bc, 2, 2.0f, false, false, slot);
        };
        if (lvl == 1) shot(0, 0);
        else if (lvl == 2) { shot(-16, 0); shot(16, 0); }
        else if (lvl == 3) { shot(0, 0); shot(-30, -90); shot(30, 90); }
        else { shot(-14, 0); shot(14, 0); shot(-42, -160); shot(42, 160); }
    }

    // 爆弹
    bool useBomb = scripted_ ? wi.bomb : bombHit;
    if (!autopilot && useBomb && f.bombs > 0) {
        f.bombs--;
        f.bombFlash = 0.9f;
        shake_ = std::max(shake_, 1.0f);
        audio::play(audio::SfxBomb, 1.0f, 1.0f);
        for (auto& b : bullets_) {
            if (!b.alive) continue;
            if (versus_) {
                if (b.hostile || b.owner == slot) continue;
            } else if (!b.hostile) {
                continue;
            }
            addHitSpark(b.pos, Color{200, 240, 255, 255});
            b.alive = false;
        }
        if (versus_) {
            int other = 1 - slot;
            if (p_[other].active) damagePlayerPvp(other, 12.0f);
        } else {
            for (size_t i = 0; i < enemies_.size(); i++) {
                Enemy& e = enemies_[i];
                if (!e.alive || e.delay > 0) continue;
                e.hitFlash = 0.2f;
                damageEnemy((int)i, 26);
            }
            if (boss_.alive) {
                boss_.hp -= 90;
                boss_.hitFlash = 0.25f;
                if (boss_.hp <= 0) killBoss();
            }
        }
    }
    if (!scripted_) wi.bomb = useBomb;

    // 记下本帧真正生效的输入（方向用解算后的值），录像直接抄这一份
    if (slot == 0) {
        lastInput_ = wi;
        lastInput_.dirX = dir.x;
        lastInput_.dirY = dir.y;
    }
}

// ---------------------------------------------------------------- 子弹
void World::updateBullets(float dt) {
    for (auto& b : bullets_) {
        if (!b.alive) continue;
        b.pos.x += b.vel.x * dt;
        b.pos.y += b.vel.y * dt;
        b.life -= dt;
        if (b.life <= 0 || b.pos.x < -160 || b.pos.x > VW + 260 || b.pos.y < -180 || b.pos.y > VH + 180)
            b.alive = false;
    }

    // 玩家弹 → 敌机
    for (auto& b : bullets_) {
        if (!b.alive || b.hostile) continue;
        for (size_t i = 0; i < enemies_.size(); i++) {
            Enemy& e = enemies_[i];
            if (!e.alive || e.delay > 0) continue;
            float rr = EnemySpan(e) * 0.34f;
            float dx = b.pos.x - e.pos.x, dy = b.pos.y - e.pos.y;
            if (dx * dx + dy * dy < rr * rr) {
                b.alive = false;
                damageEnemy((int)i, b.dmg);
                addHitSpark(b.pos, Color{255, 230, 160, 255});
                break;
            }
        }
        if (b.alive && boss_.alive) {
            float rr = 210.0f;
            float dx = b.pos.x - boss_.pos.x, dy = b.pos.y - boss_.pos.y;
            if (dx * dx + dy * dy < rr * rr) {
                b.alive = false;
                boss_.hp -= b.dmg;
                boss_.hitFlash = 0.09f;
                addHitSpark(b.pos, Color{255, 220, 150, 255});
                audio::play(audio::SfxHit, 0.18f, 1.2f);
                if (boss_.hp <= 0 && boss_.alive) killBoss();
            }
        }
    }

    // 玩家弹 → 对方（对战）
    if (versus_) {
        for (auto& b : bullets_) {
            if (!b.alive || b.hostile) continue;
            int t = 1 - b.owner;
            Fighter& f = p_[t];
            if (!f.active || f.down || f.invuln > 0) continue;
            float dx = b.pos.x - f.pos.x, dy = b.pos.y - f.pos.y;
            float rr = kPlayerR + b.r * 0.72f;
            if (dx * dx + dy * dy < rr * rr) {
                b.alive = false;
                damagePlayerPvp(t, kDmgPvp);
            }
        }
    } else {
        // 敌弹 → 玩家（两人都会被击中，各自吃自己的伤害）
        if (state_ == WorldState::Playing) {
            for (auto& b : bullets_) {
                if (!b.alive || !b.hostile) continue;
                for (int s = 0; s < playerCount_; s++) {
                    Fighter& f = p_[s];
                    if (f.down || f.invuln > 0) continue;
                    float dx = b.pos.x - f.pos.x, dy = b.pos.y - f.pos.y;
                    float rr = kPlayerR + b.r * 0.72f;
                    if (dx * dx + dy * dy < rr * rr) {
                        b.alive = false;
                        damagePlayer(s, b.dmg);
                        break;
                    }
                }
            }
        }
    }

    if (bullets_.size() > 64) {
        bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet& b) { return !b.alive; }),
                       bullets_.end());
    }
}

void World::damageEnemy(int idx, float dmg) {
    Enemy& e = enemies_[idx];
    if (!e.alive) return;
    e.hp -= dmg;
    e.hitFlash = 0.08f;
    if (e.hp <= 0) {
        e.alive = false;
        e.st = ES_Dead;
        kills_++;
        addScore(e.score);
        float sz = EnemySpan(e);
        addExplosion(e.pos, sz * 0.8f, Color{255, 168, 80, 255}, 18);
        audio::play(audio::SfxExplode, 0.55f, 0.95f + rng_.range(0, 0.2f));
        shake_ = std::max(shake_, 0.28f);
        dropPickup(e.pos);
    }
}

void World::ramExplode(Enemy& e) {
    e.alive = false;
    e.st = ES_Dead;
    kills_++;
    addScore(e.score);
    addExplosion(e.pos, 240, Color{255, 150, 90, 255}, 32);
    audio::play(audio::SfxExplode, 0.95f, 0.8f);
    shake_ = std::max(shake_, 0.95f);
    flash_ = 0.35f;
    dropPickup(e.pos);
}

void World::killBoss() {
    boss_.alive = false;
    boss_.st = ES_Dead;
    score_ += 15000;
    kills_++;
    shake_ = 1.4f;
    flash_ = 0.9f;
    audio::play(audio::SfxBigExplode, 1.0f, 0.8f);
    for (int k = 0; k < 8; k++) {
        Vector2 p = {boss_.pos.x + rng_.range(-200, 200), boss_.pos.y + rng_.range(-180, 180)};
        addExplosion(p, 240, Color{255, 150, 90, 255}, 30);
    }
    // Boss 必掉两颗（火力 + 爆弹）。位置钳进画面：它可能刚好死在出画途中，
    // 掉落物丢在 x=-800 就直接被剔除，玩家白打一场。
    Vector2 drop{clampf(boss_.pos.x, 280.0f, 1480.0f), clampf(boss_.pos.y, 240.0f, 900.0f)};
    spawnPickup({drop.x - 60, drop.y - 90}, 0);
    spawnPickup({drop.x - 60, drop.y + 90}, 1);
    waveDrop_ = true;
}

// ---------------------------------------------------------------- 敌机
// 瞬移换场（冲空后从右侧重新进场）。位置可以瞬移，姿态不能跟着瞬移：
// 位移差会被当成速度，一帧就能把倾角顶到极限，看起来就是"突然摆正"。
void World::placeEnemy(Enemy& e, Vector2 p) {
    e.pos = p;
    e.lastY = p.y;
    e.posJump = true;
}

// 姿态 = 轨迹的结果。这里对垂直速度先做一次平滑再喂给 pose，
// 避免入场/回场/起冲那几帧的速度突变把机体甩一下。
void World::updateEnemyPose(Enemy& e, float dt) {
    if (dt <= 0.0001f) return;
    float vy = (e.pos.y - e.lastY) / dt;
    float target = clampf(-vy / 320.0f, -1.0f, 1.0f);
    if (e.posJump) {  // 这一帧是瞬移，位移不是真实速度
        target = 0.0f;
        e.posJump = false;
    }
    e.vyN += (target - e.vyN) * (1.0f - std::exp(-dt * 9.0f));
    pose::Update(e.pose, pose::kEnemy, e.vyN, dt);
    e.lastY = e.pos.y;
}

void World::enemyShoot(const Enemy& e) {
    Vector2 tgt = aimTarget(e.pos);
    Vector2 aim{tgt.x - e.pos.x, tgt.y - e.pos.y};
    float base = std::atan2(aim.y, aim.x);
    switch (e.cls) {
        case EC_C:
            addBullet(e.pos, {std::cos(base) * 520, std::sin(base) * 520}, 9, BulletColor(0), 0, kDmgBullet, true);
            break;
        case EC_B:
            addBurst({e.pos.x - 40, e.pos.y}, 3, 470, 0.22f, BulletColor(1), 10, base, kDmgBullet, true);
            break;
        default:
            addBurst(e.pos, 3, 430, 0.20f, BulletColor(1), 10, base, kDmgBullet, true);
            break;
    }
    audio::play(audio::SfxHit, 0.1f, 0.7f);
}

void World::beginCharge(Enemy& e) {
    Vector2 tgt = aimTarget(e.pos);
    Vector2 d{tgt.x - e.pos.x, tgt.y - e.pos.y};
    float l = std::sqrt(d.x * d.x + d.y * d.y);
    if (l < 1.0f) { d = {-1, 0}; l = 1.0f; }
    e.lockDir = {d.x / l, d.y / l};
    e.st = ES_Charge;
    e.stateT = 0;
    audio::play(audio::SfxCharge, 0.8f, 1.0f);
}

void World::updateC(Enemy& e, float dt) {
    switch (e.st) {
        case ES_Enter: {
            float k = easeOutCubic(clampf(e.stateT / 0.95f, 0, 1));
            e.pos.x = lerpf(e.spawn.x, e.home.x, k);
            e.pos.y = lerpf(e.spawn.y, e.home.y, k);
            if (e.stateT >= 0.95f) {
                e.st = ES_Patrol;
                e.stateT = 0;
                e.fireT = rng_.range(0.7f, 1.5f);
            }
            break;
        }
        case ES_Patrol: {
            // 在右半场里绕锚点游走，绝不追着玩家跑
            Vector2 orbit{e.home.x + std::sin(e.t * 0.62f + e.phase) * 58.0f,
                          e.home.y + std::sin(e.t * 1.05f + e.phase * 1.7f) * 92.0f};
            OrbitTo(e.pos, orbit, dt, 4.5f);
            e.fireT -= dt;
            if (e.fireT <= 0) {
                enemyShoot(e);
                e.fireT = rng_.range(1.6f, 2.3f);
            }
            break;
        }
        default: break;
    }
}

void World::updateB(Enemy& e, float dt) {
    switch (e.st) {
        case ES_Enter: {
            float k = easeOutCubic(clampf(e.stateT / 1.15f, 0, 1));
            e.pos.x = lerpf(e.spawn.x, e.home.x, k);
            e.pos.y = lerpf(e.spawn.y, e.home.y, k);
            if (e.stateT >= 1.15f) {
                e.st = ES_Patrol;
                e.stateT = 0;
                e.fireT = rng_.range(1.0f, 1.8f);
            }
            break;
        }
        case ES_Patrol: {
            Vector2 orbit{e.home.x + std::sin(e.t * 0.42f + e.phase) * 32.0f,
                          e.home.y + std::sin(e.t * 0.74f + e.phase) * 76.0f};
            OrbitTo(e.pos, orbit, dt, 3.6f);

            if (e.tell > 0) {
                e.tell -= dt;
                if (e.tell <= 0) {
                    // 大炮弹：慢、大、1/4 伤害
                    Vector2 tgt = aimTarget(e.pos);
                    Vector2 aim{tgt.x - e.pos.x, tgt.y - e.pos.y};
                    float base = std::atan2(aim.y, aim.x);
                    addBullet({e.pos.x - 50, e.pos.y}, {std::cos(base) * 330, std::sin(base) * 330}, 30,
                              Color{255, 128, 40, 255}, 1, kDmgShell, true, true);
                    audio::play(audio::SfxExplode, 0.35f, 1.5f);
                    shake_ = std::max(shake_, 0.2f);
                }
            }
            e.specialT -= dt;
            if (e.specialT <= 0 && e.tell <= 0) {
                e.tell = 0.55f;
                e.specialT = rng_.range(5.4f, 7.0f);
                audio::play(audio::SfxCharge, 0.45f, 1.35f);
            }

            e.fireT -= dt;
            if (e.fireT <= 0) {
                enemyShoot(e);
                e.fireT = rng_.range(2.0f, 2.6f);
            }
            break;
        }
        default: break;
    }
}

void World::updateA(Enemy& e, float dt) {
    switch (e.st) {
        case ES_Enter: {
            float k = easeOutCubic(clampf(e.stateT / 0.85f, 0, 1));
            e.pos.x = lerpf(e.spawn.x, e.home.x, k);
            e.pos.y = lerpf(e.spawn.y, e.home.y, k);
            if (e.stateT >= 0.85f) {
                e.st = ES_Patrol;
                e.stateT = 0;
                e.specialT = rng_.range(2.2f, 3.4f);
            }
            break;
        }
        case ES_Patrol: {
            Vector2 orbit{e.home.x + std::sin(e.t * 0.55f + e.phase) * 52.0f,
                          e.home.y + std::sin(e.t * 0.90f + e.phase * 1.3f) * 84.0f};
            OrbitTo(e.pos, orbit, dt, 4.2f);
            e.specialT -= dt;
            if (e.specialT <= 0 && p_[nearestPlayer(e.pos)].pos.x < e.pos.x - 260.0f) beginCharge(e);
            break;
        }
        case ES_Charge: {
            // 锁定方向在蓄力开始就固定：玩家只要离开这条线就能躲掉
            float p = clampf(e.stateT / kChargeTime, 0, 1);
            e.pos.x += std::sin(e.t * 46.0f) * 1.6f * (0.35f + p);
            if (e.stateT >= kChargeTime) {
                e.st = ES_Dash;
                e.stateT = 0;
                e.vel = {e.lockDir.x * kRamSpeed, e.lockDir.y * kRamSpeed};
                audio::play(audio::SfxDash, 0.7f, 0.62f);
                addExplosion(e.pos, 70, Color{255, 170, 120, 255}, 8);
            }
            break;
        }
        case ES_Dash: {
            e.pos.x += e.vel.x * dt;
            e.pos.y += e.vel.y * dt;
            for (int i = 0; i < 2; i++) addTrail(e.pos, e.lockDir, Color{255, 120, 90, 255});

            for (int s = 0; s < playerCount_; s++) {
                Fighter& f = p_[s];
                if (f.down) continue;
                float dx = e.pos.x - f.pos.x, dy = e.pos.y - f.pos.y;
                if (dx * dx + dy * dy < 92.0f * 92.0f) {
                    damagePlayer(s, kDmgRam);
                    ramExplode(e);
                    return;
                }
            }
            if (e.pos.x < -320 || e.pos.y < -340 || e.pos.y > VH + 340 || e.stateT > 2.6f) {
                // 冲空了：不按原路折返（那样会背朝玩家倒着飞回来），
                // 让它继续往画面外飞掉，然后从右侧重新进场（不消失，波次才能被清空）
                e.home = {rng_.range(1310.0f, 1650.0f), rng_.range(290.0f, 800.0f)};
                e.spawn = {VW + 280.0f, e.home.y};
                placeEnemy(e, e.spawn);
                e.vel = {0, 0};
                e.st = ES_Enter;
                e.stateT = 0;
            }
            break;
        }
        default: break;
    }
}

void World::updateEnemies(float dt) {
    for (auto& e : enemies_) {
        if (!e.alive) continue;
        if (e.delay > 0) {
            e.delay -= dt;
            continue;
        }
        if (e.st == ES_Wait) {
            e.st = ES_Enter;
            e.stateT = 0;
        }
        e.t += dt;
        e.stateT += dt;
        if (e.hitFlash > 0) e.hitFlash -= dt;

        float target = clampf(e.hp / e.maxHp, 0, 1);
        e.hpTrail = (e.hpTrail > target) ? approach(e.hpTrail, target, dt * 0.85f) : target;

        switch (e.cls) {
            case EC_C: updateC(e, dt); break;
            case EC_B: updateB(e, dt); break;
            default:   updateA(e, dt); break;
        }
        if (!e.alive) continue;

        // 姿态：与玩家共用同一套公式，倾角跟着轨迹起伏（内部做了速度平滑）
        updateEnemyPose(e, dt);
    }
    if (enemies_.size() > 40)
        enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) { return !e.alive; }),
                       enemies_.end());
}

bool World::enemiesAllDead() const {
    for (const auto& e : enemies_)
        if (e.alive) return false;
    return true;
}

// ---------------------------------------------------------------- Boss
void World::bossShoot(int pattern) {
    Vector2 tgt = aimTarget(boss_.pos);
    Vector2 aim{tgt.x - boss_.pos.x, tgt.y - boss_.pos.y};
    float base = std::atan2(aim.y, aim.x);
    switch (pattern) {
        case 0:  // 三向
            addBurst({boss_.pos.x - 90, boss_.pos.y + 30}, 3, 520, 0.26f, BulletColor(1), 13, base, kDmgBullet, true);
            break;
        case 1:  // 五向
            addBurst({boss_.pos.x - 90, boss_.pos.y + 30}, 5, 470, 0.62f, BulletColor(0), 13, base, kDmgBullet, true);
            break;
        default:  // 大炮弹
            addBullet({boss_.pos.x - 130, boss_.pos.y + 10}, {std::cos(base - 0.10f) * 320, std::sin(base - 0.10f) * 320},
                      34, Color{255, 128, 40, 255}, 1, kDmgShell, true, true);
            addBullet({boss_.pos.x - 130, boss_.pos.y + 10}, {std::cos(base + 0.10f) * 320, std::sin(base + 0.10f) * 320},
                      34, Color{255, 128, 40, 255}, 1, kDmgShell, true, true);
            break;
    }
    audio::play(audio::SfxHit, 0.16f, 0.62f);
}

void World::beginBossCharge() {
    Vector2 tgt = aimTarget(boss_.pos);
    Vector2 d{tgt.x - boss_.pos.x, tgt.y - boss_.pos.y};
    float l = std::sqrt(d.x * d.x + d.y * d.y);
    if (l < 1.0f) { d = {-1, 0}; l = 1.0f; }
    boss_.lockDir = {d.x / l, d.y / l};
    boss_.st = ES_Charge;
    boss_.stateT = 0;
    audio::play(audio::SfxCharge, 1.0f, 0.72f);
}

int World::pickBossAttack(float hp01) {
    // 0 三向 1 五向 2 大炮弹 3 冲撞
    float r = rng_.f();
    if (hp01 > 0.66f) return (r < 0.40f) ? 0 : (r < 0.66f ? 1 : (r < 0.90f ? 2 : 3));
    if (hp01 > 0.33f) return (r < 0.28f) ? 0 : (r < 0.56f ? 1 : (r < 0.82f ? 2 : 3));
    return (r < 0.20f) ? 0 : (r < 0.46f ? 1 : (r < 0.72f ? 2 : 3));
}

void World::updateBoss(float dt) {
    if (!boss_.alive) return;
    boss_.t += dt;
    boss_.stateT += dt;
    if (boss_.hitFlash > 0) boss_.hitFlash -= dt;

    float hp01 = bossHp01();
    boss_.hpTrail = (boss_.hpTrail > hp01) ? approach(boss_.hpTrail, hp01, dt * 0.8f) : hp01;

    switch (boss_.st) {
        case ES_Enter: {
            float dur = std::max(bossEnterDur_, 0.2f);
            float k = easeOutCubic(clampf(boss_.stateT / dur, 0, 1));
            boss_.pos.x = lerpf(boss_.spawn.x, boss_.home.x, k);
            boss_.pos.y = lerpf(boss_.spawn.y, boss_.home.y, k);
            if (boss_.stateT >= dur) {
                boss_.st = ES_Patrol;
                boss_.stateT = 0;
                boss_.fireT = 1.4f;
            }
            break;
        }
        case ES_Charge: {
            if (boss_.stateT >= kBossChargeTime) {
                boss_.st = ES_Dash;
                boss_.stateT = 0;
                boss_.vel = {boss_.lockDir.x * 1250.0f, boss_.lockDir.y * 1250.0f};
                audio::play(audio::SfxDash, 0.85f, 0.55f);
                shake_ = std::max(shake_, 0.5f);
            }
            break;
        }
        case ES_Dash: {
            boss_.pos.x += boss_.vel.x * dt;
            boss_.pos.y += boss_.vel.y * dt;
            for (int i = 0; i < 3; i++) addTrail(boss_.pos, boss_.lockDir, Color{255, 120, 90, 255});
            for (int s = 0; s < playerCount_; s++) {
                Fighter& f = p_[s];
                if (f.down) continue;
                float dx = boss_.pos.x - f.pos.x, dy = boss_.pos.y - f.pos.y;
                if (dx * dx + dy * dy < 230.0f * 230.0f) damagePlayer(s, kDmgRam);
            }
            if (boss_.pos.x < -460 || boss_.pos.y < -460 || boss_.pos.y > VH + 460 || boss_.stateT > 2.8f) {
                // 不原路折返（那会让这个大家伙背朝玩家倒着飞回来）：
                // 冲撞就是一路冲出画面，再从右侧重新压进来，和冲锋机一个路子。
                boss_.spawn = {VW + 520.0f, boss_.home.y};
                placeEnemy(boss_, boss_.spawn);
                boss_.vel = {0, 0};
                boss_.st = ES_Enter;
                boss_.stateT = 0;
                bossEnterDur_ = 1.35f;
                boss_.fireT = 1.2f;  // 回来先喘一口，别一进场就压枪
            }
            break;
        }
        default: {
            Vector2 orbit{boss_.home.x + std::sin(boss_.t * 0.45f) * 36.0f,
                          boss_.home.y + std::sin(boss_.t * 0.70f) * 92.0f};
            OrbitTo(boss_.pos, orbit, dt, 2.8f);

            if (boss_.tell > 0) {
                boss_.tell -= dt;
                if (boss_.tell <= 0) bossShoot(2);
            }
            boss_.fireT -= dt;
            if (boss_.fireT <= 0 && boss_.tell <= 0) {
                int pick = pickBossAttack(hp01);
                float interval = (hp01 > 0.66f) ? 1.5f : (hp01 > 0.33f ? 1.15f : 0.85f);
                if (pick == 3) {
                    beginBossCharge();
                } else if (pick == 2) {
                    boss_.tell = 0.6f;
                    boss_.fireT = interval + 0.4f;
                } else {
                    bossShoot(pick);
                    boss_.fireT = interval;
                }
            }
            break;
        }
    }

    updateEnemyPose(boss_, dt);
}

// ---------------------------------------------------------------- 粒子/拾取
void World::updateParticles(float dt) {
    for (auto& p : particles_) {
        if (!p.alive) continue;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.x *= (1.0f - 2.2f * dt);
        p.vel.y *= (1.0f - 2.2f * dt);
        if (p.kind == 2) p.vel.y -= 24.0f * dt;
        p.life -= dt;
        if (p.life <= 0) p.alive = false;
    }
    if (particles_.size() > 220)
        particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                        [](const Particle& p) { return !p.alive; }),
                         particles_.end());

    for (int i = 0; i < kMaxPlayers; i++) {
        auto& gs = p_[i].ghosts;
        for (auto& g : gs) g.life -= dt;
        if (!gs.empty())
            gs.erase(std::remove_if(gs.begin(), gs.end(), [](const Ghost& g) { return g.life <= 0; }), gs.end());
    }
}

void World::updatePickups(float dt) {
    for (auto& p : pickups_) {
        if (!p.alive) continue;
        p.t += dt;

        int tgt = nearestPlayer(p.pos);
        Fighter& f = p_[tgt];

        // 出场先按初速弹向玩家，0.22s 后打开弱磁吸：
        // 敌机全在右半场、玩家上限 x=1150，不吸的话道具永远停在够不到的地方。
        bool magnetOn = (p.t > 0.22f) && !f.down && state_ == WorldState::Playing;
        if (magnetOn) {
            Vector2 d{f.pos.x - p.pos.x, f.pos.y - p.pos.y};
            float l = std::sqrt(d.x * d.x + d.y * d.y);
            if (l > 1.0f) {
                const float acc = 380.0f;
                p.vel.x += d.x / l * acc * dt;
                p.vel.y += d.y / l * acc * dt;
            }
            float sp = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
            const float spMax = 560.0f;
            if (sp > spMax) {
                p.vel.x *= spMax / sp;
                p.vel.y *= spMax / sp;
            }
            p.vel.x *= (1.0f - 0.9f * dt);
            p.vel.y *= (1.0f - 0.9f * dt);
        } else {
            p.vel.x *= (1.0f - 2.0f * dt);
            p.vel.y *= (1.0f - 2.0f * dt);
            p.pos.y += std::sin(p.t * 3.0f) * 18.0f * dt;
        }

        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;

        if (p.pos.x < -120 || p.t > 9.0f) p.alive = false;
        if (state_ != WorldState::Playing) continue;

        // 谁碰到归谁：两人的磁吸都指向最近的玩家，但最终判定看各自的机身
        for (int s = 0; s < playerCount_; s++) {
            Fighter& g = p_[s];
            if (g.down) continue;
            float dx = p.pos.x - g.pos.x, dy = p.pos.y - g.pos.y;
            if (dx * dx + dy * dy < 78 * 78) {
                p.alive = false;
                audio::play(audio::SfxPickup, 0.7f, 1.0f);
                if (p.kind == 0) g.weapon = clampi(g.weapon + 1, 1, 4);
                else if (p.kind == 1) g.bombs = clampi(g.bombs + 1, 0, 5);
                else g.hp = std::min(kPlayerMaxHp, g.hp + 35.0f);
                score_ += 200;
                break;
            }
        }
    }
    if (pickups_.size() > 12)
        pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& p) { return !p.alive; }),
                       pickups_.end());
}

// ---------------------------------------------------------------- 波次
void World::updateWave(float dt) {
    if (state_ != WorldState::Playing || versus_) return;

    if (bossPending_) {
        bossDelay_ -= dt;
        if (bossDelay_ <= 0) {
            bossPending_ = false;
            activateBoss();
        }
    }

    switch (wavePhase_) {
        case WP_Banner:
            waveT_ -= dt;
            if (waveT_ <= 0) {
                spawnWave(wave_);
                wavePhase_ = WP_Fight;
                waveT_ = 0;
            }
            break;

        case WP_Fight:
            // 关键规则：必须打完本波所有敌人（含 Boss）才允许进入下一波
            if (!bossPending_ && !boss_.alive && enemiesAllDead()) {
                // 兜底：万一整波一颗都没掉（例如全靠冲撞撞死），清场时补一颗
                if (!waveDrop_) {
                    float py = p_[nearestPlayer({1060.0f, VH * 0.5f})].pos.y;
                    spawnPickup({1060.0f, clampf(py, 320.0f, VH - 160.0f)}, 0);
                }
                wavePhase_ = WP_Gap;
                waveT_ = 2.0f;
                showBanner("WAVE CLEAR", 1.5f);
                score_ += 600 * (wave_ + 1);
                audio::play(audio::SfxUiSelect, 0.7f, 1.1f);
            }
            break;

        default:  // WP_Gap
            waveT_ -= dt;
            if (waveT_ <= 0) {
                wave_++;
                if (wave_ >= kWaveCount) {
                    state_ = WorldState::Cleared;
                    stateT_ = 0;
                    audio::play(audio::SfxUiSelect, 0.9f, 0.9f);
                } else {
                    wavePhase_ = WP_Banner;
                    waveT_ = 1.5f;
                    showBanner((wave_ == kWaveCount - 1) ? "FINAL WAVE" : ("WAVE " + std::to_string(wave_ + 1)), 1.5f);
                }
            }
            break;
    }
}

// ---------------------------------------------------------------- 主更新
void World::step(float dt, const TickInput& in, bool autopilot) {
    elapsed_ += dt;
    stateT_ += dt;
    if (bannerT_ > 0) bannerT_ -= dt;
    if (comboT_ > 0) {
        comboT_ -= dt;
        if (comboT_ <= 0) combo_ = 0;
    }
    if (shake_ > 0) shake_ -= dt * 2.4f;
    if (flash_ > 0) flash_ -= dt * 2.2f;

    bg_.setSpeed((wave_ >= kWaveCount - 1) ? 300.0f : 230.0f);
    bg_.update(dt);
    bgStorm_.setSpeed(340.0f);
    bgStorm_.update(dt);

    // 终局：雷暴压上来（对战不开雷暴，画面保持干净）
    float stormTarget = (!versus_ && wave_ >= kWaveCount - 1 && state_ == WorldState::Playing) ? 1.0f : stormMix_;
    stormMix_ = approach(stormMix_, stormTarget, dt * 0.55f);

    for (int i = 0; i < playerCount_; i++)
        updatePlayer(i, dt, in.p[i], autopilot && i == 0);

    updateBullets(dt);
    updateEnemies(dt);
    updateBoss(dt);
    updateParticles(dt);
    updatePickups(dt);
    updateWave(dt);

    if (state_ == WorldState::Cleared) {
        float need = versus_ ? 2.2f : 2.6f;
        if (stateT_ > need && !finished_) finished_ = true;
    } else if (state_ == WorldState::Failed) {
        if (stateT_ > 1.6f && !finished_) finished_ = true;
    }
}

// ---------------------------------------------------------------- 绘制
void World::drawShadow(Vector2 pos, float w, float h, float alpha) {
    float sx = pos.x + 10, sy = pos.y + 46;
    for (int i = 3; i >= 1; i--) {
        float k = i / 3.0f;
        DrawEllipse((int)sx, (int)sy, (int)(w * 0.5f * k), (int)(h * 0.5f * k), WithAlpha(BLACK, alpha * 0.18f));
    }
}

void World::drawBackground() {
    Vector2 sh = shakeOffset();

    rlPushMatrix();
    rlTranslatef(sh.x, sh.y, 0);
    bg_.setAlpha(1.0f);
    bg_.draw();          // 天空 / 远景云 / 海面
    bg_.drawProps();     // 岛、航母
    bg_.drawNearClouds();  // 近景云压在最上面（含海上的舰船），做出"穿云"的层次
    if (stormMix_ > 0.0f) {
        bgStorm_.setAlpha(stormMix_);
        bgStorm_.draw();
    }
    rlPopMatrix();

    rlPushMatrix();
    rlTranslatef(sh.x, sh.y, 0);
    for (auto& e : enemies_) {
        if (!e.alive || e.delay > 0) continue;
        float span = EnemySpan(e);
        drawShadow(e.pos, span * 0.75f, span * 0.3f, 0.9f);
    }
    if (boss_.alive) drawShadow(boss_.pos, 620, 240, 1.0f);
    for (int i = 0; i < playerCount_; i++)
        if (!p_[i].down) drawShadow(p_[i].pos, kPlayerSpan * 0.75f, kPlayerSpan * 0.3f, 1.0f);
    rlPopMatrix();
}

void World::draw3D() {
    Vector2 sh = shakeOffset();

    // 玩家机体 + 闪身残影
    for (int i = 0; i < playerCount_; i++) {
        Fighter& f = p_[i];
        Color trailCol = (i == 1) ? pal::kGold : pal::kCyan;
        for (auto& g : f.ghosts) {
            float k = clampf(g.life / g.maxLife, 0, 1);
            assets::drawCraft(assets::players[clampi(f.craft, 0, 5)], g.pos.x + sh.x, g.pos.y + sh.y, f.skin,
                              g.pose, kPlayerSpan, WithAlpha(trailCol, 0.30f * k), f.face);
        }
        if (f.down) continue;
        bool blink = (f.invuln > 0) && (((int)(GetTime() * 14)) % 2 == 0);
        Color tint = blink ? WithAlpha(WHITE, 0.45f) : WHITE;
        if (f.hitFlash > 0) tint = Color{255, 232, 210, 255};
        assets::drawCraft(assets::players[clampi(f.craft, 0, 5)], f.pos.x + sh.x, f.pos.y + sh.y, f.skin, f.pose,
                          kPlayerSpan, tint, f.face);
    }

    for (auto& e : enemies_) {
        if (!e.alive || e.delay > 0) continue;
        float span = EnemySpan(e);
        Color tint = e.hitFlash > 0 ? Color{255, 235, 220, 255} : WHITE;
        float jx = 0, jy = 0;
        if (e.st == ES_Charge) {
            float p = clampf(e.stateT / kChargeTime, 0, 1);
            float amp = 2.0f + 7.0f * p;
            jx = std::sin(e.t * 62.0f) * amp;
            jy = std::cos(e.t * 74.0f) * amp;
        }
        assets::drawCraft(assets::enemies[kEnemy[e.cls].craft], e.pos.x + sh.x + jx, e.pos.y + sh.y + jy,
                          assets::SKIN_RED, e.pose, span, tint, -1);
    }

    if (boss_.alive) {
        Color tint = boss_.hitFlash > 0 ? Color{255, 230, 210, 255} : WHITE;
        float jx = 0, jy = 0;
        if (boss_.st == ES_Charge) {
            float p = clampf(boss_.stateT / kBossChargeTime, 0, 1);
            float amp = 3.0f + 9.0f * p;
            jx = std::sin(boss_.t * 58.0f) * amp;
            jy = std::cos(boss_.t * 70.0f) * amp;
        }
        assets::drawCraft(assets::boss, boss_.pos.x + sh.x + jx, boss_.pos.y + sh.y + jy, assets::SKIN_RED, boss_.pose,
                          620.0f, tint, -1);
    }
}

// 冲锋 / 冲撞的预警：危险走廊 + 收缩圆环 + 警示牌
void World::drawTelegraphs() {
    float now = (float)GetTime();
    for (auto& e : enemies_) {
        if (!e.alive || e.st != ES_Charge) continue;
        float p = clampf(e.stateT / kChargeTime, 0, 1);
        float a = 0.6f + 0.4f * (0.5f + 0.5f * std::sin(now * 26.0f));
        float span = EnemySpan(e);

        DrawAimLine(e.pos, e.lockDir, pal::kDanger, 0.85f + 0.15f * p);


        // 机体上的红色锁定光晕
        DrawCircleV(e.pos, span * 0.52f, WithAlpha(pal::kDanger, 0.08f + 0.12f * p));
        for (int i = 0; i < 4; i++)
            DrawCircleLines((int)e.pos.x, (int)e.pos.y, (int)(span * 0.52f) + i * 3,
                            WithAlpha(pal::kDanger, (0.5f + 0.4f * a) * (1.0f - i * 0.2f)));

        // 收缩的蓄力环
        float ring = lerpf(320.0f, 96.0f, p);
        for (int i = 0; i < 3; i++)
            DrawCircleLines((int)e.pos.x, (int)e.pos.y, (int)ring + i * 2, WithAlpha(pal::kDanger, 0.45f + 0.4f * a));

        // 预警三角 + 锁定文字
        float ty = e.pos.y - span * 0.52f - 78.0f;
        if (ty < 96.0f) ty = e.pos.y + span * 0.52f + 78.0f;
        DrawWarnMark(e.pos.x, ty, 34.0f, a);
        ui::TextC("锁定", e.pos.x, ty - 62.0f, 26, WithAlpha(pal::kDanger, a), true);
    }

    if (boss_.alive && boss_.st == ES_Charge) {
        float p = clampf(boss_.stateT / kBossChargeTime, 0, 1);
        float a = 0.6f + 0.4f * (0.5f + 0.5f * std::sin(now * 24.0f));
        DrawAimLine(boss_.pos, boss_.lockDir, pal::kDanger, 0.8f + 0.2f * p);
        float ring = lerpf(520.0f, 230.0f, p);
        DrawCircleLines((int)boss_.pos.x, (int)boss_.pos.y, ring, WithAlpha(pal::kDanger, 0.3f + 0.4f * p));
        DrawCircleLines((int)boss_.pos.x, (int)boss_.pos.y, ring * 0.7f, WithAlpha(pal::kDanger, 0.16f + 0.28f * p));
        DrawWarnMark(boss_.pos.x, boss_.pos.y - 300.0f, 42.0f, a);
        ui::TextC("锁定", boss_.pos.x, boss_.pos.y - 372.0f, 30, WithAlpha(pal::kDanger, a), true);
    }

    // 大炮弹的开火预告：枪口聚能
    for (auto& e : enemies_) {
        if (!e.alive || e.tell <= 0) continue;
        float k = 1.0f - clampf(e.tell / 0.55f, 0, 1);
        float r = 18.0f + 46.0f * k;
        DrawCircleV({e.pos.x - 46, e.pos.y}, r, WithAlpha(Color{255, 120, 40, 255}, 0.18f + 0.25f * k));
        DrawCircleLines((int)(e.pos.x - 46), (int)e.pos.y, r, WithAlpha(pal::kGold, 0.5f + 0.5f * k));
    }
    if (boss_.alive && boss_.tell > 0) {
        float k = 1.0f - clampf(boss_.tell / 0.6f, 0, 1);
        float r = 26.0f + 70.0f * k;
        DrawCircleV({boss_.pos.x - 130, boss_.pos.y + 10}, r, WithAlpha(Color{255, 120, 40, 255}, 0.18f + 0.28f * k));
        DrawCircleLines((int)(boss_.pos.x - 130), (int)(boss_.pos.y + 10), r, WithAlpha(pal::kGold, 0.5f + 0.5f * k));
    }
}

void World::drawBullets() {
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto& b : bullets_) {
        if (!b.alive) continue;
        if (!b.hostile) {
            DrawCircle((int)(b.pos.x + 2), (int)b.pos.y, 16.0f, WithAlpha(Color{80, 200, 255, 255}, 0.28f));
            DrawRectangleGradientH((int)(b.pos.x - 96), (int)(b.pos.y - 9), 104, 18,
                                   WithAlpha(Color{40, 150, 220, 255}, 0.0f),
                                   WithAlpha(Color{120, 230, 255, 255}, 0.32f));
        } else {
            DrawCircle((int)b.pos.x, (int)b.pos.y, b.r * 2.6f, WithAlpha(b.col, b.big ? 0.42f : 0.3f));
        }
    }
    EndBlendMode();

    for (auto& b : bullets_) {
        if (!b.alive) continue;
        if (!b.hostile) {
            DrawRectangleGradientH((int)(b.pos.x - 70), (int)(b.pos.y - 5), 78, 10,
                                   WithAlpha(Color{110, 225, 255, 255}, 0.0f), Color{150, 240, 255, 255});
            DrawRectangle((int)(b.pos.x - 18), (int)(b.pos.y - 3), 26, 6, Color{236, 255, 255, 255});
            DrawCircle((int)(b.pos.x + 6), (int)b.pos.y, 6.0f, Color{245, 255, 255, 255});
        } else if (b.big) {
            // 大炮弹：黑边 + 熔岩内核 + 外圈脉冲
            float pulse = 1.0f + 0.06f * std::sin((float)GetTime() * 18.0f + b.pos.x);
            DrawCircle((int)b.pos.x, (int)b.pos.y, b.r * 1.12f * pulse, WithAlpha(Color{28, 6, 8, 255}, 0.75f));
            DrawCircle((int)b.pos.x, (int)b.pos.y, b.r * pulse, Color{255, 118, 36, 255});
            DrawCircle((int)b.pos.x, (int)b.pos.y, b.r * 0.72f, Color{255, 186, 76, 255});
            DrawCircle((int)b.pos.x, (int)b.pos.y, b.r * 0.38f, Color{255, 248, 224, 255});
            DrawCircleLines((int)b.pos.x, (int)b.pos.y, b.r * 1.35f * pulse, WithAlpha(pal::kDanger, 0.55f));
        } else {
            DrawCircle((int)b.pos.x, (int)b.pos.y, b.r + 4.0f, WithAlpha(Color{32, 8, 10, 255}, 0.62f));
            DrawCircle((int)b.pos.x, (int)b.pos.y, b.r, b.col);
            DrawCircle((int)(b.pos.x - b.r * 0.18f), (int)(b.pos.y - b.r * 0.2f), b.r * 0.34f,
                       Color{255, 246, 232, 255});
        }
    }
}

void World::drawParticles() {
    for (auto& p : particles_) {
        if (!p.alive) continue;
        float k = clampf(p.life / p.maxLife, 0, 1);
        if (p.kind == 2) {
            DrawCircle((int)p.pos.x, (int)p.pos.y, p.size * (2.2f - k), WithAlpha(p.col, p.col.a / 255.0f * k * 0.5f));
        } else if (p.kind == 1) {
            DrawRectangle((int)p.pos.x, (int)p.pos.y, (int)(p.size * k * 2), (int)(p.size * k), WithAlpha(p.col, k));
        } else {
            DrawCircle((int)p.pos.x, (int)p.pos.y, p.size * (0.4f + k * 0.9f), WithAlpha(p.col, k));
            if (k > 0.55f)
                DrawCircle((int)p.pos.x, (int)p.pos.y, p.size * 0.5f * k, WithAlpha(Color{255, 245, 220, 255}, k));
        }
    }
}

void World::drawPickups() {
    for (auto& p : pickups_) {
        if (!p.alive) continue;
        Color c = p.kind == 0 ? pal::kGold : (p.kind == 1 ? pal::kCyan : Color{120, 240, 150, 255});
        float s = 26.0f + std::sin(p.t * 6.0f) * 3.0f;
        DrawCircle((int)p.pos.x, (int)p.pos.y, s * 1.5f, WithAlpha(c, 0.16f));
        ui::Panel({p.pos.x - s, p.pos.y - s, s * 2, s * 2}, WithAlpha(pal::kPanelBg, 0.9f), c, 8, 2.5f);
        const char* label = p.kind == 0 ? "P" : (p.kind == 1 ? "B" : "H");
        ui::TextC(label, p.pos.x, p.pos.y - 15, 30, c, true);
    }
}

// 敌机血条：贴在机体上方，白条滞后表现这一击掉了多少
void World::drawEnemyBars() {
    for (auto& e : enemies_) {
        if (!e.alive || e.delay > 0) continue;
        if (e.pos.x > VW + 120 || e.pos.x < -120) continue;
        float span = EnemySpan(e);
        float w = clampf(span * 0.52f, 74.0f, 156.0f);
        float h = 9.0f;
        float x = e.pos.x - w * 0.5f;
        float y = e.pos.y - span * 0.34f - 30.0f;
        if (y < 24.0f) y = e.pos.y + span * 0.30f + 18.0f;  // 贴顶时改画在机体下方
        float r = clampf(e.hp / e.maxHp, 0, 1);
        Color c = EnemyBarColor(e.cls);

        DrawRectangle((int)(x - 3), (int)(y - 3), (int)(w + 6), (int)(h + 6), WithAlpha(BLACK, 0.5f));
        DrawRectangle((int)x, (int)y, (int)w, (int)h, WithAlpha(Color{34, 44, 58, 255}, 0.92f));
        if (e.hpTrail > r)
            DrawRectangle((int)x, (int)y, (int)(w * e.hpTrail), (int)h, WithAlpha(Color{255, 250, 240, 255}, 0.4f));
        DrawRectangle((int)x, (int)y, (int)(w * r), (int)h, c);
        DrawRectangle((int)x, (int)y, (int)(w * r), 3, WithAlpha(WHITE, 0.55f));
        DrawRectangleLinesEx({x - 1.5f, y - 1.5f, w + 3, h + 3}, 1.5f, WithAlpha(c, 0.7f));
    }
}

void World::drawFore() {
    // 对战：中线虚线，提示两人各守半场
    if (versus_) {
        for (float y = 280; y < VH - 100; y += 44) {
            DrawRectangle((int)(VW * 0.5f) - 1, (int)y, 2, 24, WithAlpha(WHITE, 0.07f));
        }
    }

    for (int s = 0; s < playerCount_; s++) {
        Fighter& f = p_[s];
        if (f.down) continue;

        // 命中点标记
        float a = (f.invuln > 0) ? 0.25f : 0.55f;
        DrawCircle((int)f.pos.x, (int)f.pos.y, 13.0f, WithAlpha(pal::kCyan, a * 0.35f));
        DrawCircle((int)f.pos.x, (int)f.pos.y, 4.0f, WithAlpha(Color{240, 255, 255, 255}, a + 0.3f));

        // 闪身拖影（2D 速度线）
        if (f.dashT > 0) {
            for (int i = 0; i < 5; i++) {
                float oy = (i - 2) * 34.0f;
                Vector2 a0{f.pos.x - f.dashDir.x * 150.0f, f.pos.y - f.dashDir.y * 150.0f + oy};
                Vector2 b0{f.pos.x - f.dashDir.x * 30.0f, f.pos.y - f.dashDir.y * 30.0f + oy};
                ThickLine(a0, b0, 4.0f, WithAlpha(pal::kCyan, 0.3f));
            }
        }

        // P1 / P2 头顶标识：两人同屏时最容易看混
        if (playerCount_ > 1) {
            Color tc = (s == 1) ? pal::kGold : pal::kCyan;
            ui::TextC(s == 0 ? "1P" : "2P", f.pos.x, f.pos.y - 104, 22, WithAlpha(tc, 0.85f), true);
        }
    }

    drawParticles();
    drawTelegraphs();
    drawBullets();
    drawPickups();
    drawEnemyBars();

    for (int s = 0; s < playerCount_; s++) {
        Fighter& f = p_[s];
        if (f.bombFlash > 0) {
            DrawCircle((int)f.pos.x, (int)f.pos.y, 900.0f * (1.0f - f.bombFlash / 0.9f),
                       WithAlpha(Color{180, 240, 255, 255}, f.bombFlash * 0.5f));
        }
    }
}

// 单人 / 组队共用的玩家面板：装甲、爆弹、闪避冷却
void World::drawPlayerHudPanel(int slot, Rectangle r, bool compact) {
    const Fighter& f = p_[slot];
    Color accent = (slot == 1) ? pal::kGold : pal::kCyan;
    ui::Panel(r, WithAlpha(pal::kPanelBg, 0.58f), WithAlpha(accent, 0.26f), 12, 1.5f);

    char buf[64];
    ui::TextOutline(slot == 0 ? "1P" : "2P", r.x + 16, r.y + 8, 24, WithAlpha(accent, 0.95f),
                    WithAlpha(BLACK, 0.6f), true);
    ui::TextOutline("装甲", r.x + 66, r.y + 12, 18, WithAlpha(pal::kText, 0.85f), WithAlpha(BLACK, 0.6f));

    float ratio = clampf(f.hp / kPlayerMaxHp, 0, 1);
    Color hpCol = ratio > 0.6f ? pal::kCyan : (ratio > 0.3f ? pal::kGold : pal::kDanger);
    snprintf(buf, sizeof buf, "%d", (int)std::ceil(f.hp));
    ui::TextR(buf, r.x + r.width - 16, r.y + 4, 26, WithAlpha(hpCol, 0.95f), true);

    float bx = r.x + 16, bw = r.width - 32;
    DrawRectangle((int)bx - 3, (int)r.y + 37, (int)bw + 6, 22, WithAlpha(BLACK, 0.55f));
    SegBar(bx, r.y + 40, bw, 16, ratio, 16, hpCol, WithAlpha(Color{40, 52, 66, 255}, 0.85f));
    DrawRectangle((int)bx, (int)r.y + 40, (int)(bw * ratio), 3, WithAlpha(WHITE, 0.35f));

    // 爆弹
    ui::TextOutline("爆弹", r.x + 16, r.y + 72, 18, WithAlpha(pal::kText, 0.8f), WithAlpha(BLACK, 0.6f));
    for (int i = 0; i < f.bombs; i++) {
        float cx = r.x + r.width - 20 - i * 28, cy = r.y + 84;
        DrawCircle((int)cx, (int)(cy + 4), 9, pal::kGold);
        DrawRectangle((int)cx - 3, (int)cy - 14, 6, 9, WithAlpha(pal::kGold, 0.9f));
        DrawCircle((int)cx, (int)cy - 14, 3, WithAlpha(Color{255, 240, 200, 255}, 0.9f));
    }

    // 闪避
    float dr = clampf(1.0f - f.dashCd / kDashCd, 0, 1);
    ui::TextOutline("闪避", r.x + 16, r.y + 104, 18, WithAlpha(pal::kText, 0.8f), WithAlpha(BLACK, 0.6f));
    if (dr >= 0.999f) {
        float p = ui::Pulse(2.4f);
        ui::TextR("就绪", r.x + r.width - 16, r.y + 104, 17, WithAlpha(pal::kCyan, 0.55f + 0.45f * p), true);
    }
    DrawRectangle((int)bx, (int)r.y + 132, (int)bw, 7, WithAlpha(Color{34, 44, 58, 255}, 0.9f));
    DrawRectangle((int)bx, (int)r.y + 132, (int)(bw * dr), 7,
                  WithAlpha(dr >= 0.999f ? pal::kCyan : pal::kDim, 0.95f));

    if (f.down) {
        DrawRectangle((int)r.x + 3, (int)r.y + 3, (int)r.width - 6, (int)r.height - 6,
                      WithAlpha(Color{8, 12, 18, 255}, 0.72f));
        ui::TextC("损毁", r.x + r.width * 0.5f, r.y + 38, 30, WithAlpha(pal::kDanger, 0.9f), true);
        if (!versus_) {
            snprintf(buf, sizeof buf, "重生  %.1f", std::max(0.0f, f.respawnT));
            ui::TextC(buf, r.x + r.width * 0.5f, r.y + 86, 22, WithAlpha(pal::kDim, 0.9f));
        }
    }
}

// 对战 HUD：左右各一块装甲，中间只留 VS
void World::drawDuelHud() {
    drawPlayerHudPanel(0, {36, 14, 400, 158}, true);
    drawPlayerHudPanel(1, {VW - 436, 14, 400, 158}, true);
    float p = ui::Pulse(1.4f);
    ui::TextC("VS", VW * 0.5f, 22, 52, WithAlpha(pal::kGold, 0.6f + 0.3f * p), true);
    const char* hint = "击破对方战机";
    float hw = ui::TextW(hint, 22, true) + 56;
    ui::Panel({VW * 0.5f - hw * 0.5f, VH - 92, hw, 48}, WithAlpha(pal::kPanelBg, 0.65f),
              WithAlpha(pal::kGold, 0.35f), 8, 1.5f);
    ui::TextC(hint, VW * 0.5f, VH - 80, 22, WithAlpha(pal::kDim, 0.9f));
}

void World::drawHud() {
    DrawRectangleGradientV(0, 0, (int)VW, 250, WithAlpha(BLACK, 0.55f), BLANK);
    DrawRectangleGradientV(0, (int)(VH - 170), (int)VW, 170, BLANK, WithAlpha(BLACK, 0.38f));
    // 右上 / 左下 HUD 的底色块：云层太亮，纯文字读不清
    DrawRectangleGradientH((int)(VW - 520), 0, 520, 350, BLANK, WithAlpha(BLACK, 0.42f));
    DrawRectangleGradientH(0, (int)(VH - 150), 300, 150, WithAlpha(BLACK, 0.42f), BLANK);

    char buf[64];

    // ---- 分数（对战不看分数，避免和左上角的 1P 面板打架）
    if (!versus_) {
        ui::Text("SCORE", 48, 30, 22, WithAlpha(pal::kCyan, 0.95f), true);
        snprintf(buf, sizeof buf, "%07d", score_);
        ui::TextOutline(buf, 46, 58, 52, pal::kText, WithAlpha(BLACK, 0.55f), true);
        snprintf(buf, sizeof buf, "HI %07d", std::max(save::data().hiScore, score_));
        ui::TextOutline(buf, 48, 122, 22, WithAlpha(pal::kDim, 0.95f), WithAlpha(BLACK, 0.5f));
        if (combo_ > 1) {
            float pop = comboT_ > 2.3f ? 1.0f + (comboT_ - 2.3f) * 0.9f : 1.0f;
            snprintf(buf, sizeof buf, "COMBO x%d", combo_);
            ui::TextOutline(buf, 48, 160, (int)(26 * pop), pal::kGold, WithAlpha(BLACK, 0.5f), true);
        }
    }

    // ---- 玩家面板
    if (versus_) {
        drawDuelHud();
    } else {
        float pw = 372, px0 = VW - 36 - pw;
        drawPlayerHudPanel(0, {px0, 10, pw, 158}, true);
        if (playerCount_ > 1) drawPlayerHudPanel(1, {px0, 176, pw, 158}, true);
    }

    if (!versus_) {
        // ---- 剩余敌机（左下）
        int left = enemiesLeft();
        ui::TextOutline("敌机", 48, VH - 100, 20, WithAlpha(pal::kText, 0.8f), WithAlpha(BLACK, 0.65f));
        snprintf(buf, sizeof buf, "%02d", left);
        ui::TextOutline(buf, 46, VH - 76, 44, left > 0 ? pal::kText : WithAlpha(pal::kCyan, 0.9f),
                        WithAlpha(BLACK, 0.6f), true);

        // ---- 波次进度（右下，5 格）
        float bw = 44, gap = 8;
        float totalW = kWaveCount * (bw + gap) - gap;
        float px = VW - 48 - totalW, py = VH - 76;
        ui::TextOutline("进度", px - 22 - ui::TextW("进度", 20), py + 4, 20, WithAlpha(pal::kText, 0.75f),
                        WithAlpha(BLACK, 0.6f));
        for (int i = 0; i < kWaveCount; i++) {
            bool done = (i < wave_) || (i == wave_ && wavePhase_ == WP_Gap);
            bool cur = (i == wave_) && !done;
            Color c = done ? pal::kCyan : (cur ? WithAlpha(pal::kCyan, 0.45f) : WithAlpha(WHITE, 0.14f));
            DrawRectangle((int)(px + i * (bw + gap)), (int)py, (int)bw, 26, c);
            if (done) DrawRectangle((int)(px + i * (bw + gap)), (int)py, (int)bw, 6, WithAlpha(WHITE, 0.55f));
            if (cur) DrawRectangleLinesEx({px + i * (bw + gap), py, bw, 26}, 2, WithAlpha(pal::kCyan, 0.85f));
        }
    }

    // ---- 波次横幅
    if (bannerT_ > 0 && !banner_.empty()) {
        float t = 1.0f - bannerT_ / std::max(bannerDur_, 0.01f);
        float a = clampf(bannerT_ / 0.4f, 0, 1) * clampf(t * 6.0f, 0, 1);
        float y = 300 + (1.0f - clampf(t * 4.0f, 0, 1)) * 34;
        float w = ui::TextW(banner_, 64, true) + 160;
        ui::Panel({VW * 0.5f - w * 0.5f, y - 22, w, 104}, WithAlpha(pal::kPanelBg, 0.72f * a),
                  WithAlpha(pal::kCyan, 0.75f * a), 10, 2);
        ui::TextC(banner_, VW * 0.5f, y, 64, WithAlpha(pal::kText, a), true);
        DrawRectangle((int)(VW * 0.5f - w * 0.5f + 30), (int)(y + 74), (int)(w - 60), 2,
                      WithAlpha(pal::kCyan, 0.4f * a));
    }

    // ---- Boss 血条
    if (boss_.alive) {
        float w = 1040, x = VW * 0.5f - w * 0.5f, y = 40;
        float r = bossHp01();
        DrawRectangle((int)x - 4, (int)y - 4, (int)w + 8, 34, WithAlpha(BLACK, 0.6f));
        DrawRectangle((int)x, (int)y, (int)w, 26, WithAlpha(Color{40, 20, 24, 255}, 0.85f));
        if (boss_.hpTrail > r)
            DrawRectangle((int)x, (int)y, (int)(w * boss_.hpTrail), 26, WithAlpha(Color{255, 244, 232, 255}, 0.42f));
        DrawRectangle((int)x, (int)y, (int)(w * r), 26, Color{255, 86, 70, 255});
        DrawRectangle((int)x, (int)y, (int)(w * r), 9, Color{255, 168, 140, 255});
        DrawRectangleLinesEx({x, y, w, 26}, 2, WithAlpha(pal::kDanger, 0.9f));
        ui::TextC("IRON FORTRESS", VW * 0.5f, y + 40, 26, WithAlpha(pal::kText, 0.9f), true);
    }

    // ---- 危险提示：有敌机在蓄力冲锋时，屏幕边缘泛红
    float danger = 0;
    for (auto& e : enemies_)
        if (e.alive && e.st == ES_Charge) danger = std::max(danger, clampf(e.stateT / kChargeTime, 0, 1));
    if (boss_.alive && boss_.st == ES_Charge) danger = 1.0f;
    if (danger > 0) {
        float p = 0.5f + 0.5f * std::sin((float)GetTime() * 16.0f);
        for (int i = 0; i < 3; i++) {
            float t = 0.45f - i * 0.13f;
            DrawRectangleLinesEx({0, 0, VW, VH}, 30 - i * 9, WithAlpha(pal::kDanger, t * danger * (0.45f + 0.4f * p)));
        }
        // 操作提示：教玩家用闪避脱离锁定线（双人时两套按键都要写出来）
        const char* hint = (playerCount_ > 1) ? "1P  L        2P  /        闪避" : "SHIFT + 方向 / L    闪避";
        float hw = ui::TextW(hint, 30, true) + 60;
        ui::Panel({VW * 0.5f - hw * 0.5f, VH - 168, hw, 62}, WithAlpha(pal::kPanelBg, 0.72f * danger),
                  WithAlpha(pal::kDanger, 0.6f * danger), 10, 2);
        ui::TextC(hint, VW * 0.5f, VH - 152, 30, WithAlpha(pal::kText, 0.55f + 0.45f * p), true);
    }

    if (flash_ > 0 && !app().optReduceFlash)
        DrawRectangle(0, 0, (int)VW, (int)VH, WithAlpha(Color{255, 240, 230, 255}, flash_ * 0.35f));
}

}  // namespace sky
