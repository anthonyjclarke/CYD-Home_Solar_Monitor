#pragma once
#include "energy_model.h"

namespace ui {
enum Align { Left, Center, Right };
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void rect(int x, int y, int w, int h, unsigned color, int radius = 0) = 0;
    virtual void line(int x0, int y0, int x1, int y1, unsigned color) = 0;
    virtual void text(int x, int y, int font, unsigned color, Align align, const char* value) = 0;
};
struct FlowView {
    energy::Snapshot data;
    bool usable[energy::Count] = {};
    bool wifi = false;
    bool clockKnown = false;
    unsigned phase = 0;
    const char* status = "Waiting for readings";
    const char* address = "";
    const char* batteryLabel = "";
};
void drawFlow(Canvas& canvas, const FlowView& view);
}  // namespace ui

