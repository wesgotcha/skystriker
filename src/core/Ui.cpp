#include "Ui.hpp"

#include "App.hpp"
#include "Assets.hpp"

namespace sky::ui {

static float Spacing(int size) { return std::round(size * 0.03f); }

// ---------------------------------------------------------------- 像素网格
// 画布按物理像素分配（125% 缩放时 1920 逻辑 → 2000 画布），逻辑坐标乘上 zoom
// 之后经常落在半个像素上：1~2px 的描边会一会儿 1px、一会儿 2px，扫过去就是
// "边框在闪"。所有 UI 图元统一对齐到画布像素网格，描边宽度也量化成整数像素。
float K() { return app().scale > 0.0f ? app().scale : 1.0f; }

float Px(float v) { return std::round(v * K()) / K(); }

Rectangle Snap(Rectangle r) {
    float k = K();
    float x0 = std::round(r.x * k), y0 = std::round(r.y * k);
    float x1 = std::round((r.x + r.width) * k), y1 = std::round((r.y + r.height) * k);
    return {x0 / k, y0 / k, (x1 - x0) / k, (y1 - y0) / k};
}

// 描边宽度：对齐到整数个画布像素，至少 1px
static float ThickPx(float th) {
    float k = K();
    float px = std::round(th * k);
    if (px < 1.0f) px = 1.0f;
    return px / k;
}

// ---------------------------------------------------------------- 文字
void Text(const std::string& s, float x, float y, int size, Color c, bool bold) {
    Font& f = bold ? assets::title(size) : assets::body(size);
    DrawTextEx(f, s.c_str(), {Px(x), Px(y)}, (float)size, Spacing(size), c);
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

// ---------------------------------------------------------------- 面板
// 切角八边形顶点（顺时针，y 向下）
static void ChamferPts(Rectangle r, float ch, Vector2* p) {
    float x = r.x, y = r.y, w = r.width, h = r.height;
    float lim = std::min(w, h) * 0.5f;
    if (ch > lim) ch = lim;
    if (ch < 0) ch = 0;
    p[0] = {x + ch, y};
    p[1] = {x + w - ch, y};
    p[2] = {x + w, y + ch};
    p[3] = {x + w, y + h - ch};
    p[4] = {x + w - ch, y + h};
    p[5] = {x + ch, y + h};
    p[6] = {x, y + h - ch};
    p[7] = {x, y + ch};
}

// 内外两圈之间的等宽环。八条四边形拼成，相邻边严格共用顶点：拐角不会叠加变亮，
// 也不会像 DrawLineEx 那样在斜切角上留缺口、出现粗细跳变。
static void Ring(const Vector2* o, const Vector2* in, Color c) {
    for (int i = 0; i < 8; i++) {
        int n = (i + 1) % 8;
        DrawTriangle(o[i], o[n], in[n], c);
        DrawTriangle(o[i], in[n], in[i], c);
    }
}

void Panel(Rectangle r, Color fill, Color stroke, float ch, float th) {
    Rectangle s = Snap(r);
    if (s.width < 2 || s.height < 2) return;

    float t = ThickPx(th);
    bool hasStroke = stroke.a > 0 && th > 0 && s.width > t * 2 + 1 && s.height > t * 2 + 1;

    Vector2 outer[8];
    ChamferPts(s, ch, outer);

    if (fill.a > 0) DrawTriangleFan(outer, 8, fill);
    if (!hasStroke) return;

    Rectangle ri{s.x + t, s.y + t, s.width - t * 2, s.height - t * 2};
    Vector2 inner[8];
    ChamferPts(ri, ch, inner);
    Ring(outer, inner, stroke);
}

void PanelOutline(Rectangle r, Color stroke, float ch, float th) { Panel(r, BLANK, stroke, ch, th); }

void Frame(Rectangle r, Color c, float ch) {
    Rectangle s = Snap(r);
    const float L = 26.0f, t = ThickPx(3.0f);
    float x = s.x, y = s.y, w = s.width, h = s.height;
    // 四个角的 L 形装饰：贴着切角内侧，端点同样对齐像素网格
    auto bar = [&](float bx, float by, float bw, float bh) { Rect({bx, by, bw, bh}, c); };
    bar(x + ch, y, L, t);
    bar(x, y + ch, t, L);
    bar(x + w - ch - L, y, L, t);
    bar(x + w - t, y + ch, t, L);
    bar(x + ch, y + h - t, L, t);
    bar(x, y + h - ch - L, t, L);
    bar(x + w - ch - L, y + h - t, L, t);
    bar(x + w - t, y + h - ch - L, t, L);
}

void Rect(Rectangle r, Color c) { DrawRectangleRec(Snap(r), c); }

// ---------------------------------------------------------------- 形状
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
    float gap = Px(3.0f);
    float sw = Px((w - gap * (seg - 1)) / seg);
    int lit = (int)std::round(clampf(ratio, 0, 1) * seg);
    for (int i = 0; i < seg; i++) {
        Color c = (i < lit) ? fill : bg;
        Rect({x + i * (sw + gap), y, sw, h}, c);
    }
}

void Dim(float alpha, Color tint) { DrawRectangle(0, 0, (int)VW, (int)VH, WithAlpha(tint, alpha)); }

bool Hover(Rectangle r) {
    const Input& in = app().in;
    if (!in.mouseActive && !in.lmbPressed) return false;
    Vector2 m = in.mouse;
    return m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height;
}

float Pulse(float speed, float phase) {
    return 0.5f + 0.5f * std::sin((float)GetTime() * speed * 2.0f * PI + phase);
}

bool Button(Rectangle r, const std::string& label, bool focused, bool enabled, int fontSize, Color accent) {
    bool hovered = enabled && Hover(r);
    bool click = false;
    if (hovered && app().in.lmbPressed) click = true;

    // 只做整像素位移：0 / -2 都是整像素，描边不会因为亚像素而忽粗忽细。
    // 按下时不再上下跳（几何一动，2px 描边就会闪一下），改成填充提亮。
    float lift = focused ? -Px(2.0f) : 0.0f;
    Rectangle rr = Snap({r.x, r.y + lift, r.width, r.height});

    bool pressed = hovered && app().in.lmb;
    Color fill = enabled ? (focused ? WithAlpha(accent, 0.18f) : WithAlpha(pal::kPanelBg, 0.9f))
                         : WithAlpha(Color{20, 26, 34, 255}, 0.85f);
    if (pressed) fill = Mix(fill, WithAlpha(accent, fill.a), 0.45f);
    Color stroke = enabled ? (focused ? accent : WithAlpha(accent, 0.45f)) : Color{60, 70, 82, 255};
    Panel(rr, fill, stroke, 8.0f, focused ? 2.5f : 2.0f);
    if (focused) {
        Triangle(rr.x - 26, rr.y + rr.height * 0.5f, 16, 1, accent);
        Triangle(rr.x + rr.width + 26, rr.y + rr.height * 0.5f, 16, -1, accent);
    }
    Color tc = enabled ? (focused ? pal::kText : pal::kDim) : Color{70, 80, 92, 255};
    TextC(label, rr.x + rr.width * 0.5f, rr.y + (rr.height - fontSize) * 0.5f - 1, fontSize, tc, focused);
    return click;
}

}  // namespace sky::ui
