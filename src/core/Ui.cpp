#include "Ui.hpp"

#include "App.hpp"
#include "Assets.hpp"

namespace sky::ui {

static float Spacing(int size) { return std::round(size * 0.03f); }

void Text(const std::string& s, float x, float y, int size, Color c, bool bold) {
    Font& f = bold ? assets::title(size) : assets::body(size);
    DrawTextEx(f, s.c_str(), {std::round(x), std::round(y)}, (float)size, Spacing(size), c);
}

void TextC(const std::string& s, float cx, float y, int size, Color c, bool bold) {
    float w = TextW(s, size, bold);
    Text(s, cx - w * 0.5f, y, size, c, bold);
}

void TextR(const std::string& s, float rx, float y, int size, Color c, bool bold) {
    float w = TextW(s, size, bold);
    Text(s, rx - w, y, size, c, bold);
}

float TextW(const std::string& s, int size, bool bold) {
    Font& f = bold ? assets::title(size) : assets::body(size);
    return MeasureTextEx(f, s.c_str(), (float)size, Spacing(size)).x;
}

void TextShadow(const std::string& s, float x, float y, int size, Color c, Color shadow, bool bold) {
    Text(s, x + 2, y + 3, size, shadow, bold);
    Text(s, x, y, size, c, bold);
}

void Panel(Rectangle r, Color fill, Color stroke, float ch, float th) {
    float x = r.x, y = r.y, w = r.width, h = r.height;
    Vector2 p[8] = {{x + ch, y},           {x + w - ch, y},     {x + w, y + ch},     {x + w, y + h - ch},
                    {x + w - ch, y + h},   {x + ch, y + h},     {x, y + h - ch},     {x, y + ch}};
    if (fill.a > 0) DrawTriangleFan(p, 8, fill);
    if (stroke.a > 0 && th > 0) {
        for (int i = 0; i < 8; i++) {
            Vector2 a = p[i], b = p[(i + 1) % 8];
            DrawLineEx(a, b, th, stroke);
        }
    }
}

void PanelOutline(Rectangle r, Color stroke, float ch, float th) { Panel(r, BLANK, stroke, ch, th); }

void Frame(Rectangle r, Color c, float ch) {
    float x = r.x, y = r.y, w = r.width, h = r.height;
    float L = 26.0f, t = 3.0f;
    // 四个角的 L 形装饰
    DrawRectangle((int)(x + ch), (int)y, (int)L, (int)t, c);
    DrawRectangle((int)x, (int)(y + ch), (int)t, (int)L, c);
    DrawRectangle((int)(x + w - ch - L), (int)y, (int)L, (int)t, c);
    DrawRectangle((int)(x + w - t), (int)(y + ch), (int)t, (int)L, c);
    DrawRectangle((int)(x + ch), (int)(y + h - t), (int)L, (int)t, c);
    DrawRectangle((int)x, (int)(y + h - ch - L), (int)t, (int)L, c);
    DrawRectangle((int)(x + w - ch - L), (int)(y + h - t), (int)L, (int)t, c);
    DrawRectangle((int)(x + w - t), (int)(y + h - ch - L), (int)t, (int)L, c);
}

void Triangle(float cx, float cy, float size, int dir, Color c) {
    float h = size * 0.5f;
    if (dir > 0) {
        DrawTriangle({cx + h, cy}, {cx - h * 0.8f, cy - h}, {cx - h * 0.8f, cy + h}, c);
    } else {
        DrawTriangle({cx - h, cy}, {cx + h * 0.8f, cy + h}, {cx + h * 0.8f, cy - h}, c);
    }
}

void Star(float cx, float cy, float r, Color c, bool filled) {
    Vector2 pts[10];
    for (int i = 0; i < 10; i++) {
        float a = -PI / 2 + i * PI / 5.0f;
        float rr = (i % 2 == 0) ? r : r * 0.44f;
        pts[i] = {cx + std::cos(a) * rr, cy + std::sin(a) * rr};
    }
    if (filled) {
        for (int i = 1; i < 9; i++) DrawTriangle(pts[0], pts[i], pts[i + 1], c);
    } else {
        for (int i = 0; i < 10; i++) DrawLineEx(pts[i], pts[(i + 1) % 10], 2.0f, c);
    }
}

void StatBar(float x, float y, float w, float h, float ratio, Color fill, Color bg) {
    const int seg = 10;
    float gap = 3.0f;
    float sw = (w - gap * (seg - 1)) / seg;
    int lit = (int)std::round(clampf(ratio, 0, 1) * seg);
    for (int i = 0; i < seg; i++) {
        Color c = (i < lit) ? fill : bg;
        DrawRectangle((int)(x + i * (sw + gap)), (int)y, (int)sw, (int)h, c);
    }
}

void Dim(float alpha, Color tint) { DrawRectangle(0, 0, (int)VW, (int)VH, WithAlpha(tint, alpha)); }

bool Hover(Rectangle r) {
    Vector2 m = app().in.mouse;
    return m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height;
}

float Pulse(float speed, float phase) {
    return 0.5f + 0.5f * std::sin((float)GetTime() * speed * 2.0f * PI + phase);
}

bool Button(Rectangle r, const std::string& label, bool focused, bool enabled, int fontSize, Color accent) {
    bool hovered = enabled && Hover(r);
    bool click = false;
    if (hovered && app().in.lmbPressed) click = true;

    float lift = focused ? -2.0f : 0.0f;
    if (hovered && app().in.lmb) lift = 2.0f;

    Rectangle rr{r.x, r.y + lift, r.width, r.height};
    Color fill = enabled ? (focused ? WithAlpha(accent, 0.18f) : WithAlpha(pal::kPanelBg, 0.9f))
                         : WithAlpha(Color{20, 26, 34, 255}, 0.85f);
    Color stroke = enabled ? (focused ? accent : WithAlpha(accent, 0.45f)) : Color{60, 70, 82, 255};
    Panel(rr, fill, stroke, 8.0f, focused ? 2.5f : 2.0f);
    if (focused) {
        ui::Triangle(rr.x - 26, rr.y + rr.height * 0.5f, 16, 1, accent);
        ui::Triangle(rr.x + rr.width + 26, rr.y + rr.height * 0.5f, 16, -1, accent);
    }
    Color tc = enabled ? (focused ? pal::kText : pal::kDim) : Color{70, 80, 92, 255};
    TextC(label, rr.x + rr.width * 0.5f, rr.y + (rr.height - fontSize) * 0.5f - 1, fontSize, tc, focused);
    return click;
}

}  // namespace sky::ui
