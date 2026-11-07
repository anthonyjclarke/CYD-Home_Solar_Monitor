// Audit F8: the visible status keeps "source age unknown" in both layouts,
// including the centre layout's spare-power case, and drops it once known.
#include "flow_layout.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

struct Recorder : ui::Canvas {
    std::vector<std::string> texts;
    void rect(int, int, int, int, unsigned, int) override {}
    void line(int, int, int, int, unsigned) override {}
    void text(int, int, int, unsigned, ui::Align, const char* value) override { texts.push_back(value); }
    int textWidth(int, const char* value) override { return int(std::string(value).size()) * 6; }
    bool shows(const std::string& s) const { for (const auto& t : texts) if (t == s) return true; return false; }
};

// scene 1 exports (spare power); scene 0 does not. Status strings mirror app.cpp.
static Recorder draw(ui::Layout layout, unsigned scene, bool live, bool known) {
    ui::FlowView v;
    v.layout = layout; v.data = energy::demoSnapshot(1000, scene); v.data.demo = false; v.wifi = true;
    for (bool& b : v.usable) b = live;
    v.reportKnown = known;
    v.status = !live ? "Waiting / stale or unavailable" : known ? "Live / hold for settings" : "HA connected / source age unknown";
    Recorder r; ui::drawFlow(r, v); return r;
}

int main() {
    const bool exporting = energy::demoSnapshot(1000, 1).readings[energy::Grid].value < -100;
    assert(exporting && energy::demoSnapshot(1000, 0).readings[energy::Grid].value >= -100);
    // Centre layout.
    assert(draw(ui::Centre, 0, true, true).shows("All readings live"));
    assert(draw(ui::Centre, 0, true, false).shows("Live / source age unknown"));
    assert(draw(ui::Centre, 1, true, true).shows("Good time to use power"));
    assert(draw(ui::Centre, 1, true, false).shows("Spare power / age unknown"));
    assert(!draw(ui::Centre, 1, true, false).shows("Good time to use power"));
    assert(draw(ui::Centre, 1, false, false).shows("Waiting / stale or unavailable"));
    // Cards layout shows the app status directly.
    assert(draw(ui::Cards, 1, true, false).shows("HA connected / source age unknown"));
    assert(draw(ui::Cards, 1, true, true).shows("Live / hold for settings"));
    assert(draw(ui::Cards, 0, false, true).shows("Waiting / stale or unavailable"));
    std::cout << "Flow status: verified, unknown-age and stale scenes in both layouts, including spare power, passed.\n";
}
