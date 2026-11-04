#pragma once
#include <cstdint>

namespace ui {
enum class Gesture { None, Tap, Hold };
// One event per contact. A held touch cannot turn into a tap on release or
// trigger again after changing screens. Unsigned timing survives millis wrap.
class TouchGesture {
    bool down = false, consumed = false;
    uint32_t started = 0;
public:
    Gesture update(bool pressed, uint32_t ms, bool wakeOnly = false) {
        if (pressed && !down) { started = ms; consumed = wakeOnly; }
        Gesture event = Gesture::None;
        const uint32_t age = ms-started;
        if (pressed && !consumed && age > 2000) { consumed = true; event = Gesture::Hold; }
        if (!pressed && down && !consumed && age >= 50 && age <= 2000) event = Gesture::Tap;
        down = pressed;
        return event;
    }
    uint32_t heldMs(uint32_t ms) const { return down && !consumed ? ms-started : 0; }
};
}  // namespace ui
