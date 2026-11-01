#include "Platform.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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

#else

void  EnableDpiAwareness() {}
float DpiScale() { return 1.0f; }
int   MonitorWidth() { return 1920; }
int   MonitorHeight() { return 1080; }

#endif

}  // namespace sky::platform
