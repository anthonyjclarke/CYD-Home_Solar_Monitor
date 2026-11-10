#pragma once
#include "flow_layout.h"
#include "history_model.h"

namespace ui {
struct HistoryView {
    const history::Store& store;
    uint32_t epoch = 0;
    history::Field field = history::Solar;
    Layout layout = Centre;
    Panel panel = Panel320;
    bool storageOK = false, demo = false;
};
void drawHistory(Canvas& canvas, const HistoryView& view);
}  // namespace ui
