#pragma once
// UI 绘制原语：切角面板、按钮、文字、属性条、星标
#include "Common.hpp"

namespace sky::ui {

// 文字（左上锚点 / 水平居中 / 右对齐）
void  Text(const std::string& s, float x, float y, int size, Color c, bool bold = false);
void  TextC(const std::string& s, float cx, float y, int size, Color c, bool bold = false);
void  TextR(const std::string& s, float rx, float y, int size, Color c, bool bold = false);
float TextW(const std::string& s, int size, bool bold = false);
void  TextShadow(const std::string& s, float x, float y, int size, Color c, Color shadow, bool bold = false);

// 切角矩形（四角各切 c 像素）
void Panel(Rectangle r, Color fill, Color stroke, float chamfer = 8.0f, float thickness = 2.0f);
void PanelOutline(Rectangle r, Color stroke, float chamfer = 8.0f, float thickness = 2.0f);
void Frame(Rectangle r, Color c, float chamfer = 8.0f);  // 四角装饰线

// 三角形（▶ / ◀），dir = +1 右, -1 左
void Triangle(float cx, float cy, float size, int dir, Color c);
void Star(float cx, float cy, float r, Color c, bool filled = true);

// 分段属性条
void StatBar(float x, float y, float w, float h, float ratio, Color fill, Color bg);

// 全屏压暗
void Dim(float alpha, Color tint = BLACK);

// 按钮：返回是否被点击
bool Button(Rectangle r, const std::string& label, bool focused, bool enabled, int fontSize = 32,
            Color accent = pal::kCyan);

// 鼠标（逻辑坐标）是否在矩形内
bool Hover(Rectangle r);

// 0..1 的呼吸值
float Pulse(float speed = 1.0f, float phase = 0.0f);

}  // namespace sky::ui
