#pragma once
// 应用外壳：窗口、逻辑画布缩放、输入、屏幕栈、像素化转场
#include "Common.hpp"

#include <memory>

namespace sky {

enum class Sid { Boot, Title, Hangar, Game };

// 一屏的接口。绘制被拆成四段，方便在 2D / 3D 之间切换：
//   drawWorld()  → 2D 背景（视差层、地面阴影）
//   draw3D()     → 3D 机体
//   draw()       → 2D 前景（子弹、特效、HUD）
class Screen {
public:
    virtual ~Screen() = default;
    virtual void enter() {}
    virtual void leave() {}
    virtual void update(float dt) = 0;
    virtual void drawWorld() {}
    virtual void draw3D() {}
    virtual void draw() = 0;
    // 覆盖层弹出时，底下的世界是否冻结
    virtual bool freezeWorld() const { return false; }
};

struct Input {
    Vector2 mouse{0, 0};      // 逻辑坐标
    Vector2 mouseDelta{0, 0};
    bool    mouseMoved = false;
    bool    lmb = false, lmbPressed = false, lmbReleased = false;
    bool    rmbPressed = false;
    float   wheel = 0.0f;
    bool    anyKey = false;
    bool    forceBack = false;   // 调试：注入一次 ESC
    int     forceKey = 0;        // 调试：本帧额外注入的按键码
    bool    blocked = false;     // 被上层覆盖层吞掉输入（底层屏幕仍然要跑动画）
    double  time = 0.0;

    bool pressed(int k) const { return !blocked && (k == forceKey || IsKeyPressed(k)); }
    bool down(int k) const { return !blocked && IsKeyDown(k); }
    bool repeat(int k) const { return !blocked && (IsKeyPressed(k) || IsKeyPressedRepeat(k)); }

    bool navUp() const { return repeat(KEY_UP) || repeat(KEY_W); }
    bool navDown() const { return repeat(KEY_DOWN) || repeat(KEY_S); }
    bool navLeft() const { return repeat(KEY_LEFT) || repeat(KEY_A); }
    bool navRight() const { return repeat(KEY_RIGHT) || repeat(KEY_D); }
    bool ok() const { return pressed(KEY_ENTER) || pressed(KEY_KP_ENTER) || pressed(KEY_SPACE); }
    bool back() const {
        return !blocked && (forceBack || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE));
    }
};

// 3D 机体层：正交俯视相机，1 世界单位 = 1 逻辑像素
struct CameraRig {
    Camera3D cam{};
    float tilt = kCamTilt;
    void init();
    void begin() const;
    void end() const;
    // 逻辑屏幕坐标 → 世界坐标（机体都在同一高度平面上）
    static Vector3 FromScreen(float sx, float sy);
    static float TiltCos() { return CamCos(); }
};

class App {
public:
    static App& I();

    bool init(int argc, char** argv);
    void run();
    void shutdown();

    void go(Sid id);       // 带转场
    void goNow(Sid id);    // 立即

    void pushOverlay(std::unique_ptr<Screen> ov);
    // 关闭覆盖层：延迟到本帧推进结束时才真正析构。
    // 覆盖层常常在自己的 update()/draw() 里调用它，立即 delete 会让 this 悬空。
    void popOverlay();
    Screen* overlay();
    bool hasOverlay() const { return !overlays_.empty(); }
    void clearOverlays();  // 同上，延迟生效

    Screen* screen() { return cur_.get(); }

    // 画布
    RenderTexture2D canvas{};
    float    scale = 1.0f;      // 逻辑 → 画布像素
    Vector2  origin{0, 0};      // 画布在窗口中的左上角
    Vector2  size{0, 0};        // 画布在窗口中的尺寸
    int      winW = 0, winH = 0;

    void beginCanvas();
    void begin2D();
    void end2D();
    void endCanvas();

    Input in;

    // 全局开关（设置页可改）
    bool  optShake = true;
    bool  optReduceFlash = false;
    bool  optFullscreen = false;
    float volMaster = 80.0f, volMusic = 45.0f, volSfx = 85.0f;

    void toggleFullscreen();
    void applyVolume();

    // 命令行（截图/直达屏）
    std::string shotPath;
    int         shotFrames = 0;
    float       warpTime = 0.0f;
    Sid         startAt = Sid::Boot;
    bool        demoMode = false;
    bool        godMode = false;
    std::string overlayName;
    int         forceW = 0, forceH = 0;
    float       testEscAt = -1.0f;  // 调试：在第 N 秒注入一次 ESC
    int         testKey = 0;        // 调试：--test-key=R@5
    float       testKeyAt = -1.0f;
    float       forceFailAt = -1.0f;  // 调试：第 N 秒强制阵亡
    bool        forceResult = false;

    // 转场
    struct Transition {
        bool  active = false;
        float t = 0.0f;
        Sid   target = Sid::Title;
        bool  swapped = false;
    } trans;

    void drawTransition();
    void stepSimulation(float dt);

    bool requestExit = false;

private:
    App() = default;
    void updateCanvas();
    void drawCanvasToWindow();
    void updateInput();

    void flushOverlayRequests();  // 真正执行 pop/clear
    void clearOverlaysNow();

    std::unique_ptr<Screen> cur_;
    std::vector<std::unique_ptr<Screen>> overlays_;
    int  pendingPop_ = 0;
    bool pendingClear_ = false;
    std::unique_ptr<Screen> pending_;
    float cellDelay_[24][14]{};
    bool  inited_ = false;
};

CameraRig& cameraRig();

std::unique_ptr<Screen> MakeScreen(Sid id);
std::unique_ptr<Screen> MakeOverlay(const std::string& name);
std::string GameDebugInfo();
void        GameDebugForceFail();
extern bool g_forceResult;

inline App& app() { return App::I(); }

}  // namespace sky
