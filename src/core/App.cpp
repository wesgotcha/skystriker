#include "App.hpp"

#include "Assets.hpp"
#include "Audio.hpp"
#include "Platform.hpp"

#include <cstdio>
#include <cstring>

namespace sky {

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
        else if (const char* v = val("--warp=")) warpTime = (float)atof(v);
        else if (const char* v = val("--screen=")) {
            std::string s = v;
            if (s == "boot") startAt = Sid::Boot;
            else if (s == "title") startAt = Sid::Title;
            else if (s == "hangar") startAt = Sid::Hangar;
            else if (s == "game") startAt = Sid::Game;
        } else if (a == "--demo") demoMode = true;
        else if (a == "--god") godMode = true;
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
    }

    platform::EnableDpiAwareness();

    // 允许从 build/ 或工程根目录启动
    if (!DirectoryExists("assets") && DirectoryExists("../assets")) ChangeDirectory("..");

    float dpi = platform::DpiScale();
    int w = (int)(1280 * dpi);
    int h = (int)(720 * dpi);
    if (forceW > 0 && forceH > 0) { w = forceW; h = forceH; }
    int mw = platform::MonitorWidth(), mh = platform::MonitorHeight();
    if (w > mw - 80) { float k = (mw - 80.0f) / w; w = (int)(w * k); h = (int)(h * k); }
    if (h > mh - 120) { float k = (mh - 120.0f) / h; w = (int)(w * k); h = (int)(h * k); }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(w, h, "SKY STRIKER");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    winW = GetScreenWidth();
    winH = GetScreenHeight();
    updateCanvas();

    Image icon = LoadImage("assets/logo/logo_sky_striker.png");
    if (icon.data) {
        ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        SetWindowIcon(icon);
        UnloadImage(icon);
    }

    // 转场用的每格随机延迟
    Rng rng;
    rng.seed(1337);
    for (int x = 0; x < 24; x++)
        for (int y = 0; y < 14; y++) cellDelay_[x][y] = rng.range(0.0f, 0.85f);

    audio::init();
    assets::init();

    if (forceResult) g_forceResult = true;
    if (!shotPath.empty()) {
        // 截图模式：同步加载全部机体，保证任意屏都能正确渲染
        for (int i = 0; i < assets::modelJobCount(); i++) assets::loadModelJob(i);
    }
    goNow(startAt);
    if (!overlayName.empty()) {
        auto ov = MakeOverlay(overlayName);
        if (ov) pushOverlay(std::move(ov));
    }
    inited_ = true;
    return true;
}

void App::updateCanvas() {
    int w = GetScreenWidth(), h = GetScreenHeight();
    if (w <= 0 || h <= 0) return;
    float s = std::min(w / VW, h / VH);
    s = clampf(s, 0.35f, 2.0f);
    int cw = (int)(VW * s), ch = (int)(VH * s);
    if (canvas.id != 0 && canvas.texture.width == cw && canvas.texture.height == ch) {
        scale = s;
        size = {(float)cw, (float)ch};
        origin = {std::floor((w - cw) * 0.5f), std::floor((h - ch) * 0.5f)};
        return;
    }
    if (canvas.id != 0) UnloadRenderTexture(canvas);
    canvas = LoadRenderTexture(cw, ch);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    scale = s;
    size = {(float)cw, (float)ch};
    origin = {std::floor((w - cw) * 0.5f), std::floor((h - ch) * 0.5f)};
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
    ClearBackground(BLACK);
    Rectangle src{0, 0, (float)canvas.texture.width, -(float)canvas.texture.height};
    Rectangle dst{origin.x, origin.y, size.x, size.y};
    DrawTexturePro(canvas.texture, src, dst, {0, 0}, 0.0f, WHITE);
    EndDrawing();
}

void App::updateInput() {
    int w = GetScreenWidth(), h = GetScreenHeight();
    if (w != winW || h != winH) {
        winW = w;
        winH = h;
        updateCanvas();
    }

    Vector2 m = GetMousePosition();
    in.mouse = {(m.x - origin.x) / scale, (m.y - origin.y) / scale};
    Vector2 d = GetMouseDelta();
    in.mouseDelta = {d.x / scale, d.y / scale};
    in.mouseMoved = (std::fabs(d.x) + std::fabs(d.y)) > 0.5f;
    in.lmb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.lmbPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.lmbReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    in.rmbPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.wheel = GetMouseWheelMove();
    in.anyKey = GetKeyPressed() != 0;
    in.time = GetTime();

    if (IsKeyPressed(KEY_F11) || (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) toggleFullscreen();
}

void App::toggleFullscreen() {
    if (!IsWindowState(FLAG_WINDOW_UNDECORATED) && !IsWindowFullscreen()) {
        int m = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(m), GetMonitorHeight(m));
        if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
            SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
        }
        optFullscreen = true;
    } else {
        ClearWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
        SetWindowSize((int)(1280 * platform::DpiScale()), (int)(720 * platform::DpiScale()));
        optFullscreen = false;
    }
    winW = GetScreenWidth();
    winH = GetScreenHeight();
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
    if (!trans.active) return;
    float t = trans.t;
    float phase = t < 0.5f ? t * 2.0f : (t - 0.5f) * 2.0f;
    bool  cover = t < 0.5f;
    float cw = VW / 24.0f, chh = VH / 14.0f;
    for (int x = 0; x < 24; x++) {
        for (int y = 0; y < 14; y++) {
            float a = clampf((phase - cellDelay_[x][y] * 0.9f) / 0.10f, 0, 1);
            if (!cover) a = 1.0f - a;
            if (a <= 0.01f) continue;
            DrawRectangle((int)(x * cw), (int)(y * chh), (int)cw + 1, (int)chh + 1, WithAlpha(BLACK, a));
        }
    }
}

void App::run() {
    if (!shotPath.empty()) {
        // 预热若干秒，让动画/战斗进入稳定状态
        for (float t = 0; t < warpTime; t += 1.0f / 60.0f) {
            updateInput();
            in.forceBack = (testEscAt >= 0.0f && t >= testEscAt && t < testEscAt + 1.0f / 60.0f);
            in.forceKey = (testKeyAt >= 0.0f && t >= testKeyAt && t < testKeyAt + 1.0f / 60.0f) ? testKey : 0;
            if (forceFailAt >= 0.0f && t >= forceFailAt && t < forceFailAt + 1.0f / 60.0f)
                GameDebugForceFail();
            stepSimulation(1.0f / 60.0f);
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

        beginCanvas();
        begin2D();
        if (cur_) cur_->drawWorld();
        end2D();
        if (cur_) cur_->draw3D();
        begin2D();
        if (cur_) cur_->draw();
        // 用下标遍历：draw() 里若 push 了新覆盖层，迭代器会失效
        for (size_t i = 0; i < overlays_.size(); i++) overlays_[i]->draw();
        drawTransition();
        end2D();
        endCanvas();
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
    assets::shutdown();
    audio::shutdown();
    if (canvas.id != 0) UnloadRenderTexture(canvas);
    CloseWindow();
}

}  // namespace sky
