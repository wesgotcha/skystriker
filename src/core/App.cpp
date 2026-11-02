#include "App.hpp"

#include "Assets.hpp"
#include "Audio.hpp"
#include "Match.hpp"
#include "Net.hpp"
#include "Platform.hpp"
#include "Save.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>

namespace sky {

static double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

CameraRig& cameraRig() {
    static CameraRig r;
    static bool init = false;
    if (!init) {
        r.init();
        init = true;
    }
    return r;
}

App& App::I() {
    static App instance;
    return instance;
}

// ---------------------------------------------------------------- CameraRig
void CameraRig::init() {
    const float dist = 3000.0f;
    cam.up = {0, 1, 0};
    cam.fovy = VH;  // 正交高度 = 逻辑高度 → 1 世界单位 = 1 逻辑像素
    cam.projection = CAMERA_ORTHOGRAPHIC;
    cam.target = {VW * 0.5f, 0, VH * 0.5f};
    cam.position = {VW * 0.5f, std::cos(tilt) * dist, VH * 0.5f + std::sin(tilt) * dist};
}

void CameraRig::begin() const {
    BeginMode3D(cam);
    rlDisableBackfaceCulling();
}

void CameraRig::end() const {
    rlEnableBackfaceCulling();
    EndMode3D();
}

Vector3 CameraRig::FromScreen(float sx, float sy) {
    // 屏幕 y = (Z - VH/2) * cos(tilt) + VH/2  ⇒  Z = (y - VH/2)/cos + VH/2
    return {sx, 0.0f, (sy - VH * 0.5f) / CamCos() + VH * 0.5f};
}

// ---------------------------------------------------------------- App
bool App::init(int argc, char** argv) {
    int mpMode = -1;  // 0 合作 / 1 对战
    // 解析命令行
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto val = [&](const char* key) -> const char* {
            size_t n = strlen(key);
            if (a.rfind(key, 0) == 0) return a.c_str() + n;
            return nullptr;
        };
        if (const char* v = val("--shot=")) shotPath = v;
        else if (const char* v = val("--frames=")) shotFrames = atoi(v);
        else if (const char* v = val("--seq=")) { seqDir = v; seqLeft = 9999; }
        else if (const char* v = val("--seqn=")) seqLeft = atoi(v);
        else if (const char* v = val("--warp=")) warpTime = (float)atof(v);
        else if (const char* v = val("--screen=")) {
            std::string s = v;
            if (s == "boot") startAt = Sid::Boot;
            else if (s == "title") startAt = Sid::Title;
            else if (s == "lobby") startAt = Sid::Lobby;
            else if (s == "hangar") startAt = Sid::Hangar;
            else if (s == "game") startAt = Sid::Game;
        } else if (a == "--demo") demoMode = true;
        else if (a == "--god") godMode = true;
        else if (a == "--fps") fpsLog = true;
        else if (a == "--debug-title") debugTitle = true;
        else if (a == "--state-log") stateLog = true;
        else if (a == "--console") consoleWanted = true;
        else if (a == "--time-load") timeLoad = true;
        else if (const char* v = val("--test-esc=")) testEscAt = (float)atof(v);
        else if (const char* v = val("--force-fail=")) forceFailAt = (float)atof(v);
        else if (const char* v = val("--test-key=")) {
            // 形如 R@5.0，键名取首字符
            std::string spec = v;
            size_t at = spec.find('@');
            if (at != std::string::npos) {
                std::string k = spec.substr(0, at);
                testKeyAt = (float)atof(spec.c_str() + at + 1);
                testKey = k.empty() ? 0 : (int)toupper((unsigned char)k[0]);
            }
        }
        else if (const char* v = val("--overlay=")) overlayName = v;
        else if (const char* v = val("--size=")) { sscanf(v, "%dx%d", &forceW, &forceH); }
        else if (a == "--result") forceResult = true;
        // ---- 双人 / 联机调试入口 ----
        else if (const char* v = val("--mp=")) {
            std::string s = v;
            if (s == "coop") mpMode = 0;
            else if (s == "versus") mpMode = 1;
        }
        else if (a == "--host") autoHost = true;
        else if (const char* v = val("--join=")) autoJoinIp = v;
        else if (a == "--autostart") autoNetStart = true;
    }

    // exe 是 GUI 子系统（-mwindows），默认没有控制台。
    // 从 cmd 启动就挂到父控制台，双击启动就什么也不做 —— 不会弹黑框。
    platform::SetupConsole(consoleWanted);

    double tPrev = nowMs();
    auto lap = [&](const char* tag) {
        if (!timeLoad) return;
        double t = nowMs();
        printf("[load] %-14s %7.1f ms\n", tag, t - tPrev);
        fflush(stdout);
        tPrev = t;
    };

    platform::EnableDpiAwareness();
    lap("dpi");

    // 允许从 build/ 或工程根目录启动
    if (!DirectoryExists("assets") && DirectoryExists("../assets")) ChangeDirectory("..");

    // ---- 窗口尺寸 / 位置 --------------------------------------------------
    // 单位陷阱：开了 DPI 感知之后，GLFW 把 InitWindow/SetWindowSize 的尺寸当作
    // "屏幕坐标"，内部还会乘一次显示器缩放。200% 缩放下 InitWindow(2560,1440)
    // 拿到的是 5120x2880 的帧缓冲 —— 比显示器还大，而且它按缩放前的坐标算居中，
    // 结果是负坐标：窗口大半挂在屏幕外。现象就是"画面卡在左上角"。
    // 所以：对外一律按物理像素规划，交给 raylib 之前再除以 DPI。
    float dpiScale = platform::DpiScale();
    if (dpiScale <= 0.0f) dpiScale = 1.0f;

    int areaX = 0, areaY = 0, areaW = 0, areaH = 0;
    platform::WorkArea(&areaX, &areaY, &areaW, &areaH);
    if (areaW <= 0 || areaH <= 0) {
        areaX = areaY = 0;
        areaW = platform::MonitorWidth();
        areaH = platform::MonitorHeight();
    }

    int physW = 0, physH = 0;
    if (forceW > 0 && forceH > 0) {
        physW = forceW;
        physH = forceH;
    } else {
        // 默认：工作区里能放下的最大 16:9。横向留 48px，纵向留 72px
        // （纵向要把标题栏+边框算进去，否则窗口整体比可用区域还高）。
        int availW = std::max(640, areaW - 48);
        int availH = std::max(360, areaH - 72);
        if (availW * 9 >= availH * 16) {  // 屏幕够宽 → 被高度限制
            physH = availH;
            physW = (int)std::lround(physH * 16.0 / 9.0);
        } else {
            physW = availW;
            physH = (int)std::lround(physW * 9.0 / 16.0);
        }
    }

    int reqW = (int)std::lround(physW / dpiScale);
    int reqH = (int)std::lround(physH / dpiScale);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(reqW, reqH, "SKY STRIKER");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    // 兜底：多显示器/不同缩放组合下，GetDpiForSystem() 和 GLFW 实际用的内容缩放
    // 可能不一致（窗口就会比工作区还大）。这里按"实测缩放"（帧缓冲 / 屏幕坐标）
    // 重设一次尺寸，保证任何 DPI 组合下窗口都装得下。
    {
        int swNow = GetScreenWidth();
        float real = swNow > 0 ? (float)GetRenderWidth() / (float)swNow : dpiScale;
        if (real > 0.01f && std::fabs(real - dpiScale) > 0.01f) {
            SetWindowSize((int)std::lround(physW / real), (int)std::lround(physH / real));
            dpiScale = real;
        }
    }

    // 自己摆一次窗口：raylib 的默认居中是按缩放前的坐标算的，高分屏会偏出屏幕。
    // 这里按"整个窗口（含标题栏）的物理矩形"居中，任何 DPI 下都稳稳落在工作区正中。
    platform::CenterWindow(GetWindowHandle(), areaX, areaY, areaW, areaH);

    winW = GetRenderWidth();
    winH = GetRenderHeight();
    winSW = GetScreenWidth();
    winSH = GetScreenHeight();
    updateCanvas();
    if (timeLoad) {
        Vector2 wp = GetWindowPosition();
        printf("[win] dpiScale=%.2f area=%d,%d %dx%d req=%dx%d -> pos=%.0f,%.0f screen=%dx%d "
               "render=%dx%d canvas=%dx%d scale=%.2f origin=%.0f,%.0f\n",
               (double)dpiScale, areaX, areaY, areaW, areaH, reqW, reqH, (double)wp.x, (double)wp.y,
               GetScreenWidth(), GetScreenHeight(), winW, winH, canvas.texture.width,
               canvas.texture.height, (double)scale, (double)origin.x, (double)origin.y);
        fflush(stdout);
    }
    lap("window");

    Image icon = LoadImage("assets/logo/logo_sky_striker.png");
    if (icon.data) {
        ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        SetWindowIcon(icon);
        UnloadImage(icon);
    }
    lap("icon");

    audio::init();
    lap("audio");

    assets::init();
    lap("assets");

    if (forceResult) g_forceResult = true;
    if (!shotPath.empty()) {
        // 截图模式：同步加载全部机体，保证任意屏都能正确渲染
        for (int i = 0; i < assets::modelJobCount(); i++) assets::loadModelJob(i);
        lap("models(all)");
    }

    // 直达某屏（--screen=game 等）不经过 Boot，这里补一次存档读取
    save::load();
    save::data().skin = assets::normalizeSkin(save::data().skin);
    save::data().skin2 = assets::normalizeSkin(save::data().skin2);
    if (mpMode >= 0) {
        bool lan = autoHost || !autoJoinIp.empty();
        MatchMode m = (mpMode == 1) ? (lan ? MatchMode::VersusLan : MatchMode::VersusLocal)
                                    : (lan ? MatchMode::CoopLan : MatchMode::CoopLocal);
        match::setMode(m);
        auto& c = match::cfg();
        c.online = lan;
        c.craft[0] = clampi(save::data().craft, 0, 5);
        c.skin[0] = save::data().skin;
        c.craft[1] = clampi(save::data().craft2, 0, 5);
        c.skin[1] = save::data().skin2;
        c.seed = (uint32_t)(GetTime() * 1000.0);
        if (lan) {
            autoNetMode = (int)m;
            startAt = Sid::Lobby;
            net::init();
        }
    }

    goNow(startAt);
    if (!overlayName.empty()) {
        auto ov = MakeOverlay(overlayName);
        if (ov) pushOverlay(std::move(ov));
    }
    inited_ = true;
    lap("first screen");
    return true;
}

void App::updateCanvas() {
    // 注意：这里必须用"渲染尺寸"（物理像素）而不是 GetScreenWidth()（逻辑尺寸）。
    // 在 125%/150% 缩放的显示器上两者不等，按逻辑尺寸分配画布会让整帧被放大
    // 1.25 倍再呈现，UI 的 1~2px 描边会被插值成一团糊，看起来就是在闪。
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int rw = GetRenderWidth(), rh = GetRenderHeight();
    if (sw <= 0 || sh <= 0 || rw <= 0 || rh <= 0) return;
    dpi = (float)rw / (float)sw;

    // 画布按"能完整装下 1920x1080"的比例渲染（fit），再由 endCanvas 居中贴到窗口。
    // 上限给到 3.0：2K 是 1.33、4K 正好 2.0，都能按原生像素出图；老代码卡在 2.0
    // 会让更大的屏（或 200% 缩放下的 2K+ 窗口）被强行降采样。
    float s = std::min(rw / VW, rh / VH);
    s = clampf(s, 0.25f, 3.0f);
    int cw = (int)std::ceil(VW * s), ch = (int)std::ceil(VH * s);
    // 取整误差：跟窗口差 1px 以内就直接补满，否则边上会留一条 1px 暗线
    if (rw > cw && rw - cw <= 2) cw = rw;
    if (rh > ch && rh - ch <= 2) ch = rh;
    // 缩放比反过来对齐画布尺寸，保证整个画布都被画满（多出的不到 1px 会被裁掉）
    scale = std::max((float)cw / VW, (float)ch / VH);
    size = {(float)cw, (float)ch};
    origin = {std::floor((rw - cw) * 0.5f), std::floor((rh - ch) * 0.5f)};
    if (canvas.id != 0 && canvas.texture.width == cw && canvas.texture.height == ch) return;
    if (canvas.id != 0) UnloadRenderTexture(canvas);
    canvas = LoadRenderTexture(cw, ch);
    // 1:1 贴图，任何插值都只会让边缘变糊
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
}

void App::beginCanvas() {
    BeginTextureMode(canvas);
    ClearBackground(pal::kInk);
}

void App::begin2D() {
    rlDisableBackfaceCulling();
    Camera2D c{};
    c.offset = {0, 0};
    c.target = {0, 0};
    c.rotation = 0;
    c.zoom = scale;
    BeginMode2D(c);
}

void App::end2D() { EndMode2D(); }

void App::endCanvas() {
    EndTextureMode();
    BeginDrawing();
    ClearBackground(pal::kInk);

    // 物理像素 → 窗口 2D 逻辑坐标（raylib 的 2D 空间按逻辑尺寸铺）
    Rectangle src{0, 0, (float)canvas.texture.width, -(float)canvas.texture.height};

    // 画布在窗口里居中：16:9 的窗口正好铺满，其它比例两端留沉稳的暗色边。
    // （试过用"同一帧压暗铺底"去填那两条，会看到重影的 logo / 镜像的云，
    // 比干净的黑边难看得多，已弃用。）
    Rectangle dst{origin.x / dpi, origin.y / dpi, size.x / dpi, size.y / dpi};
    DrawTexturePro(canvas.texture, src, dst, {0, 0}, 0.0f, WHITE);

    // 转场遮罩铺在整屏而不是画布里：否则非 16:9 窗口的补边不会被一起压暗，
    // 淡出时会留下两条亮边在闪。
    drawTransition();

    if (!seqDir.empty() && seqLeft > 0) {
        rlDrawRenderBatchActive();  // 先把批次刷进帧缓冲，否则读到黑屏
        Image img = LoadImageFromScreen();
        char p[512];
        snprintf(p, sizeof p, "%s/f%04d.png", seqDir.c_str(), seqIndex++);
        ExportImage(img, p);
        UnloadImage(img);
        seqLeft--;
    }
    EndDrawing();
}

void App::updateInput() {
    // 用物理尺寸判断是否变化：跨屏拖动时逻辑尺寸可能不变，但 DPI 变了
    int w = GetRenderWidth(), h = GetRenderHeight();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (w != winW || h != winH || sw != winSW || sh != winSH) {
        winSW = sw;
        winSH = sh;
        winW = w;
        winH = h;
        updateCanvas();
    }

    Vector2 m = GetMousePosition();  // 逻辑坐标
    // 鼠标：逻辑 → 物理 → 画布像素 → 逻辑游戏坐标
    Vector2 mp{m.x * dpi, m.y * dpi};
    in.mouse = {(mp.x - origin.x) / scale, (mp.y - origin.y) / scale};
    Vector2 d = GetMouseDelta();
    in.mouseDelta = {d.x * dpi / scale, d.y * dpi / scale};
    in.mouseMoved = (std::fabs(d.x) + std::fabs(d.y)) > 0.5f;
    in.lmb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.lmbPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.lmbReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    in.rmbPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.wheel = GetMouseWheelMove();
    in.anyKey = GetKeyPressed() != 0;
    if (in.mouseMoved) in.mouseActive = true;
    if (in.anyKey) in.mouseActive = false;   // 一动键盘就回到键盘焦点
    in.time = GetTime();

    if (IsKeyPressed(KEY_F11) || (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) toggleFullscreen();
}

void App::toggleFullscreen() {
    if (!optFullscreen) {
        // 进全屏前记下窗口尺寸/位置（屏幕坐标），退出时原样还回去：
        // 原来直接 SetWindowSize(1280*dpi, 720*dpi)，在 200% 缩放下等于
        // 2560x1440 个"屏幕坐标" = 5120x2880 物理像素，比屏幕还大。
        savedW_ = GetScreenWidth();
        savedH_ = GetScreenHeight();
        Vector2 wp = GetWindowPosition();
        savedX_ = (int)wp.x;
        savedY_ = (int)wp.y;

        int m = GetCurrentMonitor();
        // GetMonitorWidth 给的是物理像素，SetWindowSize 吃的是屏幕坐标（内部再乘 DPI）
        float k = platform::DpiScale();
        if (k <= 0.0f) k = 1.0f;
        SetWindowSize((int)std::lround(GetMonitorWidth(m) / k), (int)std::lround(GetMonitorHeight(m) / k));
        SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
        SetWindowPosition(0, 0);
        optFullscreen = true;
    } else {
        ClearWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
        SetWindowSize(savedW_ > 0 ? savedW_ : 1280, savedH_ > 0 ? savedH_ : 720);
        SetWindowPosition(savedX_, savedY_);
        optFullscreen = false;
    }
    winW = GetRenderWidth();
    winH = GetRenderHeight();
    updateCanvas();
}

void App::applyVolume() { audio::applyVolumes(volMaster, volMusic, volSfx); }

void App::go(Sid id) {
    if (trans.active) return;
    trans.active = true;
    trans.t = 0.0f;
    trans.target = id;
    trans.swapped = false;
}

void App::goNow(Sid id) {
    if (cur_) cur_->leave();
    cur_ = MakeScreen(id);
    if (cur_) cur_->enter();
}

void App::beginIntro() {
    // 不留黑场：当前帧就把主菜单换上来，logo 停在启动屏那一帧的位置，
    // 由 TitleScreen 的入场动画继续把它抬到菜单标题位。
    introPending = true;
    trans.active = false;
    trans.t = 0.0f;
    clearOverlaysNow();
    goNow(Sid::Title);
}

void App::pushOverlay(std::unique_ptr<Screen> ov) {
    if (ov) {
        ov->enter();
        overlays_.push_back(std::move(ov));
    }
}

void App::popOverlay() {
    // 只记账，不析构：调用方可能正处在被弹出对象的成员函数里
    if (!overlays_.empty()) pendingPop_++;
}

Screen* App::overlay() { return overlays_.empty() ? nullptr : overlays_.back().get(); }

void App::clearOverlays() { pendingClear_ = true; }

void App::flushOverlayRequests() {
    if (pendingClear_) {
        for (auto& o : overlays_) o->leave();
        overlays_.clear();
        pendingClear_ = false;
        pendingPop_ = 0;
        return;
    }
    while (pendingPop_ > 0 && !overlays_.empty()) {
        overlays_.back()->leave();
        overlays_.pop_back();
        pendingPop_--;
    }
    pendingPop_ = 0;
}

void App::clearOverlaysNow() {
    for (auto& o : overlays_) o->leave();
    overlays_.clear();
    pendingPop_ = 0;
    pendingClear_ = false;
}

void App::drawTransition() {
    // 全屏淡黑：0 → 0.5 压到全黑（此时换屏），0.5 → 1 再揭开。
    // 用 smoothstep 让两端速度为 0，不会出现"啪"一下变黑/变亮的跳变。
    // （原来是 24x14 的像素格随机翻黑，动画感强但碎、换屏时噪点多，已弃用。）
    if (!trans.active) return;
    float t = clampf(trans.t, 0.0f, 1.0f);
    float a = (t < 0.5f) ? (t * 2.0f) : (1.0f - (t - 0.5f) * 2.0f);
    a = a * a * (3.0f - 2.0f * a);
    if (a <= 0.002f) return;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WithAlpha(BLACK, a));
}

void App::run() {
    if (!shotPath.empty()) {
        // 预热若干秒，让动画/战斗进入稳定状态
        // --state-log 在这里也生效（2Hz），方便回放 "第几秒发生了什么"
        int logStep = -1;
        for (float t = 0; t < warpTime; t += 1.0f / 60.0f) {
            updateInput();
            in.forceBack = (testEscAt >= 0.0f && t >= testEscAt && t < testEscAt + 1.0f / 60.0f);
            in.forceKey = (testKeyAt >= 0.0f && t >= testKeyAt && t < testKeyAt + 1.0f / 60.0f) ? testKey : 0;
            if (forceFailAt >= 0.0f && t >= forceFailAt && t < forceFailAt + 1.0f / 60.0f)
                GameDebugForceFail();
            stepSimulation(1.0f / 60.0f);
            if (stateLog && (int)(t * 2.0f) != logStep) {
                logStep = (int)(t * 2.0f);
                printf("[warp %.2f] %s\n", t, GameDebugInfo().c_str());
                fflush(stdout);
            }
        }
        in.forceBack = false;
        in.forceKey = 0;
        int frames = shotFrames > 0 ? shotFrames : 2;
        for (int i = 0; i < frames; i++) {
            updateInput();
            beginCanvas();
            begin2D();
            if (cur_) cur_->drawWorld();
            end2D();
            if (cur_) cur_->draw3D();
            begin2D();
            if (cur_) cur_->draw();
            // 用下标遍历：draw() 里若 push 了新覆盖层，迭代器会失效
        for (size_t i = 0; i < overlays_.size(); i++) overlays_[i]->draw();
            end2D();
            endCanvas();
        }
        Image img = LoadImageFromTexture(canvas.texture);
        ImageFlipVertical(&img);
        ExportImage(img, shotPath.c_str());
        UnloadImage(img);
        printf("[shot] %s  (%dx%d)\n", shotPath.c_str(), canvas.texture.width, canvas.texture.height);
        if (startAt == Sid::Game) printf("[dbg] %s\n", GameDebugInfo().c_str());
        return;
    }

    while (!WindowShouldClose() && !requestExit) {
        updateInput();
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;

        stepSimulation(dt);

        if (fpsLog) {
            static double lastLog = 0;
            static int slow = 0, frames = 0;
            static float worst = 0;
            frames++;
            if (dt > 0.05f) { slow++; if (dt > worst) worst = dt; }
            if (in.time - lastLog >= 1.0) {
                printf("[fps] %2.0f frames=%2d slow=%d worst=%.0fms\n",
                       (double)GetFPS(), frames, slow, worst * 1000.0f);
                fflush(stdout);
                lastLog = in.time; slow = 0; frames = 0; worst = 0;
            }
        }

        if (stateLog) {
            static double lastState = 0;
            if (in.time - lastState >= 1.0) {
                lastState = in.time;
                printf("[state] %s\n", GameDebugInfo().c_str());
                fflush(stdout);
            }
        }

        if (debugTitle) {
            const char* scr = cur_ ? cur_->debugName() : "-";
            const char* ov = overlay() ? overlay()->debugName() : "";
            SetWindowTitle(TextFormat("SKY STRIKER | %s%s%s%s", scr, ov[0] ? "+" : "", ov,
                                     trans.active ? " (trans)" : ""));
        }

        beginCanvas();
        begin2D();
        if (cur_) cur_->drawWorld();
        end2D();
        if (cur_) cur_->draw3D();
        begin2D();
        if (cur_) cur_->draw();
        // 用下标遍历：draw() 里若 push 了新覆盖层，迭代器会失效
        for (size_t i = 0; i < overlays_.size(); i++) overlays_[i]->draw();
        end2D();
        endCanvas();
        if (!seqDir.empty() && seqLeft <= 0) requestExit = true;
    }
}

// 一帧的推进逻辑（正常循环与 --warp 预热共用同一条路径）
void App::stepSimulation(float dt) {
    if (trans.active) {
        trans.t += dt / 0.62f;
        if (!trans.swapped && trans.t >= 0.5f) {
            trans.swapped = true;
            clearOverlaysNow();
            goNow(trans.target);
        }
        if (trans.t >= 1.0f) {
            trans.active = false;
            trans.t = 0.0f;
        }
    }

    Screen* ov = overlay();
    if (ov) {
        ov->update(dt);
        // update 里可能已经请求关闭覆盖层（ESC / 确认按钮）。
        // 这里必须重新取一次：上面的 ov 是裸指针，不能再解引用。
        Screen* top = overlay();
        if (cur_) {
            if (top && top->freezeWorld()) {
                // 冻结：世界完全不更新
            } else {
                // 世界继续动（机库的机体还要转），但这次按键已经被上层吃掉了。
                // 不屏蔽的话，同一次 ESC 会让底下的机库也执行"返回主菜单"。
                Input saved = in;
                in.blocked = true;
                in.lmb = in.lmbPressed = in.lmbReleased = in.rmbPressed = false;
                in.mouseMoved = false;
                in.anyKey = false;
                in.wheel = 0.0f;
                in.forceBack = false;
                cur_->update(dt);
                in = saved;
            }
        }
    } else if (cur_) {
        cur_->update(dt);
    }
    flushOverlayRequests();
}

void App::shutdown() {
    clearOverlaysNow();
    cur_.reset();
    net::shutdown();
    assets::shutdown();
    audio::shutdown();
    if (canvas.id != 0) UnloadRenderTexture(canvas);
    CloseWindow();
}

}  // namespace sky
