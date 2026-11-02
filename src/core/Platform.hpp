#pragma once
// 与 raylib 无关的少量平台能力（避免 windows.h 与 raylib 的宏冲突）
namespace sky::platform {

// 在 InitWindow 之前调用：开启 Per-Monitor V2 DPI 感知，保证高分屏下不糊
void EnableDpiAwareness();
// 在打印任何日志之前调用：exe 是 GUI 子系统，默认没有控制台。
// 从 cmd/PowerShell 启动时挂到父控制台（日志照旧能看到），
// 双击启动时父进程没有控制台，什么也不做 —— 所以不会弹黑框。
// force=true（--console）时即使没有父控制台也新建一个，方便双击调试。
void SetupConsole(bool force);
// 主显示器缩放（1.0 = 96 DPI）
float DpiScale();
int   MonitorWidth();
int   MonitorHeight();

}  // namespace sky::platform
