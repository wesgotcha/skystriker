#include "Platform.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cstdio>

namespace sky::platform {

#if defined(_WIN32)

typedef BOOL(WINAPI* SetCtxFn)(HANDLE);
typedef UINT(WINAPI* GetDpiForSystemFn)();
typedef int(WINAPI* GetDpiForWindowFn)(HWND);

static void* Proc(HMODULE m, const char* name) {
    if (!m) return nullptr;
    return (void*)GetProcAddress(m, name);
}

void EnableDpiAwareness() {
    HMODULE user32 = LoadLibraryA("user32.dll");
    // Windows 10 1703+
    auto setCtx = (SetCtxFn)Proc(user32, "SetProcessDpiAwarenessContext");
    if (setCtx) {
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
        if (setCtx((HANDLE)(LONG_PTR)-4)) return;
        if (setCtx((HANDLE)(LONG_PTR)-3)) return;  // PER_MONITOR_AWARE
    }
    // 回退到 shcore.dll（Win 8.1）
    HMODULE shcore = LoadLibraryA("shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI * SetAwarenessFn)(int);
        auto setAwareness = (SetAwarenessFn)Proc(shcore, "SetProcessDpiAwareness");
        if (setAwareness) {
            setAwareness(2);  // PROCESS_PER_MONITOR_DPI_AWARE
            return;
        }
    }
    SetProcessDPIAware();
}

void SetupConsole(bool force) {
    // 情况一：stdout 已经指向有效的文件/管道（live_test.py 就是这么重定向的），
    // 不要抢回来，直接沿用。
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out && out != INVALID_HANDLE_VALUE) return;
    // 情况二：本来就是控制台程序，什么都不用做。
    if (GetConsoleWindow()) return;

    // 情况三：GUI 子系统。父进程有控制台（从 cmd / run.bat 启动）就挂上去；
    // 双击启动时父进程是 explorer，没有控制台，AttachConsole 会失败 —— 于是什么都不发生。
    bool ok = AttachConsole(ATTACH_PARENT_PROCESS) != 0;
    if (!ok && force) ok = AllocConsole() != 0;
    if (!ok) return;

    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);
}

float DpiScale() {
    HMODULE user32 = LoadLibraryA("user32.dll");
    auto getDpi = (GetDpiForSystemFn)Proc(user32, "GetDpiForSystem");
    UINT dpi = 96;
    if (getDpi) dpi = getDpi();
    else {
        HDC dc = GetDC(nullptr);
        if (dc) {
            dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
            ReleaseDC(nullptr, dc);
        }
    }
    if (dpi < 72) dpi = 96;
    return (float)dpi / 96.0f;
}

int MonitorWidth() {
    return GetSystemMetrics(SM_CXSCREEN);
}

int MonitorHeight() {
    return GetSystemMetrics(SM_CYSCREEN);
}

void WorkArea(int* x, int* y, int* w, int* h) {
    RECT r{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &r, 0)) {
        *x = 0;
        *y = 0;
        *w = MonitorWidth();
        *h = MonitorHeight();
        return;
    }
    *x = r.left;
    *y = r.top;
    *w = r.right - r.left;
    *h = r.bottom - r.top;
}

void CenterWindow(void* hwnd, int areaX, int areaY, int areaW, int areaH) {
    HWND h = (HWND)hwnd;
    if (!h || areaW <= 0 || areaH <= 0) return;
    RECT wr{};
    if (!GetWindowRect(h, &wr)) return;
    // 按"整个窗口"（含标题栏）算，只按客户区居中会让标题栏顶出屏幕
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    SetWindowPos(h, nullptr, areaX + (areaW - ww) / 2, areaY + (areaH - wh) / 2, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

#else

void  EnableDpiAwareness() {}
void  SetupConsole(bool) {}
float DpiScale() { return 1.0f; }
int   MonitorWidth() { return 1920; }
int   MonitorHeight() { return 1080; }
void  WorkArea(int* x, int* y, int* w, int* h) { *x = 0; *y = 0; *w = 1920; *h = 1080; }
void  CenterWindow(void*, int, int, int, int) {}

#endif

}  // namespace sky::platform
