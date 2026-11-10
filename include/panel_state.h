#pragma once
#include <cstdint>

namespace ui {
enum class Screen : uint8_t { Boot, Flow, History, Settings, Calibration };

// Touch targets in frame coordinates. Each matches a button drawn by
// src/ui for that frame size, extended to the nearest panel edge.
struct Area {
    uint16_t x, y, w, h;
    constexpr bool contains(uint16_t px, uint16_t py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};
struct Targets {
    Area history;  // Flow screen: open History.
    Area home;     // History: back to the flow screen.
    Area tabs;     // History: five equal metric tabs.
    Area prev, next;
    // Tab under a point inside `tabs`, 0-4.
    constexpr unsigned tab(uint16_t px) const { return px < tabs.x ? 0 : (px - tabs.x) * 5u / tabs.w > 4 ? 4 : (px - tabs.x) * 5u / tabs.w; }
};
inline constexpr Targets targets320{{232, 209, 88, 31}, {260, 0, 60, 29}, {0, 31, 320, 27}, {0, 209, 52, 31}, {268, 209, 52, 31}};
inline constexpr Targets targets480{{336, 260, 144, 60}, {380, 0, 100, 36}, {0, 36, 480, 34}, {0, 268, 88, 52}, {392, 268, 88, 52}};
constexpr const Targets& targetsFor(uint16_t frameWidth) { return frameWidth >= 480 ? targets480 : targets320; }

// One active screen. Backlight, gesture consumption and rendering cadence are
// independent concerns; none of them may change navigation implicitly.
class PanelState {
    Screen screen_ = Screen::Boot;
    const Targets& targets_;
public:
    explicit PanelState(const Targets& targets = targets320) : targets_(targets) {}
    Screen screen() const { return screen_; }
    void tap(uint16_t x, uint16_t y) {
        switch (screen_) {
            case Screen::Boot:
            case Screen::Settings: screen_ = Screen::Flow; break;
            case Screen::Flow:
                if (targets_.history.contains(x, y)) screen_ = Screen::History;
                break;
            case Screen::History:
                if (targets_.home.contains(x, y)) screen_ = Screen::Flow;
                break;
            case Screen::Calibration: break;
        }
    }
    void hold() {
        if (screen_ != Screen::Calibration)
            screen_ = screen_ == Screen::Settings ? Screen::Calibration : Screen::Settings;
    }
    void finishBoot() { if (screen_ == Screen::Boot) screen_ = Screen::Flow; }
    void finishCalibration() { if (screen_ == Screen::Calibration) screen_ = Screen::Settings; }
};
}  // namespace ui
