#pragma once
// 战斗世界：玩家、敌机、弹幕、Boss、波次
//
// 双人 / 对战说明
// ---------------
// 玩家从单个成员改成 Fighter 数组（最多两人）。两种对局共用同一套模拟：
//   - 组队（campaign）：两人一起打波次，敌机锁定最近的一名玩家，道具谁捡谁用
//   - 对战（versus）  ：不生成敌机，两人在左右半场互相射击，谁先被打光装甲谁输
// 联机采用「锁步」：双方用同一份输入跑同一份模拟，因此 step() 只吃 Ctrl，
// 绝不读 app().in —— 否则两端输入不同步，几十帧后就会飘。
#include "Backdrop.hpp"
#include "Common.hpp"
#include "Pose.hpp"

#include <string>
#include <vector>

namespace sky {

// 全局战斗常量（HUD 与逻辑共用）
constexpr int   kWaveCount = 5;
constexpr float kPlayerMaxHp = 100.0f;
constexpr int   kMaxPlayers = 2;
constexpr float kSimStep = 1.0f / 60.0f;  // 固定步长：联机锁步的前提

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
    int     owner = 0;        // 玩家弹的发射者槽位（对战里用来排除自己）
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
    int        slot = 0;
};

// 一帧的按键（按住状态）。按下沿由 World 内部和上一帧对比得出，
// 这样联机只需要同步 7 个 bit，不需要同步"这一帧刚按下"。
struct Ctrl {
    bool left = false, right = false, up = false, down = false;
    bool fire = false, bomb = false, dash = false;
    uint16_t bits() const {
        return (uint16_t)((left ? 1 : 0) | (right ? 2 : 0) | (up ? 4 : 0) | (down ? 8 : 0) |
                          (fire ? 16 : 0) | (bomb ? 32 : 0) | (dash ? 64 : 0));
    }
    static Ctrl fromBits(uint16_t b) {
        Ctrl c;
        c.left = (b & 1) != 0; c.right = (b & 2) != 0; c.up = (b & 4) != 0; c.down = (b & 8) != 0;
        c.fire = (b & 16) != 0; c.bomb = (b & 32) != 0; c.dash = (b & 64) != 0;
        return c;
    }
};

struct TickInput {
    Ctrl    p[kMaxPlayers];
    bool    mouseAim = false;  // 单人键鼠模式：按住左键跟随光标（联机不用）
    Vector2 mouse{0, 0};
};

// 一名玩家（组队 / 对战通用）
struct Fighter {
    bool       active = false;
    int        slot = 0;
    int        craft = 0;
    int        skin = 0;
    int        face = 1;       // +1 朝右，-1 朝左（对战里 2P 朝左）
    Vector2    pos{500, 700};
    Vector2    vel{0, 0};
    pose::Pose pose;
    float      lastY = 0;
    float      vyN = 0;
    float      invuln = 0;
    float      fireCd = 0;
    float      hp = kPlayerMaxHp;
    int        weapon = 1;
    int        bombs = 2;
    bool       down = false;     // 阵亡 / 等待复活
    float      deathT = 0;       // 死亡爆炸剩余时间（这期间不画机体）
    float      respawnT = 0;     // 组队复活倒计时
    float      dashT = 0;
    float      dashCd = 0;
    Vector2    dashDir{1, 0};
    float      ghostTimer = 0;
    std::vector<Ghost> ghosts;
    uint16_t   prevBits = 0;
    float      hitFlash = 0;
    float      bombFlash = 0;
    int        kills = 0;
};

struct RunSetup {
    int      mode = 0;               // MatchMode（0 = 单人）
    int      craft[kMaxPlayers] = {0, 0};
    int      skin[kMaxPlayers] = {0, 0};
    uint32_t seed = 0x5EEDu;
    bool     online = false;
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
    void start(const RunSetup& setup, bool continueRun);
    void reset();
    // dt 固定为 kSimStep；输入由调用方按槽位准备好（联机从锁步缓冲里取）
    void step(float dt, const TickInput& in, bool autopilot);

    void drawBackground();  // 2D：视差 + 地面阴影
    void draw3D();          // 3D：所有机体
    void drawFore();        // 2D：子弹 / 粒子 / 前景
    void drawHud();         // 2D：HUD

    // 状态查询
    bool  finished() const { return finished_; }
    bool  cleared() const { return state_ == WorldState::Cleared; }
    bool  versus() const { return versus_; }
    bool  online() const { return online_; }
    int   playerCount() const { return playerCount_; }
    int   winner() const { return winner_; }  // 对战：0/1，未分胜负 -1
    const Fighter& fighter(int i) const { return p_[clampi(i, 0, kMaxPlayers - 1)]; }
    int   score() const { return score_; }
    int   kills() const { return kills_; }
    int   maxCombo() const { return maxCombo_; }
    float elapsed() const { return elapsed_; }
    int   stars() const;
    int   hp() const { return (int)p_[0].hp; }
    int   maxHp() const { return (int)kPlayerMaxHp; }
    float hp01() const { return clampf(p_[0].hp / kPlayerMaxHp, 0, 1); }
    int   bombs() const { return p_[0].bombs; }
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

private:
    void updatePlayer(int slot, float dt, const Ctrl& c, bool autopilot);
    void startDash(int slot, Vector2 dir);
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

    // 目标选择：活着且距离最近的玩家
    int     nearestPlayer(Vector2 from) const;
    Vector2 aimTarget(Vector2 from) const;
    bool    anyPlayerAlive() const;

    void addBullet(Vector2 pos, Vector2 vel, float r, Color col, int kind, float dmg, bool hostile,
                   bool big = false, int owner = 0);
    void addBurst(Vector2 pos, float count, float speed, float spread, Color col, float r, float baseAngle,
                  float dmg, bool hostile);
    void addExplosion(Vector2 pos, float size, Color col, int count = 14);
    void addHitSpark(Vector2 pos, Color col);
    void addTrail(Vector2 pos, Vector2 dir, Color col);
    void damagePlayer(int slot, float dmg);
    void damagePlayerPvp(int slot, float dmg);
    void killPlayer(int slot);
    void respawnPlayer(int slot);
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
    void drawPlayerHudPanel(int slot, Rectangle r, bool compact);
    void drawDuelHud();

    // 数据
    Backdrop bg_;
    Backdrop bgStorm_;
    float    stormMix_ = 0;

    Fighter  p_[kMaxPlayers];
    int      playerCount_ = 1;
    bool     versus_ = false;
    bool     online_ = false;
    int      winner_ = -1;

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
};

}  // namespace sky
