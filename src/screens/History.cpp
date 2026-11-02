// HISTORY 作战复盘列表：replays/ 里的每一局，排序 + 一键重看
#include "Screens.hpp"

#include "App.hpp"
#include "Audio.hpp"
#include "Ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sky {

namespace {

Rectangle PanelRect() { return {370, 116, 1180, 780}; }

constexpr float kRowTop = 150;  // 相对面板顶（删掉排序页签后上移，填满顶部留白）
constexpr float kRowH = 76;
constexpr int   kRows = 6;

Rectangle RowRect(int i) {
    Rectangle r = PanelRect();
    return {r.x + 32, r.y + kRowTop + i * kRowH - 12, r.width - 64, kRowH - 12};
}
// 「复盘」按钮（重看那一局），靠右
Rectangle RowBtnRect(int i) {
    Rectangle rr = RowRect(i);
    return {rr.x + rr.width - 150 - 12, rr.y + 9, 150, 46};
}
// 「删除」按钮，在「复盘」按钮左侧
Rectangle RowDelBtnRect(int i) {
    Rectangle rr = RowRect(i);
    return {rr.x + rr.width - 150 - 12 - 64 - 12, rr.y + 9, 64, 46};
}
// 底部「清空全部」按钮（二次确认后才会真删）
Rectangle ClearBtnRect() {
    Rectangle r = PanelRect();
    return {r.x + 44, r.y + r.height - 78, 150, 46};
}

}  // namespace

void HistoryOverlay::enter() {
    t_ = 0;
    sort_ = 0;
    sel_ = 0;
    top_ = 0;
    items_ = replay::List();
    sortItems();
}

void HistoryOverlay::sortItems() {
    std::sort(items_.begin(), items_.end(), [this](const replay::Meta& a, const replay::Meta& b) {
        if (sort_ == 1) {
            if (a.score != b.score) return a.score > b.score;
        } else if (sort_ == 2) {
            if (a.time != b.time) return a.time > b.time;
        }
        return a.stamp() > b.stamp();
    });
    if (sel_ >= (int)items_.size()) sel_ = std::max(0, (int)items_.size() - 1);
}

void HistoryOverlay::play(int i) {
    if (i < 0 || i >= (int)items_.size()) return;
    audio::play(audio::SfxUiSelect, 0.8f);
    replay::SetPending(items_[i].path);
    app().popOverlay();
    app().go(Sid::Replay);
}

void HistoryOverlay::refresh() {
    items_ = replay::List();
    sortItems();
    int n = (int)items_.size();
    if (top_ > n - kRows) top_ = std::max(0, n - kRows);
    if (top_ < 0) top_ = 0;
}

void HistoryOverlay::confirmDelete(int idx) {
    if (idx < 0 || idx >= (int)items_.size()) return;
    audio::play(audio::SfxUiSelect, 0.8f);
    std::string when = items_[idx].when();
    app().pushOverlay(std::make_unique<ConfirmOverlay>(
        "删除记录", "删除这条记录（" + when + "）?",
        [this, idx]() {
            if (idx >= 0 && idx < (int)items_.size()) {
                replay::Delete(items_[idx].path);
                refresh();
            }
        },
        "删除", "取消"));
}

void HistoryOverlay::confirmClear() {
    if (items_.empty()) return;
    audio::play(audio::SfxUiSelect, 0.8f);
    app().pushOverlay(std::make_unique<ConfirmOverlay>(
        "清空历史", "确定要清空全部战斗记录? 此操作不可恢复。",
        [this]() {
            replay::ClearAll();
            refresh();
        },
        "清空", "取消"));
}

void HistoryOverlay::update(float dt) {
    t_ += dt;
    auto& in = app().in;
    const int n = (int)items_.size();
    bool selMoved = false;

    if (in.back()) {
        app().popOverlay();
        return;
    }
    if (n > 0) {
        int prev = sel_;
        if (in.navUp()) sel_ = (sel_ + n - 1) % n;
        if (in.navDown()) sel_ = (sel_ + 1) % n;
        if (sel_ != prev) {
            audio::play(audio::SfxUiMove, 0.4f);
            selMoved = true;
        }
        if (in.ok()) {
            play(sel_);
            return;
        }
        if (in.pressed(KEY_DELETE)) {
            confirmDelete(sel_);
            return;
        }
    }

    // ---- 鼠标
    for (int i = 0; i < kRows && top_ + i < n; i++) {
        int idx = top_ + i;
        if (ui::Hover(RowRect(i)) && (in.mouseMoved || in.lmbPressed) && sel_ != idx) {
            sel_ = idx;
            selMoved = true;
        }
        if (!in.lmbPressed) continue;
        if (ui::Hover(RowBtnRect(i))) {
            play(idx);
            return;
        }
        if (ui::Hover(RowDelBtnRect(i))) {
            confirmDelete(idx);
            return;
        }
        if (ui::Hover(RowRect(i))) {
            sel_ = idx;
            audio::play(audio::SfxUiMove, 0.4f);
        }
    }

    // 底部「清空全部」按钮
    if (in.lmbPressed && ui::Hover(ClearBtnRect())) {
        confirmClear();
        return;
    }

    // 选中项保证在可视范围内：仅当选择真的变了才拉回，否则会把滚轮滚走的位置拽回来
    if (selMoved) {
        if (sel_ < top_) top_ = sel_;
        if (sel_ >= top_ + kRows) top_ = sel_ - kRows + 1;
    }
    if (top_ < 0) top_ = 0;
    if (n > kRows && top_ > n - kRows) top_ = n - kRows;

    // 鼠标滚轮滚动列表：放在最后，只做边界夹紧，不强行把选择拉回可视区
    if (n > kRows && in.wheel != 0.0f) {
        top_ -= (int)std::round(in.wheel);
        if (top_ < 0) top_ = 0;
        if (n > kRows && top_ > n - kRows) top_ = n - kRows;
    }
}

void HistoryOverlay::draw() {
    ui::Dim(0.68f, Color{4, 8, 14, 255});
    Rectangle r = PanelRect();
    ui::Panel(r, Color{6, 12, 22, 255}, WithAlpha(pal::kCyan, 0.6f), 14, 2);
    ui::Frame(r, WithAlpha(pal::kCyan, 0.3f), 14);

    ui::TextC("历 史 战 绩", VW * 0.5f, r.y + 24, 46, pal::kText, true);

    // 列头（红圈已删：副标题 / 排序页签 / 计数；蓝圈列头文字改为居中）
    float hy = r.y + 96;
    const float cx = r.x;
    ui::Text("时间", cx + 106, hy, 20, WithAlpha(pal::kDim, 0.7f));
    ui::TextC("状态", cx + 442, hy, 20, WithAlpha(pal::kDim, 0.7f));
    ui::TextC("得分", cx + 644, hy, 20, WithAlpha(pal::kDim, 0.7f));
    ui::TextC("击毁", cx + 780, hy, 20, WithAlpha(pal::kDim, 0.7f));
    ui::TextC("时长", cx + 849, hy, 20, WithAlpha(pal::kDim, 0.7f));
    ui::Rect({cx + 44, hy + 34, r.width - 88, 1}, WithAlpha(pal::kCyan, 0.25f));

    if (items_.empty()) {
        ui::TextC("暂 无 复 盘", VW * 0.5f, r.y + 380, 34, WithAlpha(pal::kDim, 0.75f), true);
        ui::TextC("出击一次，打完就会自动存下", VW * 0.5f, r.y + 434, 22, WithAlpha(pal::kDim, 0.55f));
    }

    for (int i = 0; i < kRows && top_ + i < (int)items_.size(); i++) {
        int idx = top_ + i;
        const replay::Meta& m = items_[idx];
        Rectangle rr = RowRect(i);
        bool sel = (idx == sel_);
        float y = rr.y + 12;

        if (sel) {
            ui::Rect(rr, WithAlpha(pal::kCyan, 0.09f));
            ui::Rect({rr.x, rr.y, 4, rr.height}, pal::kCyan);
        }

        char buf[64];
        std::snprintf(buf, sizeof buf, "%02d", idx + 1);
        ui::Text(buf, cx + 46, y, 24, WithAlpha(pal::kDim, sel ? 0.95f : 0.65f));

        ui::Text(m.when(), cx + 106, y, 25, sel ? pal::kText : Color{196, 210, 224, 255});
        if (m.cont) ui::TextR("续", cx + 386, y + 4, 20, WithAlpha(pal::kGold, 0.8f));

        // 状态
        Rectangle badge{cx + 398, y - 2, 88, 34};
        Color bc = m.win ? pal::kCyan : WithAlpha(pal::kDanger, 0.85f);
        ui::Panel(badge, WithAlpha(bc, 0.14f), WithAlpha(bc, 0.6f), 6, 1.5f);
        ui::TextC(m.win ? "完 成" : "失 败", badge.x + badge.width * 0.5f, badge.y + 6, 20, bc, true);

        std::snprintf(buf, sizeof buf, "%07d", m.score);
        ui::TextR(buf, cx + 700, y, 27, sel ? pal::kGold : WithAlpha(pal::kGold, 0.85f), true);
        std::snprintf(buf, sizeof buf, "%d", m.kills);
        ui::TextR(buf, cx + 800, y + 2, 24, WithAlpha(pal::kDim, 0.95f));
        ui::TextR(m.length(), cx + 878, y + 2, 24, WithAlpha(pal::kDim, 0.95f));

        // 「复盘」按钮（重看那一局）
        Rectangle rb = RowBtnRect(i);
        bool hov = ui::Hover(rb);
        ui::Panel(rb, hov ? WithAlpha(pal::kCyan, 0.26f) : WithAlpha(pal::kCyan, 0.07f),
                  hov ? pal::kCyan : WithAlpha(pal::kCyan, 0.45f), 6, hov ? 2.0f : 1.5f);
        ui::TextC("复 盘", rb.x + rb.width * 0.5f, rb.y + 10, 22, hov ? pal::kText : pal::kCyan, true);
        ui::Triangle(rb.x + 22, rb.y + rb.height * 0.5f, 9, 1, hov ? pal::kCyan : WithAlpha(pal::kCyan, 0.55f));

        // 「删除」按钮（二次确认后真删）
        Rectangle dlb = RowDelBtnRect(i);
        bool dlhov = ui::Hover(dlb);
        ui::Panel(dlb, dlhov ? WithAlpha(pal::kDanger, 0.30f) : WithAlpha(pal::kDanger, 0.10f),
                  dlhov ? pal::kDanger : WithAlpha(pal::kDanger, 0.5f), 6, dlhov ? 2.0f : 1.5f);
        ui::TextC("删除", dlb.x + dlb.width * 0.5f, dlb.y + 13, 20,
                  dlhov ? pal::kText : WithAlpha(pal::kDanger, 0.9f), true);

        ui::Rect({cx + 44, rr.y + rr.height + 5, r.width - 88, 1}, WithAlpha(WHITE, 0.06f));
    }

    // 滚动条（超过一屏时）
    int n = (int)items_.size();
    if (n > kRows) {
        float trackY = r.y + kRowTop - 8;
        float trackH = kRows * kRowH - 16;
        ui::Rect({r.x + r.width - 24, trackY, 4, trackH}, WithAlpha(WHITE, 0.08f));
        float hh = trackH * kRows / n;
        float yy = trackY + (trackH - hh) * (top_ / (float)std::max(1, n - kRows));
        ui::Rect({r.x + r.width - 24, yy, 4, hh}, WithAlpha(pal::kCyan, 0.6f));
    }

    // 底部「清空全部」按钮（有记录时才显示）
    if (!items_.empty()) {
        Rectangle cb = ClearBtnRect();
        bool cbhov = ui::Hover(cb);
        ui::Panel(cb, cbhov ? WithAlpha(pal::kDanger, 0.28f) : WithAlpha(pal::kDanger, 0.08f),
                  cbhov ? pal::kDanger : WithAlpha(pal::kDanger, 0.5f), 8, cbhov ? 2.0f : 1.5f);
        ui::TextC("清空全部", cb.x + cb.width * 0.5f, cb.y + 11, 22,
                  cbhov ? pal::kText : WithAlpha(pal::kDanger, 0.9f), true);
    }
}

}  // namespace sky
