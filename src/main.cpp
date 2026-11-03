#include <Arduino.h>
#include <WiFi.h>
#include "app/app.h"
#include "backlight_service.h"
#include "debug.h"
#include "display_service.h"
#include "monitor_config.h"
#include "monitor_web.h"
#include "network/improv_setup.h"
#include "ota_service.h"
#include "storage_service.h"
#include "touch_service.h"
#include "wifi_service.h"
#include "serial_diagnostics.h"

uint8_t debugLevel=APP_DEFAULT_DEBUG_LEVEL;

void setup() {
    Serial.begin(115200); improvBegin();
    monitor::loadConfig(); cyd::storage::begin();
    // Keep the active-low LED off rather than run the scaffold demo cycle.
    for (int pin : {LED_R_PIN,LED_G_PIN,LED_B_PIN}) { pinMode(pin,OUTPUT); digitalWrite(pin,HIGH); }
    cyd::backlight::begin(); cyd::display::begin();
    cyd::display::showBootMessage("HOME ENERGY", "Starting local services...");
    cyd::touch::begin(320,240); cyd::network::begin();
    cyd::ota::begin(cyd::storage::settings().deviceName);
    app::begin();
    diagnostics::begin();
    DBG_INFO("%s %s ready", PROJECT_NAME,FIRMWARE_VERSION);
}
void loop() {
    static bool timeStarted=false;
    improvTick(); cyd::network::update();
    if (!timeStarted && WiFi.isConnected()) {
        configTzTime(APP_TZ_POSIX,APP_NTP_SERVER_1,APP_NTP_SERVER_2);
        DBG_INFO("[CLOCK] NTP started; timezone=%s",APP_TZ_NAME);
        timeStarted=true;
    }
    cyd::ota::update(); cyd::touch::update(); web::update(); app::update(millis());
    diagnostics::update(millis());
    vTaskDelay(1);  // Yield one tick to WiFi/idle tasks; not a timing delay.
}
