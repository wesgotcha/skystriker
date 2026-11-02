#pragma once
// 双人模式配置：屏幕之间传递"这一局怎么打"
#include <cstdint>

namespace sky {

enum class MatchMode {
    Solo = 0,        // 单人闯关
    CoopLocal = 1,   // 同屏组队
    VersusLocal = 2, // 同屏对战
    CoopLan = 3,     // 联机组队
    VersusLan = 4    // 联机对战
};

struct MatchConfig {
    MatchMode mode = MatchMode::Solo;
    int       craft[2] = {0, 0};   // 默认两人都用解锁的 0 号机
    int       skin[2] = {0, 2};   // 1P 蓝、2P 橙，方便区分
    uint32_t  seed = 0x5EEDu;
    bool      online = false;

    int  players() const { return mode == MatchMode::Solo ? 1 : 2; }
    bool versus() const { return mode == MatchMode::VersusLocal || mode == MatchMode::VersusLan; }
    bool lan() const { return mode == MatchMode::CoopLan || mode == MatchMode::VersusLan; }
};

namespace match {

inline MatchConfig& cfg() {
    static MatchConfig c;
    return c;
}

inline const char* modeName(MatchMode m) {
    switch (m) {
        case MatchMode::CoopLocal:
        case MatchMode::CoopLan: return "合作";
        case MatchMode::VersusLocal:
        case MatchMode::VersusLan: return "对战";
        default: return "单人";
    }
}

inline void setMode(MatchMode m) {
    cfg().mode = m;
    cfg().online = (m == MatchMode::CoopLan || m == MatchMode::VersusLan);
}

}  // namespace match
}  // namespace sky
