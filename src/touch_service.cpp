#include "touch_service.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

namespace cyd::touch {
namespace {
SPIClass bus(VSPI);
XPT2046_Touchscreen controller(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
State current = {};
uint16_t width = 320, height = 240;
uint16_t limits[4] = {TOUCH_X_MIN, TOUCH_X_MAX, TOUCH_Y_MIN, TOUCH_Y_MAX};
}
void begin(uint16_t w, uint16_t h) {
    width = w; height = h;
    Preferences p;
    if (p.begin("solar-touch", true)) {
        if (p.getBytesLength("limits") == sizeof(limits)) p.getBytes("limits", limits, sizeof(limits));
        p.end();
    }
    bus.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
    controller.begin(bus); controller.setRotation(1);
    current.available = true;
}
void update() {
    current.pressed = false;
    if (!controller.tirqTouched() || !controller.touched()) return;
    const TS_Point p = controller.getPoint();
    // Reject saturated/no-response pressure and rail coordinates, as in the
    // working AnimatedPixelClock, rather than creating phantom touches.
    if (p.z < 350 || p.z >= 4000 || p.x <= 0 || p.y <= 0 || p.x >= 4095 || p.y >= 4095) return;
    current.rawX = p.x; current.rawY = p.y; current.z = p.z;
    current.x = constrain(map(p.x, limits[0], limits[1], 0, width - 1), 0, width - 1);
    current.y = constrain(map(p.y, limits[2], limits[3], 0, height - 1), 0, height - 1);
    current.pressed = true;
}
const State& state() { return current; }

bool calibrate(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1) {
    if (x1 <= x0 + 1000 || y1 <= y0 + 1000 || x1 > 4095 || y1 > 4095) return false;
    uint16_t next[] = {x0, x1, y0, y1};
    Preferences p;
    if (!p.begin("solar-touch", false)) return false;
    bool ok = p.putBytes("limits", next, sizeof(next)) == sizeof(next); p.end();
    if (ok) memcpy(limits, next, sizeof(limits));
    return ok;
}
}  // namespace cyd::touch

