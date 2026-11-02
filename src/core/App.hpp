#pragma once
// 应用外壳：窗口、逻辑画布缩放、输入、屏幕栈、像素化转场
#include "Common.hpp"

#include <memory>

namespace sky {

enum class Sid { Boot, Title, Hangar, Game, Replay };

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
    // 调试用：当前屏幕名字，会拼进窗口标题，方便自动化测试判断状态
    virtual const char* debugName() const { return "?"; }
};

struct Input {
    Vector2 mouse{0, 0};      // 逻辑坐标
    Vector2 mouseDelta{0, 0};
    bool    mouseMoved = false;
    // 只有真正动过鼠标，悬停才算数：否则窗口一出现，光标恰好压在某个菜单项上，
    // 焦点会被它抢走（键盘操作时尤其容易误触发）
    bool    mouseActive = false;
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

    void go(Sid id);       // 带转场（全屏淡黑）
    void goNow(Sid id);    // 立即
    // 启动屏 → 主菜单专用：不用转场，logo 由 TitleScreen 的入场动画接着演，
    // 中间不黑屏、不跳位（见 TitleScreen::intro）
    void beginIntro();

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
    float    dpi = 1.0f;        // 物理像素 / 逻辑像素（125% 缩放 = 1.25）
    float    scale = 1.0f;      // 逻辑 → 画布像素
    Vector2  origin{0, 0};      // 画布在窗口中的左上角（物理像素）
    Vector2  size{0, 0};        // 画布尺寸（物理像素）
    int      winW = 0, winH = 0;      // 渲染尺寸（物理像素）
    int      winSW = 0, winSH = 0;    // 窗口尺寸（屏幕坐标，用于感知 DPI 变化）

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
    // 调试：逐帧抓屏（真实呈现链路）
    std::string seqDir;
    int         seqLeft = 0;
    int         seqIndex = 0;
    float       warpTime = 0.0f;
    Sid         startAt = Sid::Boot;
    bool        demoMode = false;
    bool        godMode = false;
    bool        fpsLog = false;   // 调试：每秒打印一次帧率
    bool        debugTitle = false;  // 调试：把当前屏幕写进窗口标题
    bool        stateLog = false;    // 调试：每秒打印一次战斗状态
    bool        consoleWanted = false;  // 调试：--console 强制开一个控制台窗口
    bool        timeLoad = false;    // 调试：--time-load 打印各阶段加载耗时
    std::string overlayName;
    int         forceW = 0, forceH = 0;
    float       testEscAt = -1.0f;  // 调试：在第 N 秒注入一次 ESC
    int         testKey = 0;        // 调试：--test-key=R@5
    float       testKeyAt = -1.0f;
    float       forceFailAt = -1.0f;  // 调试：第 N 秒强制阵亡
    bool        forceFailDone = false;
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

    // 开场衔接标记：启动屏结束、主菜单还没播入场动画时为 true
    bool introPending = false;

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
    bool  inited_ = false;
    // 全屏前的窗口状态，退出全屏时还原
    int   savedW_ = 0, savedH_ = 0, savedX_ = 0, savedY_ = 0;
};

CameraRig& cameraRig();

std::unique_ptr<Screen> MakeScreen(Sid id);
std::unique_ptr<Screen> MakeOverlay(const std::string& name);
std::string GameDebugInfo();
void        GameDebugForceFail();
extern bool g_forceResult;

inline App& app() { return App::I(); }

}  // namespace sky
