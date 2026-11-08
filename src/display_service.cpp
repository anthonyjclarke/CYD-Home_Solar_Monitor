#include "display_service.h"
#include "board_profile.h"

namespace cyd::display {
namespace { TFT_eSPI screen; }
void begin() {
    screen.init(); screen.setRotation(boardProfile().rotation);
    screen.fillScreen(TFT_BLACK);
}
TFT_eSPI& tft() { return screen; }
int16_t originX() { return (screen.width() - CYD_EXPECTED_WIDTH) / 2; }
int16_t originY() { return (screen.height() - CYD_EXPECTED_HEIGHT) / 2; }
void showBootMessage(const String& title, const String& detail) {
    screen.fillScreen(TFT_BLACK);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.drawString(title, 12, 24, 4);
    screen.drawString(detail, 12, 65, 2);
}
void showWifiPortalMessage(const String& apName) { showBootMessage("WiFi setup", apName); }
}  // namespace cyd::display

