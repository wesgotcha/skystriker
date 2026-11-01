#pragma once
// 与 raylib 无关的少量平台能力（避免 windows.h 与 raylib 的宏冲突）
namespace sky::platform {

// 在 InitWindow 之前调用：开启 Per-Monitor V2 DPI 感知，保证高分屏下不糊
void EnableDpiAwareness();
// 主显示器缩放（1.0 = 96 DPI）
float DpiScale();
int   MonitorWidth();
int   MonitorHeight();

}  // namespace sky::platform
