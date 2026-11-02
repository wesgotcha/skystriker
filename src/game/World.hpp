#pragma once
// 战斗世界：玩家、敌机、弹幕、Boss、波次
#include "Backdrop.hpp"
#include "Common.hpp"
#include "Pose.hpp"

#include <string>
#include <vector>

namespace sky {

// 全局战斗常量（HUD 与逻辑共用）
constexpr int   kWaveCount = 5;
constexpr float kPlayerMaxHp = 100.0f;

struct Bullet {
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    float   r = 8;
    float   life = 6;
    int     kind = 0;       // 0 圆弹 1 菱形 2 长条（玩家）
    float   spin = 0;
    float   dmg = 1;
    Color   col = RED;
    bool    big = false;      // 大炮弹：体型大、伤害高
    bool    hostile = false;  // 敌弹（true）/ 玩家弹（false）
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

// 敌机三类（编号与素材表解耦，见 World.cpp 的 kEnemy）
enum EnemyClass {
    EC_C = 0,   // 侦察机：单发点射
    EC_B = 1,   // 炮击机：三向散射 + 偶发大炮弹
    EC_A = 2,   // 冲锋机：蓄力锁定后直冲撞击
    EC_BOSS = 3
};

// 敌机行为状态
//
// 注意：冲撞结束后**没有**「原路折返」这个状态。冲空的机体继续往前飞出画面，
// 再瞬移到右侧外面重新 ES_Enter 飞回来（见 World.cpp 的 ES_Dash 收尾）。
enum EnemyState {
    ES_Wait = 0,  // 尚未出场（出场延迟）
    ES_Enter,     // 从右侧飞入，抵达右半场巡航位
    ES_Patrol,    // 巡航：全场绝大多数时间处于这个状态
    ES_Charge,    // A / Boss：蓄力锁定（玩家闪身的窗口）
    ES_Dash,      // A / Boss：直线冲撞
    ES_Dead
};

struct Enemy {
    int      cls = EC_C;
    int      st = ES_Wait;
    Vector2  pos{0, 0};
    Vector2  vel{0, 0};
    Vector2  spawn{0, 0};  // 入场起点（也用于冲撞后重新进场）
    Vector2  home{0, 0};   // 巡航锚点，永远在右半场
    Vector2  lockDir{1, 0};
    Vector2  lockTo{0, 0};
    float    hp = 10, maxHp = 10;
    float    hpTrail = 1.0f;  // 血条上滞后的白色掉血条
    float    scale = 1.0f;
    pose::Pose pose;
    float    lastY = 0;
    float    vyN = 0;         // 平滑后的归一化垂直速度，专门喂给姿态
    bool     posJump = false; // 本帧发生瞬移（换场/回场）：姿态别拿这段位移当速度
    float    t = 0;
    float    phase = 0;
    float    stateT = 0;
    float    fireT = 1.0f;
    float    specialT = 4.0f;  // A 冲锋 / B 大炮弹的冷却
    float    tell = 0;         // 大炮弹的开火预告
    float    hitFlash = 0;
    float    delay = 0;
    int      score = 100;
    bool     alive = false;
};

struct Pickup {
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    int     kind = 0;  // 0 火力 1 爆弹 2 修复
    float   t = 0;
    bool    alive = false;
};

// 闪身残影
struct Ghost {
    Vector2    pos{0, 0};
    pose::Pose pose;
    float      life = 0.3f, maxLife = 0.3f;
};

// 一帧的玩家输入（已经解过键位/对角线归一化）。
//
// 实时游玩时由 updatePlayer() 从 app().in 里解出来，复盘时由 ReplayScreen
// 按录像逐帧灌进来 —— 两条路走的是同一段逻辑，录像才能演回原样。
// 触发类（bomb / dash）记的是"这一帧按下了"，不是"按住"。
struct WorldInput {
    float dirX = 0, dirY = 0;
    float mouseX = 0, mouseY = 0;
    bool  mouseSteer = false;
    bool  fire = false;
    bool  bomb = false;
    bool  dash = false;
};

enum class WorldState { Playing, Cleared, Failed };

// 波次阶段
enum WavePhase {
    WP_Banner = 0,  // 横幅（"WAVE n"），结束后生成
    WP_Fight,       // 清场中：必须打完本波所有敌人才进下一波
    WP_Gap          // "WAVE CLEAR" 间隔
};

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
    int   hp() const { return (int)hp_; }
    int   maxHp() const { return (int)kPlayerMaxHp; }
    float hp01() const { return clampf(hp_ / kPlayerMaxHp, 0, 1); }
    int   bombs() const { return bombs_; }
    int   waveIndex() const { return wave_; }
    int   waveTotal() const { return kWaveCount; }
    int   enemiesLeft() const;
    float dashReady01() const;
    float bossHp01() const;
    bool  bossActive() const { return boss_.alive; }
    float shakeAmount() const { return shake_; }
    Vector2 shakeOffset() const;

    // 供暂停菜单使用
    void killPlayerNow();
    std::string debugInfo() const;

    // 复盘通道：录像帧灌进来之后，updatePlayer 不再读键盘/鼠标
    void setScriptedInput(const WorldInput& wi) { script_ = wi; scripted_ = true; }
    void clearScriptedInput() { scripted_ = false; }
    const WorldInput& lastInput() const { return lastInput_; }  // 本帧实际吃到的输入（录像用）
    bool scripted() const { return scripted_; }

private:
    void updatePlayer(float dt, bool autopilot);
    void startDash(Vector2 dir);
    void updateBullets(float dt);
    void updateEnemies(float dt);
    void updateBoss(float dt);
    void updateParticles(float dt);
    void updatePickups(float dt);
    void updateWave(float dt);

    void updateC(Enemy& e, float dt);
    void updateB(Enemy& e, float dt);
    void updateA(Enemy& e, float dt);

    void spawnWave(int idx);
    void spawnEnemy(int cls, float delay);
    void activateBoss();
    void beginCharge(Enemy& e);
    void beginBossCharge();
    void killBoss();
    void enemyShoot(const Enemy& e);
    void bossShoot(int pattern);
    int  pickBossAttack(float hp01);
    bool enemiesAllDead() const;
    void showBanner(const std::string& s, float dur);

    // 姿态 / 回场
    void placeEnemy(Enemy& e, Vector2 p);
    void updateEnemyPose(Enemy& e, float dt);

    void addBullet(Vector2 pos, Vector2 vel, float r, Color col, int kind, float dmg, bool hostile, bool big = false);
    void addBurst(Vector2 pos, float count, float speed, float spread, Color col, float r, float baseAngle,
                  float dmg, bool hostile);
    void addExplosion(Vector2 pos, float size, Color col, int count = 14);
    void addHitSpark(Vector2 pos, Color col);
    void addTrail(Vector2 pos, Vector2 dir, Color col);
    void damagePlayer(float dmg);
    void damageEnemy(int idx, float dmg);
    void ramExplode(Enemy& e);
    void addScore(int base);
    void dropPickup(Vector2 pos);                       // 按概率掉落（每波至少保底一颗）
    void spawnPickup(Vector2 pos, int kind);            // 无条件生成一颗，出场就往玩家一侧弹

    void drawShadow(Vector2 pos, float w, float h, float alpha);
    void drawTelegraphs();
    void drawBullets();
    void drawParticles();
    void drawPickups();
    void drawEnemyBars();

    // 数据
    Backdrop bg_;
    Backdrop bgStorm_;
    float    stormMix_ = 0;

    Vector2  player_{500, 700};
    Vector2  playerVel_{0, 0};
    pose::Pose playerPose_;
    float    playerLastY_ = 0;
    float    playerVyN_ = 0;
    float    invuln_ = 0;
    float    fireCd_ = 0;
    int      weapon_ = 1;
    float    hp_ = 100;
    int      bombs_ = 2;
    float    bombFlash_ = 0;
    float    playerDeathT_ = 0;

    // 闪身
    float    dashT_ = 0;
    float    dashCd_ = 0;
    Vector2  dashDir_{1, 0};
    float    ghostTimer_ = 0;
    std::vector<Ghost> ghosts_;

    std::vector<Bullet>   bullets_;
    std::vector<Enemy>    enemies_;
    std::vector<Particle> particles_;
    std::vector<Pickup>   pickups_;

    Enemy   boss_{};
    float   bossEnter_ = 0;
    float   bossEnterDur_ = 2.0f;
    bool    bossPending_ = false;
    float   bossDelay_ = 0;

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

    int        wave_ = 0;
    int        wavePhase_ = WP_Banner;
    float      waveT_ = 0;
    bool       waveDrop_ = false;  // 本波是否已经掉过道具（保底判定）
    float      bannerT_ = 0;
    float      bannerDur_ = 1.6f;
    std::string banner_;

    Rng     rng_;
    float   autopilotT_ = 0;
    Vector2 autoTarget_{500, 700};

    WorldInput script_;      // 复盘灌进来的本帧输入
    WorldInput lastInput_;   // 本帧实际输入（录像直接抄它，和逻辑天然一致）
    bool       scripted_ = false;
};

}  // namespace sky
