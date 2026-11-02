#pragma once
// 所有屏幕/覆盖层的声明
#include "App.hpp"
#include "Backdrop.hpp"
#include "Common.hpp"
#include "Replay.hpp"

#include <functional>
#include <string>
#include <vector>

namespace sky {

// 开场衔接：启动屏的 logo 位 = 主菜单入场动画的起点。
// 两屏共用同一组常量，交接的那一帧 logo 矩形完全一致，看不到位移/缩放跳变。
namespace intro {
constexpr float kLogoAspect = 437.0f / 1299.0f;  // logoTex 宽高比
constexpr float kBootLogoW  = 720.0f;            // 启动屏 logo 宽度
constexpr float kBootLogoY  = 400.0f;            // 启动屏 logo 顶边
constexpr float kTitleLogoW = 900.0f;            // 主菜单 logo 宽度
constexpr float kTitleLogoY = 180.0f;            // 主菜单 logo 顶边
constexpr float kDur        = 0.85f;             // logo 归位总时长（秒）
}  // namespace intro

// ---------------------------------------------------------------- BOOT
class BootScreen : public Screen {
public:
    void enter() override;
    void update(float dt) override;
    void drawWorld() override;
    void draw() override;
    const char* debugName() const override { return "boot"; }

private:
    float t_ = 0;
    float doneT_ = -1;
    int   loaded_ = 0;
    bool  leaving_ = false;
};

// ---------------------------------------------------------------- 通用覆盖层
class DimOverlay : public Screen {
public:
    void drawWorld() override {}
    void update(float dt) override {}
    void draw() override {}
};

// 确认框
class ConfirmOverlay : public Screen {
public:
    ConfirmOverlay(std::string title, std::string body, std::function<void()> onYes,
                   std::string yes = "确定", std::string no = "取消");
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return true; }
    const char* debugName() const override { return "confirm"; }

private:
    std::string title_, body_, yes_, no_;
    std::function<void()> onYes_;
    int   focus_ = 0;  // 0 = 取消, 1 = 确定
    float t_ = 0;
};

// 设置
class SettingsOverlay : public Screen {
public:
    SettingsOverlay();
    void enter() override;
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return true; }
    const char* debugName() const override { return "settings"; }

private:
    int  page_ = 0;
    int  row_ = 0;
    bool rebind_ = false;
    int  rebindRow_ = 0;
    void apply();
};

// 战绩
class RecordsOverlay : public Screen {
public:
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return true; }
    const char* debugName() const override { return "records"; }
};

// 作战复盘：replays/ 里的每局记录，按分数/时长/时间排序，点一行就能重看那一局
class HistoryOverlay : public Screen {
public:
    void enter() override;
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return true; }
    const char* debugName() const override { return "history"; }

private:
    void sortItems();
    void play(int i);
    std::vector<replay::Meta> items_;
    int sort_ = 0;  // 0 最新 / 1 得分 / 2 时长
    int sel_ = 0;
    int top_ = 0;
    float t_ = 0;
};

// ---------------------------------------------------------------- TITLE
class TitleScreen : public Screen {
public:
    void enter() override;
    void update(float dt) override;
    void drawWorld() override;
    void draw() override;
    const char* debugName() const override { return "title"; }

private:
    Backdrop bg_;
    float    t_ = 0;
    int      focus_ = 0;
    float    focusF_ = 0;
    bool     mouseMode_ = false;
    float    menuY_ = 0;
    float    intro_ = 1.0f;   // 开场衔接进度 0→1（1 = 已就位，不再播动画）
    void     activate(int idx);
};

// ---------------------------------------------------------------- HANGAR
class HangarScreen : public Screen {
public:
    enum Mode { Select, Result };
    explicit HangarScreen(Mode m = Select);
    void enter() override;
    void update(float dt) override;
    void drawWorld() override;
    void draw3D() override;
    void draw() override;
    const char* debugName() const override { return mode_ == Result ? "hangar-result" : "hangar"; }

    // 结算数据（由战斗结束时填充）
    struct ResultData {
        int kills = 0;
        int score = 0;
        int maxCombo = 0;
        float time = 0;
        int stars = 0;
        bool newRecord = false;
        bool win = true;
    };
    static ResultData& lastResult();

private:
    Mode  mode_;
    float t_ = 0;
    int   craft_ = 0;
    int   skin_ = 0;
    float yaw_ = 0;
    float appear_[5] = {0, 0, 0, 0, 0};
    float swapT_ = 1.0f;
    bool  mouseMode_ = false;
    int   focus_ = 0;  // 0=选择条 1=出击
    void  changeCraft(int d);
    void  drawStatPanel();
    void  drawStrip();
    void  drawResultPanel();
};

// ---------------------------------------------------------------- 简报
class BriefingOverlay : public Screen {
public:
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return false; }
    const char* debugName() const override { return "briefing"; }
    bool updateBehind() const { return true; }

private:
    float t_ = 0;
    int   focus_ = 0;
};

// ---------------------------------------------------------------- GAME
class GameScreen : public Screen {
public:
    void enter() override;
    void leave() override;
    void update(float dt) override;
    void drawWorld() override;
    void draw3D() override;
    void draw() override;
    bool freezeWorld() const override { return true; }
    const char* debugName() const override { return "game"; }

private:
    float t_ = 0;
};

// 暂停 / 阵亡
class PauseOverlay : public Screen {
public:
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return true; }
    const char* debugName() const override { return "pause"; }

private:
    int   focus_ = 0;
    float t_ = 0;
};

class GameOverOverlay : public Screen {
public:
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return true; }
    const char* debugName() const override { return "gameover"; }

private:
    float t_ = 0;
    int   focus_ = 0;
    bool  newRecord_ = false;
    bool  hasReplay_ = false;
};

// 复盘播放：拿录像里的输入把那一局重跑一遍
class ReplayScreen : public Screen {
public:
    void enter() override;
    void leave() override;
    void update(float dt) override;
    void drawWorld() override;
    void draw3D() override;
    void draw() override;
    const char* debugName() const override { return "replay"; }

private:
    void stepFrame();
    void restart();
    void finishAndBack();

    World        world_;
    replay::Rec  rec_;
    size_t       idx_ = 0;
    int          speedIdx_ = 1;   // 0.5x / 1x / 2x / 4x
    float        acc_ = 0;
    float        played_ = 0;
    bool         paused_ = false;
    bool         ended_ = false;
    bool         godBak_ = false;
    int          craftBak_ = 0;
    int          skinBak_ = 0;
    bool         logged_ = false;
    float        t_ = 0;
};

// 全局战斗入口（Game.cpp 提供）
void StartNewRun(bool continueRun);

}  // namespace sky
