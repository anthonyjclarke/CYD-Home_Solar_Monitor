#pragma once
#include <cstdint>

namespace ui {
enum class Screen : uint8_t { Boot, Flow, History, Settings, Calibration };
// One active screen. Backlight, gesture consumption and rendering cadence are
// independent concerns; none of them may change navigation implicitly.
class PanelState {
    Screen screen_ = Screen::Boot;
public:
    Screen screen() const { return screen_; }
    void tap(uint16_t x, uint16_t y) {
        switch (screen_) {
            case Screen::Boot:
            case Screen::Settings: screen_ = Screen::Flow; break;
            case Screen::Flow:
                if (y >= 209 && x >= 232) screen_ = Screen::History;
                break;
            case Screen::History:
                if (y < 29 && x >= 260) screen_ = Screen::Flow;
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
