#pragma once
#include "energy_model.h"

namespace ui {
enum Align { Left, Center, Right };
// Cards: original dark four-card grid. Centre: light cross layout with the
// four flows meeting at a central hub.
enum Layout : unsigned char { Cards, Centre, LayoutCount };
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void rect(int x, int y, int w, int h, unsigned color, int radius = 0) = 0;
    virtual void line(int x0, int y0, int x1, int y1, unsigned color) = 0;
    virtual void text(int x, int y, int font, unsigned color, Align align, const char* value) = 0;
    virtual int textWidth(int font, const char* value) = 0;
};
struct FlowView {
    energy::Snapshot data;
    bool usable[energy::Count] = {};
    bool wifi = false;
    bool clockKnown = false;
    bool totalLive[energy::TotalCount] = {};
    bool weatherLive = false;
    unsigned phase = 0;
    Layout layout = Centre;
    const char* status = "Waiting for readings";
    const char* address = "";
    const char* batteryLabel = "";
    const char* time = "";
};
void drawFlow(Canvas& canvas, const FlowView& view);

// Boot screen: identity, then one row per start-up step so the IP address and
// Wi-Fi state can be read before the flow display takes over.
enum BootState : unsigned char { BootInfo, BootOk, BootWaiting, BootProblem };
struct BootRow {
    const char* label = "";
    const char* value = "";
    BootState state = BootInfo;
};
struct BootView {
    static constexpr int rowCount = 6;
    BootRow rows[rowCount];
    const char* version = "";
    const char* footer = "";
    float progress = 0;  // 0..1 across the boot screen's time on screen.
    Layout layout = Centre;  // Matches the theme of the chosen panel layout.
};
void drawBoot(Canvas& canvas, const BootView& view);
}  // namespace ui
