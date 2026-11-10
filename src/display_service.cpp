#include "display_service.h"
#include "board_profile.h"

namespace cyd::display {
namespace { TFT_eSPI screen; }
void begin() {
    screen.init(); screen.setRotation(boardProfile().rotation);
    screen.fillScreen(TFT_BLACK);
}
TFT_eSPI& tft() { return screen; }

// The UI frame fills the panel: 320 x 240 boards draw 320 x 240 and the
// 4.0-inch draws its own 480 x 320 layout. Scaling a 320 x 240 frame by 4/3
// looked jagged and made small text unreadable (owner's 4.0-inch,
// 10-11-2026). Offsets stay for a panel larger than its frame.
int16_t panelX(int16_t x) { return (screen.width() - CYD_EXPECTED_WIDTH) / 2 + x; }
int16_t panelY(int16_t y) { return (screen.height() - CYD_EXPECTED_HEIGHT) / 2 + y; }
void pushFrame(TFT_eSprite& band, int16_t bandY, int16_t x, int16_t y, int16_t w, int16_t h) {
    // A whole band keeps the original single-window push.
    if (x == 0 && y == bandY && w == band.width() && h == band.height()) band.pushSprite(panelX(0), panelY(bandY));
    else band.pushSprite(panelX(x), panelY(y), x, y - bandY, w, h);
}
void showBootMessage(const String& title, const String& detail) {
    screen.fillScreen(TFT_BLACK);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.drawString(title, 12, 24, 4);
    screen.drawString(detail, 12, 65, 2);
}
void showWifiPortalMessage(const String& apName) { showBootMessage("WiFi setup", apName); }
}  // namespace cyd::display

