#pragma once
// 战斗世界：玩家、敌机、弹幕、Boss、波次
#include "Backdrop.hpp"
#include "Common.hpp"
#include "Pose.hpp"

#include <vector>

namespace sky {

struct Bullet {
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    float   r = 8;
    float   life = 6;
    int     kind = 0;  // 0 圆弹 1 菱形 2 长条
    float   spin = 0;
    float   dmg = 1;
    Color   col = RED;
    bool    alive = false;
};

struct Particle {
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    float   life = 0.5f, maxLife = 0.5f;
    float   size = 4;
    Color   col = WHITE;
    int     kind = 0;  // 0 圆 1 火花 2 烟
    float   spin = 0;
    bool    alive = false;
};

struct Enemy {
    int     type = 0;
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    float   hp = 10, maxHp = 10;
    float   scale = 1.0f;      // 世界缩放（相对基准）
    pose::Pose pose;      // 滚转 / 俯仰（由垂直速度驱动）
    float   lastY = 0;    // 上一帧 y，用来反推垂直速度
    float   fireT = 1.0f;
    float   t = 0;
    float   phase = 0;
    float   hitFlash = 0;
    float   baseY = 0;
    float   stopX = 1200;
    int     pattern = 0;
    int     score = 100;
    bool    alive = false;
    bool    entered = false;
};

struct Pickup {
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    int     kind = 0;  // 0 火力 1 爆弹 2 修复
    float   t = 0;
    bool    alive = false;
};

enum class WorldState { Playing, StormShift, Warning, Boss, Cleared, Failed };

class World {
public:
    void start(bool continueRun);
    void update(float dt, bool autopilot);
    void reset();

    void drawBackground();  // 2D：视差 + 地面阴影
    void draw3D();          // 3D：所有机体
    void drawFore();        // 2D：子弹 / 粒子 / 前景
    void drawHud();         // 2D：HUD

    // 状态查询
    bool  finished() const { return finished_; }
    bool  cleared() const { return state_ == WorldState::Cleared; }
    int   score() const { return score_; }
    int   kills() const { return kills_; }
    int   maxCombo() const { return maxCombo_; }
    float elapsed() const { return elapsed_; }
    int   stars() const;
    int   lives() const { return lives_; }
    int   bombs() const { return bombs_; }
    float bossHp01() const;
    bool  bossActive() const { return state_ == WorldState::Boss && boss_.alive; }
    float shakeAmount() const { return shake_; }
    Vector2 shakeOffset() const;

    // 供暂停菜单使用
    void killPlayerNow();
    std::string debugInfo() const;

private:
    void updatePlayer(float dt, bool autopilot);
    void updateBullets(float dt);
    void updateEnemies(float dt);
    void updateBoss(float dt);
    void updateParticles(float dt);
    void updatePickups(float dt);
    void updateSpawner(float dt);

    void spawnWave(int idx);
    void spawnEnemy(int type, float x, float y, int pattern, float scale = 1.0f);
    void enemyShoot(const Enemy& e);
    void bossShoot(int pattern);
    void addBullet(Vector2 pos, Vector2 vel, float r, Color col, int kind, float dmg = 1);
    void addBurst(Vector2 pos, float count, float speed, float spread, Color col, float r, float baseAngle = 0);
    void addExplosion(Vector2 pos, float size, Color col, int count = 14);
    void addHitSpark(Vector2 pos, Color col);
    void playerHit();
    void damageEnemy(int idx, float dmg);
    void addScore(int base);
    void dropPickup(Vector2 pos);

    void drawShadow(Vector2 pos, float w, float h, float alpha);
    void drawBullets();
    void drawParticles();
    void drawPickups();

    // 数据
    Backdrop bg_;
    Backdrop bgStorm_;
    float    stormMix_ = 0;

    Vector2 player_{500, 700};
    Vector2 playerVel_{0, 0};
    pose::Pose playerPose_;   // 玩家姿态
    float   playerLastY_ = 0; // 上一帧 y
    float   playerVyN_ = 0;   // 归一化垂直速度（+ 向上）
    float   invuln_ = 0;
    float   fireCd_ = 0;
    int     weapon_ = 1;
    int     lives_ = 3;
    int     bombs_ = 2;
    float   bombFlash_ = 0;
    float   playerDeathT_ = 0;

    std::vector<Bullet>   bullets_;
    std::vector<Enemy>    enemies_;
    std::vector<Particle> particles_;
    std::vector<Pickup>   pickups_;

    Enemy   boss_{};
    float   bossEnter_ = 0;

    WorldState state_ = WorldState::Playing;
    float      stateT_ = 0;
    bool       finished_ = false;
    float      elapsed_ = 0;
    int        score_ = 0;
    int        kills_ = 0;
    int        combo_ = 0;
    int        maxCombo_ = 0;
    float      comboT_ = 0;
    float      shake_ = 0;
    float      flash_ = 0;

    int   wave_ = 0;
    int   waveCount_ = 12;
    float waveT_ = 0;
    bool  waveSpawning_ = false;
    int   spawnStep_ = 0;
    float spawnTimer_ = 0;
    float bannerT_ = 0;
    std::string banner_;

    Rng rng_;
    float autopilotT_ = 0;
    Vector2 autoTarget_{500, 700};
};

}  // namespace sky
