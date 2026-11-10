#include "panel_state.h"
#include "touch_gesture.h"
#include "wifi_lifecycle.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
    using ui::Screen;
    ui::PanelState panel;
    assert(panel.screen() == Screen::Boot);
    panel.finishCalibration(); assert(panel.screen() == Screen::Boot);
    panel.tap(319, 239); assert(panel.screen() == Screen::Flow); // Skip boot only.
    panel.tap(231, 209); assert(panel.screen() == Screen::Flow);
    panel.tap(232, 209); assert(panel.screen() == Screen::History);
    panel.tap(260, 29); assert(panel.screen() == Screen::History);
    panel.tap(259, 28); assert(panel.screen() == Screen::History);
    panel.tap(260, 28); assert(panel.screen() == Screen::Flow);
    panel.hold(); assert(panel.screen() == Screen::Settings);
    panel.finishBoot(); assert(panel.screen() == Screen::Settings);
    panel.hold(); assert(panel.screen() == Screen::Calibration);
    panel.tap(0, 0); panel.hold(); assert(panel.screen() == Screen::Calibration);
    panel.finishCalibration(); assert(panel.screen() == Screen::Settings);
    panel.tap(319, 239); assert(panel.screen() == Screen::Flow); // Return only.
    panel.tap(319, 239); panel.hold(); assert(panel.screen() == Screen::Settings);
    ui::PanelState boot;
    boot.hold(); assert(boot.screen() == Screen::Settings);
    boot.finishBoot(); assert(boot.screen() == Screen::Settings);
    ui::PanelState timedBoot;
    timedBoot.finishBoot(); assert(timedBoot.screen() == Screen::Flow);

    // 4.0-inch native 480 x 320 targets: History in the rail, Home top right.
    ui::PanelState wide(ui::targetsFor(480));
    wide.finishBoot();
    wide.tap(232, 209); assert(wide.screen() == Screen::Flow);  // 320 target is not a 480 one.
    wide.tap(335, 300); assert(wide.screen() == Screen::Flow);
    wide.tap(336, 260); assert(wide.screen() == Screen::History);
    wide.tap(379, 10); assert(wide.screen() == Screen::History);
    wide.tap(380, 36); assert(wide.screen() == Screen::History);
    wide.tap(479, 35); assert(wide.screen() == Screen::Flow);
    wide.tap(479, 319); assert(wide.screen() == Screen::History);
    static_assert(ui::targets320.tab(0) == 0 && ui::targets320.tab(63) == 0 && ui::targets320.tab(64) == 1 &&
                  ui::targets320.tab(319) == 4, "320 tabs are 64 px");
    static_assert(ui::targets480.tab(95) == 0 && ui::targets480.tab(96) == 1 && ui::targets480.tab(479) == 4,
                  "480 tabs are 96 px");
    static_assert(ui::targets480.prev.contains(0, 319) && ui::targets480.next.contains(479, 319) &&
                  !ui::targets480.prev.contains(88, 300) && ui::targets480.tabs.contains(200, 50),
                  "480 history buttons reach the panel edges");
    static_assert(&ui::targetsFor(320) == &ui::targets320, "320 x 240 boards keep their targets");

    // A wake contact and a settings hold cannot leak taps into the next screen.
    ui::TouchGesture gesture;
    assert(gesture.update(true, 100, true) == ui::Gesture::None);
    assert(gesture.update(true, 2300) == ui::Gesture::None);
    assert(gesture.update(false, 2400) == ui::Gesture::None);
    gesture.update(true, 2500);
    assert(gesture.update(true, 4501) == ui::Gesture::Hold);
    panel.hold(); assert(panel.screen() == Screen::Calibration);
    assert(gesture.update(false, 4600) == ui::Gesture::None);

    using namespace cyd::network;
    Lifecycle wifi;
    const uint32_t starts[] = {0, std::numeric_limits<uint32_t>::max()-10000};
    for (uint32_t start : starts) {
        wifi.begin(start);
        assert(wifi.update(start+19999, false, false) == Action::None);
        assert(wifi.update(start+20000, false, false) == Action::OpenPortal);
        assert(wifi.phase() == Phase::Portal);
        for (uint32_t t=20001; t<200000; t+=1000)
            assert(wifi.update(start+t, false, true) == Action::None);
        // The old shared timer reset at portal closure and starved its reopen.
        assert(wifi.update(start+200000, false, false) == Action::Reconnect);
        assert(wifi.phase() == Phase::RetryWait);
        assert(wifi.update(start+219999, false, false) == Action::None);
        assert(wifi.update(start+220000, false, false) == Action::OpenPortal);
        assert(wifi.update(start+220001, true, true) == Action::ClosePortal);
        assert(wifi.phase() == Phase::Online);
        assert(wifi.update(start+220002, true, false) == Action::None);
        // Whether portal or USB supplied credentials, an outage never opens AP.
        for (uint32_t t=220003; t<600000; t+=1000) {
            const auto action=wifi.update(start+t, false, false);
            assert(action == Action::None || action == Action::Reconnect);
            assert(wifi.phase() == Phase::Reconnecting);
        }
        assert(wifi.update(start+600000, true, false) == Action::None);
        assert(wifi.phase() == Phase::Online);
    }
    // Delayed servicing used to run reconnect first on every pass, resetting
    // the shared timestamp before the portal retry condition could see it.
    wifi.begin(0);
    assert(wifi.update(20000, false, false) == Action::OpenPortal);
    assert(wifi.update(200001, false, false) == Action::Reconnect);
    assert(wifi.update(230001, false, false) == Action::OpenPortal);
    wifi.begin(0);
    assert(wifi.update(1, true, false) == Action::None); // Saved/USB network before AP.
    assert(wifi.update(30000, false, false) == Action::Reconnect);
    assert(wifi.update(60000, false, false) == Action::Reconnect);
    assert(wifi.update(60001, false, false) == Action::None);
    wifi.begin(0);
    assert(wifi.update(20000, false, false) == Action::OpenPortal);
    assert(wifi.update(20001, false, false) == Action::None); // Failed AP start.
    assert(wifi.update(40000, false, false) == Action::None);
    assert(wifi.update(40001, false, false) == Action::OpenPortal);
    std::puts("Panel and Wi-Fi lifecycles: navigation, consumed gestures, portal retry, outages, recovery and rollover passed.");
}
