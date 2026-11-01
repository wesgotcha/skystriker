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

struct EnemyDef {
    int   craft;
    float hp;
    float span;
    int   score;
};

// 0 Imperial 1 Insurgent 2 Pancake 3 Spitfire
const EnemyDef kEnemy[4] = {
    {0, 36.0f, 300.0f, 300},
    {1, 15.0f, 240.0f, 150},
    {2, 30.0f, 330.0f, 260},
    {3, 13.0f, 250.0f, 180},
};

constexpr float kPlayerSpan = 330.0f;
constexpr float kPlayerSpeed = 820.0f;
constexpr float kPlayerR = 11.0f;

Color BulletColor(int kind) {
    switch (kind) {
        case 0: return Color{255, 74, 56, 255};
        case 1: return Color{255, 146, 26, 255};
        case 2: return Color{214, 84, 255, 255};
        default: return Color{255, 100, 80, 255};
    }
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
    waveT_ = 0;
    waveSpawning_ = false;
    spawnStep_ = 0;
    spawnTimer_ = 0;
    bannerT_ = 0;
    stormMix_ = 0;
    state_ = WorldState::Playing;
    stateT_ = 0;
    finished_ = false;  // 重开必须复位，否则新一局一进来就被判定为已结束
    bossEnter_ = 0;
    bombFlash_ = 0;
    playerDeathT_ = 0;
    invuln_ = 1.5f;
    player_ = {500, 720};
    playerVel_ = {0, 0};
    playerPose_ = pose::FromRoll(pose::kPlayer.base);
    playerLastY_ = player_.y;
    playerVyN_ = 0;
    weapon_ = 1;
    lives_ = 3;
    bombs_ = 2;
    rng_.seed(0x5EED);
    bg_.setup(Backdrop::Ocean);
    bgStorm_.setup(Backdrop::Storm);
}

void World::start(bool continueRun) {
    reset();
    if (continueRun) {
        weapon_ = 2;
        score_ = 0;
    }
    wave_ = 0;
    waveT_ = 1.2f;
    banner_ = "WAVE 1";
    bannerT_ = 1.6f;
}

// ---------------------------------------------------------------- 工具
Vector2 World::shakeOffset() const {
    if (shake_ <= 0.01f) return {0, 0};
    float a = shake_ * 18.0f;
    return {(float)GetRandomValue(-100, 100) / 100.0f * a, (float)GetRandomValue(-100, 100) / 100.0f * a};
}

float World::bossHp01() const {
    if (!boss_.alive || boss_.maxHp <= 0) return 0;
    return clampf(boss_.hp / boss_.maxHp, 0, 1);
}

int World::stars() const {
    if (state_ != WorldState::Cleared) return 0;
    int s = 1;
    if (kills_ >= 34) s = 2;
    if (kills_ >= 40 && lives_ >= 2) s = 3;
    return s;
}

void World::addBullet(Vector2 pos, Vector2 vel, float r, Color col, int kind, float dmg) {
    if ((int)bullets_.size() > 1200) return;
    Bullet b;
    b.pos = pos;
    b.vel = vel;
    b.r = r;
    b.col = col;
    b.kind = kind;
    b.dmg = dmg;
    b.life = 8.0f;
    b.alive = true;
    b.spin = 0;
    bullets_.push_back(b);
}

void World::addBurst(Vector2 pos, float count, float speed, float spread, Color col, float r, float baseAngle) {
    for (int i = 0; i < (int)count; i++) {
        float a = baseAngle + (count > 1 ? (-spread * 0.5f + spread * i / (count - 1)) : 0.0f);
        addBullet(pos, {std::cos(a) * speed, std::sin(a) * speed}, r, col, 0);
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

void World::addScore(int base) {
    combo_++;
    comboT_ = 2.6f;
    maxCombo_ = std::max(maxCombo_, combo_);
    float mult = 1.0f + std::min(combo_, 40) * 0.05f;
    score_ += (int)(base * mult);
}

void World::dropPickup(Vector2 pos) {
    float r = rng_.f();
    int kind = 0;
    if (r < 0.14f) kind = 1;
    else if (r < 0.24f) kind = 2;
    else if (r > 0.62f) return;  // 其余不掉落
    Pickup p;
    p.pos = pos;
    p.vel = {-90.0f, rng_.range(-40, 40)};
    p.kind = kind;
    p.t = 0;
    p.alive = true;
    pickups_.push_back(p);
}

// ---------------------------------------------------------------- 生成
void World::spawnEnemy(int type, float x, float y, int pattern, float scale) {
    Enemy e;
    e.type = type;
    e.pos = {x, y};
    e.baseY = y;
    e.scale = scale;
    e.maxHp = e.hp = kEnemy[type].hp * (pattern == 3 ? 1.15f : 1.0f);
    e.pattern = pattern;
    e.score = kEnemy[type].score;
    e.alive = true;
    e.phase = rng_.range(0, 6.28f);
    e.fireT = rng_.range(0.7f, 1.8f);
    e.stopX = rng_.range(1180, 1460);
    pose::Reset(e.pose, pose::kEnemy, type == 2 ? -0.05f : -0.08f);
    e.lastY = y;
    if (pattern == 2) {
        float a = rng_.range(2.55f, 3.35f);
        e.vel = {std::cos(a) * 420.0f, std::sin(a) * 420.0f};
    }
    enemies_.push_back(e);
}

void World::spawnWave(int idx) {
    // 12 波，逐步加压
    int stage = idx % 12;
    if (stage == 0) {
        for (int i = 0; i < 3; i++) spawnEnemy(1, 2100 + i * 220, 380 + i * 140, 1);
    } else if (stage == 1) {
        for (int i = 0; i < 4; i++) spawnEnemy(3, 2050 + i * 180, 300 + (i % 2) * 320, 0);
    } else if (stage == 2) {
        for (int i = 0; i < 2; i++) spawnEnemy(0, 2150 + i * 300, 420 + i * 260, 3);
        for (int i = 0; i < 2; i++) spawnEnemy(1, 2000 + i * 200, 760, 1);
    } else if (stage == 3) {
        for (int i = 0; i < 5; i++) spawnEnemy(3, 2000 + i * 150, 260 + i * 120, 2);
    } else if (stage == 4) {
        for (int i = 0; i < 2; i++) spawnEnemy(2, 2200 + i * 340, 460 + i * 300, 3);
    } else if (stage == 5) {
        for (int i = 0; i < 3; i++) spawnEnemy(1, 2050 + i * 200, 340 + i * 200, 0);
        for (int i = 0; i < 3; i++) spawnEnemy(1, 2350 + i * 200, 420 + i * 180, 1);
    } else if (stage == 6) {
        for (int i = 0; i < 4; i++) spawnEnemy(0, 2100 + i * 260, 380 + (i % 2) * 300, 3);
    } else if (stage == 7) {
        for (int i = 0; i < 6; i++) spawnEnemy(3, 2000 + i * 130, 300 + i * 90, 2);
    } else if (stage == 8) {
        for (int i = 0; i < 3; i++) spawnEnemy(2, 2150 + i * 300, 420 + i * 180, 3);
        for (int i = 0; i < 2; i++) spawnEnemy(1, 2050 + i * 220, 820, 1);
    } else if (stage == 9) {
        for (int i = 0; i < 5; i++) spawnEnemy(1, 2000 + i * 160, 320 + (i % 3) * 220, i % 2);
    } else if (stage == 10) {
        for (int i = 0; i < 3; i++) spawnEnemy(0, 2100 + i * 300, 400 + i * 220, 3);
        for (int i = 0; i < 3; i++) spawnEnemy(2, 2400 + i * 300, 520 + i * 160, 0);
    } else {
        for (int i = 0; i < 2; i++) spawnEnemy(2, 2150 + i * 320, 430 + i * 260, 3);
        for (int i = 0; i < 4; i++) spawnEnemy(3, 2000 + i * 150, 300 + i * 160, 2);
        for (int i = 0; i < 2; i++) spawnEnemy(0, 2500 + i * 300, 500 + i * 300, 3);
    }
    banner_ = "WAVE " + std::to_string(idx + 1);
    bannerT_ = 1.5f;
}

// ---------------------------------------------------------------- 玩家
void World::playerHit() {
    if (invuln_ > 0 || playerDeathT_ > 0) return;
    if (app().godMode) return;
    lives_--;
    audio::play(audio::SfxDamage, 0.9f, 1.0f);
    addExplosion(player_, 150, Color{255, 140, 90, 255}, 26);
    shake_ = std::max(shake_, 0.9f);
    flash_ = 0.55f;
    combo_ = 0;
    weapon_ = std::max(1, weapon_ - 1);
    if (lives_ <= 0) {
        playerDeathT_ = 1.4f;
        state_ = WorldState::Failed;
        stateT_ = 0;
        addExplosion(player_, 220, Color{255, 120, 80, 255}, 40);
        audio::play(audio::SfxBigExplode, 1.0f, 0.9f);
    } else {
        invuln_ = 2.4f;
        player_ = {480, 760};
        playerVel_ = {0, 0};
    }
}

std::string World::debugInfo() const {
    char b[256];
    int pb = 0, eb = 0;
    for (auto& x : bullets_) {
        if (!x.alive) continue;
        if (x.vel.x > 0) pb++; else eb++;
    }
    snprintf(b, sizeof b, "state=%d fin=%d player=(%.0f,%.0f) lives=%d inv=%.1f bullets=%d(p%d/e%d) enemies=%d alive=%d score=%d",
             (int)state_, (int)finished_, player_.x, player_.y, lives_, invuln_, (int)bullets_.size(), pb, eb,
             (int)enemies_.size(), (int)std::count_if(enemies_.begin(), enemies_.end(), [](const Enemy& e){ return e.alive; }), score_);
    return b;
}

void World::killPlayerNow() {
    lives_ = 1;
    invuln_ = 0;
    playerHit();
}

void World::updatePlayer(float dt, bool autopilot) {
    if (playerDeathT_ > 0) {
        playerDeathT_ -= dt;
        return;
    }
    if (invuln_ > 0) invuln_ -= dt;

    Vector2 dir{0, 0};
    if (autopilot) {
        // 自动演示：躲弹 + 居中
        autopilotT_ -= dt;
        Vector2 repulse{0, 0};
        for (const auto& b : bullets_) {
            if (!b.alive) continue;
            float dx = player_.x - b.pos.x;
            if (dx < -40 || dx > 320) continue;
            float dy = player_.y - b.pos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > 210 * 210) continue;
            float d = std::sqrt(d2) + 0.01f;
            repulse.x += dx / d * (210 - d) * 0.6f;
            repulse.y += dy / d * (210 - d);
        }
        float targetY = clampf(player_.y + repulse.y * 0.06f, 320, 980);
        targetY = lerpf(targetY, 700, 0.35f);
        float targetX = clampf(460 + repulse.x * 0.02f, 240, 900);
        autoTarget_ = {targetX, targetY};
        Vector2 d{autoTarget_.x - player_.x, autoTarget_.y - player_.y};
        float l = std::sqrt(d.x * d.x + d.y * d.y);
        if (l > 2) { d.x /= l; d.y /= l; } else { d = {0, 0}; }
        dir = d;
    } else {
        const Input& in = app().in;
        if (in.down(KEY_LEFT) || in.down(KEY_A)) dir.x -= 1;
        if (in.down(KEY_RIGHT) || in.down(KEY_D)) dir.x += 1;
        if (in.down(KEY_UP) || in.down(KEY_W)) dir.y -= 1;
        if (in.down(KEY_DOWN) || in.down(KEY_S)) dir.y += 1;
        if (dir.x != 0 && dir.y != 0) {
            dir.x *= 0.7071f;
            dir.y *= 0.7071f;
        }
        // 鼠标操控：按住右键或移动时跟随
        if (in.lmb && in.mouseMoved) {
            float k = 1.0f - std::exp(-dt * 18.0f);
            player_.x = lerpf(player_.x, in.mouse.x, k);
            player_.y = lerpf(player_.y, in.mouse.y, k);
        }
    }

    player_.x += dir.x * kPlayerSpeed * dt;
    player_.y += dir.y * kPlayerSpeed * dt;
    player_.x = clampf(player_.x, 110, VW - 520);
    player_.y = clampf(player_.y, 300, VH - 70);

    playerVel_ = {dir.x * kPlayerSpeed, dir.y * kPlayerSpeed};

    // 姿态 = 运动的结果：用真实位移反推垂直速度（键鼠一致），再指数趋近目标倾角
    if (dt > 0.0001f) {
        float vyPx = (player_.y - playerLastY_) / dt;   // 屏幕坐标，向下为正
        float vyN = clampf(-vyPx / kPlayerSpeed, -1.0f, 1.0f);  // 归一化，向上为正
        playerVyN_ = lerpf(playerVyN_, vyN, 1.0f - std::exp(-dt * 18.0f));
    }
    playerLastY_ = player_.y;
    pose::Update(playerPose_, pose::kPlayer, playerVyN_, dt);

    // 射击
    const Input& in = app().in;
    bool firing = autopilot || in.down(save::data().keyFire) || in.down(KEY_SPACE) || in.lmb;
    fireCd_ -= dt;
    if (firing && fireCd_ <= 0) {
        fireCd_ = 0.115f;
        int lvl = clampi(weapon_, 1, 4);
        float bx = player_.x + 96;
        audio::play(audio::SfxShot, 0.28f, 0.94f + rng_.range(0, 0.12f));
        auto shot = [&](float oy, float vy) {
            addBullet({bx, player_.y + oy}, {1980.0f, vy}, 7.0f, Color{140, 245, 255, 255}, 2, 2.0f);
        };
        if (lvl == 1) shot(0, 0);
        else if (lvl == 2) { shot(-16, 0); shot(16, 0); }
        else if (lvl == 3) { shot(0, 0); shot(-30, -90); shot(30, 90); }
        else { shot(-14, 0); shot(14, 0); shot(-42, -160); shot(42, 160); }
    }

    // 爆弹
    if (!autopilot && (in.pressed(save::data().keyBomb) || in.pressed(KEY_L))) {
        if (bombs_ > 0) {
            bombs_--;
            bombFlash_ = 0.9f;
            shake_ = std::max(shake_, 1.0f);
            audio::play(audio::SfxBomb, 1.0f, 1.0f);
            for (auto& b : bullets_) {
                if (!b.alive) continue;
                addHitSpark(b.pos, Color{200, 240, 255, 255});
                b.alive = false;
            }
            for (auto& e : enemies_) {
                if (!e.alive) continue;
                e.hp -= 26;
                e.hitFlash = 0.2f;
                if (e.hp <= 0) {
                    e.alive = false;
                    addExplosion(e.pos, 120, Color{255, 170, 90, 255}, 20);
                    addScore(e.score);
                    kills_++;
                    dropPickup(e.pos);
                }
            }
            if (boss_.alive) {
                boss_.hp -= 90;
                boss_.hitFlash = 0.25f;
            }
        }
    }
    if (bombFlash_ > 0) bombFlash_ -= dt;
}

// ---------------------------------------------------------------- 子弹
void World::updateBullets(float dt) {
    for (auto& b : bullets_) {
        if (!b.alive) continue;
        b.pos.x += b.vel.x * dt;
        b.pos.y += b.vel.y * dt;
        b.life -= dt;
        if (b.life <= 0 || b.pos.x < -120 || b.pos.x > VW + 260 || b.pos.y < -160 || b.pos.y > VH + 160)
            b.alive = false;
    }
    // 玩家子弹 vs 敌机
    for (auto& b : bullets_) {
        if (!b.alive || b.dmg <= 0) continue;
        bool playerBullet = (b.vel.x > 0);
        if (!playerBullet) continue;
        for (size_t i = 0; i < enemies_.size(); i++) {
            Enemy& e = enemies_[i];
            if (!e.alive) continue;
            float rr = kEnemy[e.type].span * e.scale * 0.34f;
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
                if (boss_.hp <= 0 && boss_.alive) {
                    boss_.alive = false;
                    state_ = WorldState::Cleared;
                    stateT_ = 0;
                    score_ += 12000;
                    kills_ += 6;
                    shake_ = 1.4f;
                    flash_ = 0.9f;
                    audio::play(audio::SfxBigExplode, 1.0f, 0.8f);
                    for (int k = 0; k < 6; k++) {
                        Vector2 p = {boss_.pos.x + rng_.range(-180, 180), boss_.pos.y + rng_.range(-160, 160)};
                        addExplosion(p, 220, Color{255, 150, 90, 255}, 28);
                    }
                }
            }
        }
    }
    // 敌弹 vs 玩家
    if (playerDeathT_ <= 0 && invuln_ <= 0) {
        for (auto& b : bullets_) {
            if (!b.alive || b.vel.x >= 0) continue;
            float dx = b.pos.x - player_.x, dy = b.pos.y - player_.y;
            float rr = kPlayerR + b.r * 0.72f;
            if (dx * dx + dy * dy < rr * rr) {
                b.alive = false;
                playerHit();
                break;
            }
        }
    }
    // 清理
    if (bullets_.size() > 64) {
        bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet& b) { return !b.alive; }),
                       bullets_.end());
    }
}

void World::damageEnemy(int idx, float dmg) {
    Enemy& e = enemies_[idx];
    e.hp -= dmg;
    e.hitFlash = 0.08f;
    if (e.hp <= 0 && e.alive) {
        e.alive = false;
        kills_++;
        addScore(e.score);
        float sz = kEnemy[e.type].span * e.scale;
        addExplosion(e.pos, sz * 0.8f, Color{255, 168, 80, 255}, 18);
        audio::play(audio::SfxExplode, 0.55f, 0.95f + rng_.range(0, 0.2f));
        shake_ = std::max(shake_, 0.28f);
        dropPickup(e.pos);
    }
}

// ---------------------------------------------------------------- 敌机
void World::enemyShoot(const Enemy& e) {
    Vector2 aim{player_.x - e.pos.x, player_.y - e.pos.y};
    float base = std::atan2(aim.y, aim.x);
    switch (e.type) {
        case 0:
            addBurst(e.pos, 5, 320, 0.66f, BulletColor(1), 11, base);
            break;
        case 1:
            addBullet(e.pos, {std::cos(base) * 360, std::sin(base) * 360}, 9, BulletColor(0), 0);
            break;
        case 2:
            addBurst(e.pos, 10, 260, 6.283f, BulletColor(2), 12, rng_.range(0, 6.28f));
            break;
        default:
            for (int i = 0; i < 3; i++) {
                float a = base + (i - 1) * 0.14f;
                addBullet(e.pos, {std::cos(a) * 420, std::sin(a) * 420}, 8, BulletColor(1), 1);
            }
            break;
    }
    audio::play(audio::SfxHit, 0.12f, 0.7f);
}

void World::updateEnemies(float dt) {
    for (auto& e : enemies_) {
        if (!e.alive) continue;
        e.t += dt;
        if (e.hitFlash > 0) e.hitFlash -= dt;
        e.fireT -= dt;

        switch (e.pattern) {
            case 0:  // 直线
                e.pos.x -= 360.0f * dt;
                break;
            case 1:  // 蛇形
                e.pos.x -= 300.0f * dt;
                e.pos.y = e.baseY + std::sin(e.t * 2.2f + e.phase) * 120.0f;
                break;
            case 2:  // 俯冲
                e.pos.x += e.vel.x * dt;
                e.pos.y += e.vel.y * dt;
                break;
            default:  // 定点
                if (e.pos.x > e.stopX) {
                    e.pos.x -= 420.0f * dt;
                } else {
                    e.pos.y = e.baseY + std::sin(e.t * 1.3f + e.phase) * 46.0f;
                    if (e.t > 7.0f) e.pos.x -= 420.0f * dt;
                }
                break;
        }

        // 姿态：与玩家共用同一套公式，倾角跟着轨迹起伏
        if (dt > 0.0001f) {
            float vy = (e.pos.y - e.lastY) / dt;  // 屏幕坐标，向下为正
            float vyN = clampf(-vy / 300.0f, -1.0f, 1.0f);
            pose::Update(e.pose, pose::kEnemy, vyN, dt);
        }
        e.lastY = e.pos.y;

        if (e.fireT <= 0 && e.pos.x < VW + 40 && e.pos.x > 60) {
            enemyShoot(e);
            e.fireT = (e.type == 2) ? 3.0f : (e.type == 0 ? 2.3f : 1.7f);
        }
        if (e.pos.x < -320 || e.pos.y > VH + 320 || e.pos.y < -320) e.alive = false;
    }
    if (enemies_.size() > 40)
        enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) { return !e.alive; }),
                       enemies_.end());
}

// ---------------------------------------------------------------- Boss
void World::bossShoot(int pattern) {
    float hp01 = bossHp01();
    switch (pattern) {
        case 0: {
            Vector2 aim{player_.x - boss_.pos.x, player_.y - boss_.pos.y};
            float base = std::atan2(aim.y, aim.x);
            addBurst({boss_.pos.x - 60, boss_.pos.y + 40}, 7, 380, 0.9f, BulletColor(0), 12, base);
            break;
        }
        case 1: {
            int n = hp01 < 0.4f ? 18 : 14;
            float off = boss_.t * 0.9f;
            for (int i = 0; i < n; i++) {
                float a = off + i * 6.2831f / n;
                addBullet({boss_.pos.x - 40, boss_.pos.y}, {std::cos(a) * 300, std::sin(a) * 300}, 11,
                          BulletColor(2), 0);
            }
            break;
        }
        default: {
            for (int k = 0; k < 3; k++) {
                float a = PI + (k - 1) * 0.42f;
                addBullet({boss_.pos.x - 80, boss_.pos.y + 20},
                          {std::cos(a) * 460, std::sin(a) * 460}, 10, BulletColor(1), 1);
            }
            break;
        }
    }
    audio::play(audio::SfxHit, 0.16f, 0.62f);
}

void World::updateBoss(float dt) {
    if (!boss_.alive) return;
    boss_.t += dt;
    if (boss_.hitFlash > 0) boss_.hitFlash -= dt;

    if (bossEnter_ < 1.0f) {
        bossEnter_ = std::min(1.0f, bossEnter_ + dt / 2.2f);
        boss_.pos.x = lerpf(VW + 400, 1430, easeOutCubic(bossEnter_));
        boss_.pos.y = 520 + std::sin(boss_.t * 0.9f) * 24;
        return;
    }
    boss_.pos.y = 520 + std::sin(boss_.t * 0.75f) * 90;
    boss_.pos.x = 1430 + std::sin(boss_.t * 0.5f) * 40;
    if (dt > 0.0001f) {
        float vy = (boss_.pos.y - boss_.lastY) / dt;
        pose::Update(boss_.pose, pose::kEnemy, clampf(-vy / 300.0f, -1.0f, 1.0f), dt);
    }
    boss_.lastY = boss_.pos.y;

    boss_.fireT -= dt;
    if (boss_.fireT <= 0) {
        float hp01 = bossHp01();
        int p = 0;
        if (hp01 > 0.66f) p = (rng_.f() < 0.6f) ? 0 : 2;
        else if (hp01 > 0.33f) p = (rng_.f() < 0.55f) ? 1 : 0;
        else p = (rng_.f() < 0.5f) ? 1 : 2;
        bossShoot(p);
        boss_.fireT = (hp01 > 0.66f ? 1.05f : (hp01 > 0.33f ? 0.82f : 0.62f));
    }
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
    if (particles_.size() > 200)
        particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                        [](const Particle& p) { return !p.alive; }),
                         particles_.end());
}

void World::updatePickups(float dt) {
    for (auto& p : pickups_) {
        if (!p.alive) continue;
        p.t += dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.x *= (1.0f - 1.2f * dt);
        p.vel.y *= (1.0f - 1.2f * dt);
        p.pos.y += std::sin(p.t * 3.0f) * 18.0f * dt;
        if (p.pos.x < -80 || p.t > 9.0f) p.alive = false;
        float dx = p.pos.x - player_.x, dy = p.pos.y - player_.y;
        if (dx * dx + dy * dy < 78 * 78) {
            p.alive = false;
            audio::play(audio::SfxPickup, 0.7f, 1.0f);
            if (p.kind == 0) weapon_ = clampi(weapon_ + 1, 1, 4);
            else if (p.kind == 1) bombs_ = clampi(bombs_ + 1, 0, 5);
            else lives_ = clampi(lives_ + 1, 0, 5);
            score_ += 200;
        }
    }
    if (pickups_.size() > 12)
        pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& p) { return !p.alive; }),
                       pickups_.end());
}

// ---------------------------------------------------------------- 波次
void World::updateSpawner(float dt) {
    if (state_ == WorldState::Playing) {
        waveT_ -= dt;
        if (waveT_ <= 0) {
            if (wave_ < waveCount_) {
                spawnWave(wave_);
                wave_++;
                waveT_ = 6.6f;
            } else {
                bool anyAlive = false;
                for (auto& e : enemies_)
                    if (e.alive) anyAlive = true;
                if (!anyAlive) {
                    state_ = WorldState::StormShift;
                    stateT_ = 0;
                    banner_ = "雷暴到达";
                    bannerT_ = 2.0f;
                    audio::play(audio::SfxWarn, 0.8f, 1.0f);
                }
            }
        }
    }
}

// ---------------------------------------------------------------- 主更新
void World::update(float dt, bool autopilot) {
    elapsed_ += dt;
    stateT_ += dt;
    if (bannerT_ > 0) bannerT_ -= dt;
    if (comboT_ > 0) {
        comboT_ -= dt;
        if (comboT_ <= 0) combo_ = 0;
    }
    if (shake_ > 0) shake_ -= dt * 2.4f;
    if (flash_ > 0) flash_ -= dt * 2.2f;
    if (invuln_ > 0) invuln_ -= dt;

    bg_.setSpeed(state_ == WorldState::Boss ? 300.0f : 230.0f);
    bg_.update(dt);
    bgStorm_.setSpeed(340.0f);
    bgStorm_.update(dt);

    if (state_ == WorldState::Playing || state_ == WorldState::Boss) {
        if (state_ != WorldState::Failed) updatePlayer(dt, autopilot);
    } else if (state_ == WorldState::Failed) {
        updatePlayer(dt, autopilot);
    }

    updateBullets(dt);
    updateEnemies(dt);
    updateBoss(dt);
    updateParticles(dt);
    updatePickups(dt);
    updateSpawner(dt);

    if (state_ == WorldState::StormShift) {
        stormMix_ = clampf(stateT_ / 2.6f, 0, 1);
        if (stateT_ > 3.2f) {
            state_ = WorldState::Warning;
            stateT_ = 0;
            banner_ = "";
            audio::play(audio::SfxWarn, 1.0f, 0.9f);
        }
    } else if (state_ == WorldState::Warning) {
        stormMix_ = 1.0f;
        if (stateT_ > 2.4f) {
            state_ = WorldState::Boss;
            stateT_ = 0;
            boss_ = Enemy{};
            boss_.alive = true;
            boss_.maxHp = boss_.hp = 1400.0f;
            boss_.pos = {VW + 420, 520};
            boss_.t = 0;
            boss_.fireT = 2.0f;
            pose::Reset(boss_.pose, pose::kEnemy);
            boss_.lastY = 520;
            bossEnter_ = 0;
            banner_ = "IRON FORTRESS";
            bannerT_ = 2.4f;
        }
    } else if (state_ == WorldState::Cleared) {
        stormMix_ = 1.0f;
        if (stateT_ > 2.6f && !finished_) finished_ = true;
    } else if (state_ == WorldState::Failed) {
        if (stateT_ > 1.6f && !finished_) finished_ = true;
    }
}

// ---------------------------------------------------------------- 绘制
void World::drawShadow(Vector2 pos, float w, float h, float alpha) {
    // 机体在倾斜正交相机下投影到海面的椭圆阴影（侧视角下贴近机体下方）
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
    bg_.draw();
    bg_.drawProps();
    if (stormMix_ > 0.0f) {
        bgStorm_.setAlpha(stormMix_);
        bgStorm_.draw();
    }
    rlPopMatrix();

    // 地面阴影
    rlPushMatrix();
    rlTranslatef(sh.x, sh.y, 0);
    for (auto& e : enemies_) {
        if (!e.alive) continue;
        drawShadow(e.pos, kEnemy[e.type].span * e.scale * 0.75f, kEnemy[e.type].span * e.scale * 0.3f, 0.9f);
    }
    if (boss_.alive) drawShadow(boss_.pos, 620, 240, 1.0f);
    if (playerDeathT_ <= 0) drawShadow(player_, kPlayerSpan * 0.75f, kPlayerSpan * 0.3f, 1.0f);
    rlPopMatrix();
}

void World::draw3D() {
    Vector2 sh = shakeOffset();
    float px = player_.x + sh.x, py = player_.y + sh.y;

    // 玩家（无敌时闪烁）
    if (playerDeathT_ <= 0) {
        bool blink = (invuln_ > 0) && (((int)(GetTime() * 14)) % 2 == 0);
        Color tint = blink ? WithAlpha(WHITE, 0.45f) : WHITE;
        assets::drawCraft(assets::players[save::data().craft], px, py, save::data().skin, playerPose_,
                          kPlayerSpan, tint, 1);
    }

    for (auto& e : enemies_) {
        if (!e.alive) continue;
        float span = kEnemy[e.type].span * e.scale;
        Color tint = e.hitFlash > 0 ? Color{255, 235, 220, 255} : WHITE;
        assets::drawCraft(assets::enemies[e.type], e.pos.x + sh.x, e.pos.y + sh.y, assets::SKIN_RED, e.pose,
                          span, tint, -1);
    }
    if (boss_.alive) {
        Color tint = boss_.hitFlash > 0 ? Color{255, 230, 210, 255} : WHITE;
        assets::drawCraft(assets::boss, boss_.pos.x + sh.x, boss_.pos.y + sh.y, assets::SKIN_RED, boss_.pose,
                          620.0f, tint, -1);
    }
}

void World::drawBullets() {
    // 第一层：加法混合的柔光，让弹幕在亮色海面上也能看清
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto& b : bullets_) {
        if (!b.alive) continue;
        if (b.vel.x > 0) {
            DrawCircle((int)(b.pos.x + 2), (int)b.pos.y, 16.0f, WithAlpha(Color{80, 200, 255, 255}, 0.28f));
            DrawRectangleGradientH((int)(b.pos.x - 96), (int)(b.pos.y - 9), 104, 18,
                                   WithAlpha(Color{40, 150, 220, 255}, 0.0f),
                                   WithAlpha(Color{120, 230, 255, 255}, 0.32f));
        } else {
            DrawCircle((int)b.pos.x, (int)b.pos.y, b.r * 2.6f, WithAlpha(b.col, 0.3f));
        }
    }
    EndBlendMode();

    // 第二层：实体
    for (auto& b : bullets_) {
        if (!b.alive) continue;
        if (b.vel.x > 0) {
            DrawRectangleGradientH((int)(b.pos.x - 70), (int)(b.pos.y - 5), 78, 10,
                                   WithAlpha(Color{110, 225, 255, 255}, 0.0f), Color{150, 240, 255, 255});
            DrawRectangle((int)(b.pos.x - 18), (int)(b.pos.y - 3), 26, 6, Color{236, 255, 255, 255});
            DrawCircle((int)(b.pos.x + 6), (int)b.pos.y, 6.0f, Color{245, 255, 255, 255});
        } else {
            // 深色描边 + 小亮芯：远看是实心暖色弹，近看有立体感
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

void World::drawFore() {
    // 命中点标记：贴图中心的青色小点，帮助玩家判断判定框
    if (playerDeathT_ <= 0) {
        float a = (invuln_ > 0) ? 0.25f : 0.55f;
        DrawCircle((int)player_.x, (int)player_.y, 13.0f, WithAlpha(pal::kCyan, a * 0.35f));
        DrawCircle((int)player_.x, (int)player_.y, 4.0f, WithAlpha(Color{240, 255, 255, 255}, a + 0.3f));
    }
    drawParticles();
    drawBullets();
    drawPickups();
    if (bombFlash_ > 0) {
        DrawCircle((int)player_.x, (int)player_.y, 900.0f * (1.0f - bombFlash_ / 0.9f), WithAlpha(Color{180, 240, 255, 255}, bombFlash_ * 0.5f));
    }
}

void World::drawHud() {
    // 顶部/底部压暗，保证 HUD 在亮天空上可读
    DrawRectangleGradientV(0, 0, (int)VW, 210, WithAlpha(BLACK, 0.42f), BLANK);
    DrawRectangleGradientV(0, (int)(VH - 170), (int)VW, 170, BLANK, WithAlpha(BLACK, 0.38f));

    // 分数
    ui::Text("SCORE", 48, 30, 22, WithAlpha(pal::kCyan, 0.95f), true);
    char buf[64];
    snprintf(buf, sizeof buf, "%07d", score_);
    ui::TextShadow(buf, 46, 58, 52, pal::kText, WithAlpha(BLACK, 0.55f), true);

    snprintf(buf, sizeof buf, "HI %07d", std::max(save::data().hiScore, score_));
    ui::TextShadow(buf, 48, 122, 22, WithAlpha(pal::kDim, 0.95f), WithAlpha(BLACK, 0.5f));

    if (combo_ > 1) {
        float pop = comboT_ > 2.3f ? 1.0f + (comboT_ - 2.3f) * 0.9f : 1.0f;
        snprintf(buf, sizeof buf, "COMBO x%d", combo_);
        ui::TextShadow(buf, 48, 160, (int)(26 * pop), pal::kGold, WithAlpha(BLACK, 0.5f), true);
    }

    // 生命 / 爆弹
    float rx = VW - 60;
    ui::TextR("生命", rx - 118, 34, 20, WithAlpha(pal::kDim, 0.9f));
    for (int i = 0; i < 5; i++) {
        if (i >= lives_) break;
        float cx = rx - 26 - i * 34, cy = 46;
        // 小飞机图标
        DrawTriangle({cx, cy - 13}, {cx - 13, cy + 10}, {cx + 13, cy + 10}, pal::kCyan);
        DrawTriangle({cx, cy + 15}, {cx - 7, cy + 2}, {cx + 7, cy + 2}, WithAlpha(pal::kCyan, 0.85f));
    }
    ui::TextR("爆弹", rx - 118, 84, 20, WithAlpha(pal::kDim, 0.9f));
    for (int i = 0; i < bombs_; i++) {
        float cx = rx - 26 - i * 30, cy = 100;
        DrawCircle((int)cx, (int)(cy + 4), 10, pal::kGold);
        DrawRectangle((int)cx - 3, (int)cy - 16, 6, 10, WithAlpha(pal::kGold, 0.9f));
        DrawCircle((int)cx, (int)cy - 16, 3, WithAlpha(Color{255, 240, 200, 255}, 0.9f));
    }

    // 波次进度
    float bw = 18, gap = 6;
    float totalW = waveCount_ * (bw + gap) - gap;
    float bx = VW - 48 - totalW, by = VH - 74;
    ui::TextR("进度", bx - 22, by + 2, 20, WithAlpha(pal::kDim, 0.9f));
    for (int i = 0; i < waveCount_; i++) {
        Color c = (i < wave_) ? pal::kCyan : WithAlpha(WHITE, 0.16f);
        DrawRectangle((int)(bx + i * (bw + gap)), (int)by, (int)bw, 26, c);
        if (i < wave_) DrawRectangle((int)(bx + i * (bw + gap)), (int)by, (int)bw, 6, WithAlpha(WHITE, 0.55f));
    }

    // 波次横幅
    if (bannerT_ > 0 && !banner_.empty()) {
        float a = clampf(bannerT_ / 0.45f, 0, 1) * clampf((1.6f - bannerT_) * 4.0f, 0, 1);
        float y = 300 + (1.0f - clampf((1.6f - bannerT_) * 3.0f, 0, 1)) * 34;
        float w = ui::TextW(banner_, 64, true) + 160;
        ui::Panel({VW * 0.5f - w * 0.5f, y - 22, w, 104}, WithAlpha(pal::kPanelBg, 0.72f * a),
                  WithAlpha(pal::kCyan, 0.75f * a), 10, 2);
        ui::TextC(banner_, VW * 0.5f, y, 64, WithAlpha(pal::kText, a), true);
        DrawRectangle((int)(VW * 0.5f - w * 0.5f + 30), (int)(y + 74), (int)(w - 60), 2,
                      WithAlpha(pal::kCyan, 0.4f * a));
    }

    // Boss 血条
    if (state_ == WorldState::Boss || (state_ == WorldState::Cleared && stateT_ < 2.2f)) {
        float w = 1120, x = VW * 0.5f - w * 0.5f, y = 40;
        DrawRectangle((int)x - 4, (int)y - 4, (int)w + 8, 34, WithAlpha(BLACK, 0.6f));
        DrawRectangle((int)x, (int)y, (int)(w * bossHp01()), 26, Color{255, 86, 70, 255});
        DrawRectangle((int)x, (int)y, (int)(w * bossHp01()), 9, Color{255, 168, 140, 255});
        DrawRectangleLinesEx({x, y, w, 26}, 2, WithAlpha(pal::kDanger, 0.9f));
        ui::TextC("IRON FORTRESS", VW * 0.5f, y + 40, 26, WithAlpha(pal::kText, 0.9f), true);
    }

    // 警告
    if (state_ == WorldState::Warning) {
        float p = ui::Pulse(2.2f);
        float a = 0.35f + 0.45f * p;
        for (int i = 0; i < 4; i++) {
            float t = 0.5f - i * 0.11f;
            DrawRectangleLinesEx({0, 0, VW, VH}, 26 - i * 5, WithAlpha(pal::kDanger, a * t * 0.6f));
        }
        ui::TextShadow("警  告", VW * 0.5f - ui::TextW("警  告", 120, true) * 0.5f, 380, 120,
                       WithAlpha(pal::kDanger, 0.75f + 0.25f * p), WithAlpha(BLACK, 0.6f), true);
        ui::TextC("雷暴要塞到达", VW * 0.5f, 540, 40, WithAlpha(pal::kText, 0.9f), true);
    }

    if (flash_ > 0 && !app().optReduceFlash)
        DrawRectangle(0, 0, (int)VW, (int)VH, WithAlpha(Color{255, 240, 230, 255}, flash_ * 0.35f));
}

}  // namespace sky
