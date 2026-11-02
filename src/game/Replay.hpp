#pragma once
// 战斗复盘：把每一局的「玩家输入 + 每帧 dt」录下来，存进 replays/。
//
// 为什么只存输入：World 里所有随机都走自己的 Rng（reset() 里固定 seed），
// 只要每帧的 dt 和输入序列一致，整局的运动学、弹幕、命中判定就都是确定的。
// 复盘 = 用同一个 World 把同一串输入重跑一遍，不需要存任何世界状态。
//
// 文件格式（小端，逐字段写，不吃结构体对齐）：
//   char[6] magic "SKYRPL" | u16 ver
//   u16 craft, skin | u8 win, god, cont, pad
//   i32 score, kills, combo, stars | f32 time
//   u16 年 月 日 时 分 秒 pad | u32 seed | u32 帧数
//   帧 × { f32 dt, i8 dx, i8 dy, i16 mx, i16 my, u8 flags, u8 pad }
//
// seed 必须录：World 的随机序列由 RunSetup.seed 决定，不存的话复盘是另一局。
#include "World.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sky::replay {

enum FrameFlag : uint8_t {
    F_Fire = 1 << 0,
    F_Bomb = 1 << 1,
    F_Dash = 1 << 2,
    F_Mouse = 1 << 3,
};

struct Frame {
    float   dt = 0;
    float   dx = 0, dy = 0;
    float   mx = 0, my = 0;
    uint8_t flags = 0;

    WorldInput toWorldInput() const;
};

// 一条记录的抬头信息（列表只读这一半，不用把整局帧数据读进来）
struct Meta {
    std::string path;
    int   craft = 0, skin = 0;
    int   score = 0, kills = 0, combo = 0, stars = 0;
    int   year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    float    time = 0;
    uint32_t seed = 0x5EEDu;
    bool     win = false, god = false, cont = false;
    int      frames = 0;

    long long stamp() const;    // 20261102161945，用于「最新」排序
    std::string when() const;   // 2026.11.02 16:19
    std::string length() const; // 2:14
};

struct Rec {
    Meta               meta;
    std::vector<Frame> frames;
};

constexpr int kMaxKeep = 24;         // 最多保留多少条（超出就删最旧的）
constexpr int kMaxFrames = 60 * 60 * 20;  // 单局最多录 20 分钟

void Begin(Rec& r, bool cont, bool god, int craft, int skin, uint32_t seed);
void Push(Rec& r, const WorldInput& in, float dt);
bool Finish(Rec& r, const World& w);  // 填结算数据并落盘

std::vector<Meta> List();             // 扫描 replays/，按时间倒序
bool Load(const std::string& path, Rec& out);

// 列表里点「复盘」→ 记下路径 → go(Sid::Replay) → ReplayScreen::enter 取走
void        SetPending(const std::string& path);
std::string TakePending();
const std::string& LastSaved();       // 本局刚存下的那条（阵亡界面「看回放」用）
void        ClearLastSaved();
bool        HasAny();

void Delete(const std::string& path);  // 删除单条记录（历史面板「删除」用）
void ClearAll();                       // 清空全部记录（历史面板「清空」用，二次确认后调用）

}  // namespace sky::replay
