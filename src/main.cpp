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

// Apply the configured POSIX timezone whenever it changes (boot or web save).
// configTzTime() below only starts SNTP once; TZ itself can change at any time.
void applyTimezone() {
    static String applied;
    const auto& c=monitor::config();
    if (c.tzPosix==applied) return;
    setenv("TZ",c.tzPosix.c_str(),1); tzset(); applied=c.tzPosix;
    DBG_INFO("[CLOCK] timezone=%s posix=%s",c.tzName.c_str(),c.tzPosix.c_str());
}

void setup() {
    Serial.begin(115200); improvBegin();
    monitor::loadConfig(); cyd::storage::begin(); applyTimezone();
    // Keep the active-low LED off rather than run the scaffold demo cycle.
    for (int pin : {LED_R_PIN,LED_G_PIN,LED_B_PIN}) { pinMode(pin,OUTPUT); digitalWrite(pin,HIGH); }
    cyd::backlight::begin(); cyd::display::begin();
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
        configTzTime(monitor::config().tzPosix.c_str(),APP_NTP_SERVER_1,APP_NTP_SERVER_2);
        DBG_INFO("[CLOCK] NTP started; timezone=%s",monitor::config().tzName.c_str());
        timeStarted=true;
    }
    applyTimezone();
    cyd::ota::update(); cyd::touch::update(); web::update(); app::update(millis());
    diagnostics::update(millis());
    vTaskDelay(1);  // Yield one tick to WiFi/idle tasks; not a timing delay.
}
