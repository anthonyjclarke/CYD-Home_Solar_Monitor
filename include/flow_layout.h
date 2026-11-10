#pragma once
#include "energy_model.h"
#include "boot_status.h"

namespace ui {
enum Align { Left, Center, Right };
// Cards: original dark four-card grid. Centre: light cross layout with the
// four flows meeting at a central hub.
enum Layout : unsigned char { Cards, Centre, LayoutCount };
// Frame each screen is drawn for: the 320 x 240 every board but the 4.0-inch
// uses, or the 4.0-inch's native 480 x 320 (same built-in fonts, more room).
enum Panel : unsigned char { Panel320, Panel480 };
constexpr int panelWidth(Panel p) { return p == Panel480 ? 480 : 320; }
constexpr int panelHeight(Panel p) { return p == Panel480 ? 320 : 240; }
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
    // False when a displayed reading has no Home Assistant report time (or the
    // clock is unset). The centre layout keeps this warning even when it would
    // otherwise show "All readings live" or the spare-power hint (audit F8).
    bool reportKnown = true;
    unsigned phase = 0;
    uint32_t ms = 0;  // Frame time; the centre layout's comets advance with it.
    bool motionOnly = false;  // Redraw only the moving pipes and hub (centre layout).
    Layout layout = Centre;
    Panel panel = Panel320;
    const char* status = "Waiting for readings";
    const char* address = "";
    const char* batteryLabel = "";
    const char* time = "";
};
struct Box { int x = 0, y = 0, w = 0, h = 0; };
// Returns the area holding the animated pipes (empty for the cards layout),
// which is all a motionOnly frame changes. It never exceeds 160 px in height,
// so a 480 x 320 motion frame fits one band.
Box drawFlow(Canvas& canvas, const FlowView& view);

// Boot screen: identity, then one row per start-up step so the IP address and
// Wi-Fi state can be read before the flow display takes over.
using BootState = boot::State;
constexpr BootState BootInfo=boot::Info, BootOk=boot::Ok, BootWaiting=boot::Waiting, BootProblem=boot::Problem;
struct BootRow {
    const char* label = "";
    const char* value = "";
    BootState state = BootInfo;
};
struct BootView {
    static constexpr int rowCount = boot::RowCount;
    BootRow rows[rowCount];
    const char* version = "";
    const char* footer = "";
    float progress = 0;  // 0..1 across the boot screen's time on screen.
    Layout layout = Centre;  // Matches the theme of the chosen panel layout.
    Panel panel = Panel320;
};
void drawBoot(Canvas& canvas, const BootView& view);

// Device / setup screen opened by a 2-second hold: how to reach the web page
// (or the Wi-Fi portal) and the access code. Always dark.
struct SettingsView {
    bool connected = false;
    const char* webUrl = "";      // http://<mdns>.local/
    const char* ip = "";
    const char* apName = "";
    const char* accessCode = "";
    const char* haUrl = "";
    const char* version = "";     // Without a leading v.
    Panel panel = Panel320;
};
void drawSettings(Canvas& canvas, const SettingsView& view);
}  // namespace ui
