#include "touch_service.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <algorithm>
#include <cstdlib>
#if defined(TOUCH_CS)
#include "display_service.h"
#else
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#endif

namespace cyd::touch {
namespace {
#if defined(TOUCH_CS)
// Shared-bus boards (4.0-inch): the XPT2046 sits on the display's SPI lines,
// so only TFT_eSPI may drive it; it drops to SPI_TOUCH_FREQUENCY per read.
// Same raw-axis convention as CYD_AnimatedPixelClock's 4.0-inch backend.
bool sample(uint16_t& x, uint16_t& y, uint16_t& z) {
    z = cyd::display::tft().getTouchRawZ();
    if (z < 350) return false;
    cyd::display::tft().getTouchRaw(&x, &y);
    return true;
}
#else
SPIClass bus(VSPI);
XPT2046_Touchscreen controller(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
bool sample(uint16_t& x, uint16_t& y, uint16_t& z) {
    if (!controller.tirqTouched() || !controller.touched()) return false;
    const TS_Point p = controller.getPoint();
    x = p.x; y = p.y; z = p.z;
    return true;
}
#endif
State current = {};
uint16_t width = 320, height = 240;
uint16_t limits[4] = {TOUCH_X_MIN, TOUCH_X_MAX, TOUCH_Y_MIN, TOUCH_Y_MAX};
// Some boards' touch film runs its X/Y across the panel's landscape axes.
bool swapXY = TOUCH_SWAP_XY != 0;
// The same spans calibrate() requires; equal endpoints would divide by zero
// in map(). A reversed pair is a mirrored axis, which map() handles, so a
// board whose touch film runs the other way is fixed by calibrating.
bool validLimits(const uint16_t* l) {
    return std::abs(int(l[1]) - int(l[0])) > 1000 && std::abs(int(l[3]) - int(l[2])) > 1000 &&
           l[0] <= 4095 && l[1] <= 4095 && l[2] <= 4095 && l[3] <= 4095;
}
}
void begin(uint16_t w, uint16_t h) {
    width = w; height = h;
    Preferences p;
    if (p.begin("solar-touch", true)) {
        uint16_t saved[4];
        // A corrupt or out-of-range record keeps the built-in defaults. A
        // record without the swap key predates it and was made unswapped:
        // valid as-is on unswapped boards, meaningless on swapped ones. Limits
        // map raw readings onto the UI frame they were sampled on; a record
        // without the frame key was made on a 320 x 240 frame (the 4.0-inch
        // before its native 480 x 320 layout) and is ignored on other frames.
        const bool hasSwap = p.isKey("swap");
        const bool sameFrame = p.isKey("frame") ? p.getUShort("frame", 0) == w : w == 320;
        if (p.getBytesLength("limits") == sizeof(saved) && p.getBytes("limits", saved, sizeof(saved)) == sizeof(saved) &&
            validLimits(saved) && (hasSwap || !TOUCH_SWAP_XY) && sameFrame) {
            memcpy(limits, saved, sizeof(limits));
            if (hasSwap) swapXY = p.getBool("swap", swapXY);
        }
        p.end();
    }
#if !defined(TOUCH_CS)
    // TFT_eSPI starts the shared-bus controller inside tft.init().
    bus.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
    controller.begin(bus); controller.setRotation(1);
#endif
    current.available = true;
}
void update() {
    current.pressed = false;
    uint16_t x = 0, y = 0, z = 0;
    if (!sample(x, y, z)) return;
    // Reject saturated/no-response pressure and rail coordinates, as in the
    // working AnimatedPixelClock, rather than creating phantom touches.
    if (z < 350 || z >= 4000 || x == 0 || y == 0 || x >= 4095 || y >= 4095) return;
    if (swapXY) std::swap(x, y);
    current.rawX = x; current.rawY = y; current.z = z;
    current.x = constrain(map(x, limits[0], limits[1], 0, width - 1), 0, width - 1);
    current.y = constrain(map(y, limits[2], limits[3], 0, height - 1), 0, height - 1);
    current.pressed = true;
}
const State& state() { return current; }

bool swapped() { return swapXY; }

bool calibrate(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1, bool swap) {
    uint16_t next[] = {x0, x1, y0, y1};
    if (!validLimits(next)) return false;
    Preferences p;
    if (!p.begin("solar-touch", false)) return false;
    bool ok = p.putBytes("limits", next, sizeof(next)) == sizeof(next) && p.putBool("swap", swap) &&
              p.putUShort("frame", width) == sizeof(uint16_t);
    p.end();
    if (ok) { memcpy(limits, next, sizeof(limits)); swapXY = swap; }
    return ok;
}
}  // namespace cyd::touch

