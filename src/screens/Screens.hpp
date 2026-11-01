#pragma once
// 所有屏幕/覆盖层的声明
#include "App.hpp"
#include "Backdrop.hpp"
#include "Common.hpp"

#include <functional>
#include <string>
#include <vector>

namespace sky {

// ---------------------------------------------------------------- BOOT
class BootScreen : public Screen {
public:
    void enter() override;
    void update(float dt) override;
    void drawWorld() override;
    void draw() override;

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
};

// ---------------------------------------------------------------- TITLE
class TitleScreen : public Screen {
public:
    void enter() override;
    void update(float dt) override;
    void drawWorld() override;
    void draw() override;

private:
    Backdrop bg_;
    float    t_ = 0;
    int      focus_ = 0;
    float    focusF_ = 0;
    bool     mouseMode_ = false;
    float    menuY_ = 0;
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
    bool updateBehind() const { return true; }

private:
    float t_ = 0;
    int   focus_ = 0;
};

// ---------------------------------------------------------------- GAME
class GameScreen : public Screen {
public:
    void enter() override;
    void update(float dt) override;
    void drawWorld() override;
    void draw3D() override;
    void draw() override;
    bool freezeWorld() const override { return true; }

private:
    float t_ = 0;
};

// 暂停 / 阵亡
class PauseOverlay : public Screen {
public:
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return true; }

private:
    int   focus_ = 0;
    float t_ = 0;
};

class GameOverOverlay : public Screen {
public:
    void update(float dt) override;
    void draw() override;
    bool freezeWorld() const override { return true; }

private:
    float t_ = 0;
    int   focus_ = 0;
    bool  newRecord_ = false;
};

// 全局战斗入口（Game.cpp 提供）
void StartNewRun(bool continueRun);

}  // namespace sky
