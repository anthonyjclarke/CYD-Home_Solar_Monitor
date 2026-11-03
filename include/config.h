#pragma once

#define PROJECT_NAME "CYD-Home-Solar-Monitor"
#define FIRMWARE_VERSION "0.8.0"
#define IMPROV_SETUP_ENABLED 1
#define IMPROV_DEVICE_PREFIX "CYD-Solar"
#define APP_DEFAULT_DEBUG_LEVEL 3
#define APP_DEFAULT_BRIGHTNESS 180
#define APP_OTA_PORT 3232
// mDNS name: the device answers at http://solar.local/ (ArduinoOTA advertises it too).
#define APP_MDNS_NAME "solar"
// Boot screen: held this long after Wi-Fi connects so the IP can be read, and
// at most this long overall if Wi-Fi never connects. A tap skips it.
#define APP_BOOT_HOLD_MS 6000
#define APP_BOOT_MAX_MS 15000
// Panel redraw: the whole screen 4 times a second, and in between only the
// centre layout's pipes at 20 fps. Pushing the full sprite at 10-20 fps
// starved Wi-Fi (TCP connects took 0.4 s and downloads ran 4x slower).
#define APP_FULL_FRAME_MS 250
#define APP_FLOW_FRAME_MS 50
// TLS handshake limit in seconds. Measured on this board: about 8 s to
// Open-Meteo (ECDSA chain), so 8 s failed intermittently.
#define APP_TLS_HANDSHAKE_S 20
// Default timezone until one is chosen in the browser settings: Australia/Sydney
// as a POSIX rule (applied with setenv/tzset, not ezTime).
#define APP_TZ_POSIX "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define APP_TZ_NAME "Australia/Sydney"
#define APP_NTP_SERVER_1 "pool.ntp.org"
#define APP_NTP_SERVER_2 "time.cloudflare.com"

// Working CYD_AnimatedPixelClock 2.8-inch wiring. Display and touch must
// occupy different SPI peripherals. TOUCH_CS is intentionally undefined.
#define USER_SETUP_LOADED
#define ILI9341_DRIVER
#define USE_HSPI_PORT
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 21
#define TFT_RGB_ORDER TFT_BGR
#define SPI_FREQUENCY 55000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#define CYD_BOARD_KEY "cyd28"
#define CYD_BOARD_NAME "ESP32-2432S028R"
#define CYD_DRIVER_NAME "ILI9341"
#define CYD_NATIVE_WIDTH 240
#define CYD_NATIVE_HEIGHT 320
#define CYD_EXPECTED_WIDTH 320
#define CYD_EXPECTED_HEIGHT 240
#define CYD_LANDSCAPE_ROT 1
#define TFT_BL_PIN 21
#define CYD_HAS_RGB_LED 1
#define CYD_HAS_LDR 1
#define CYD_RGB_ACTIVE_LOW 1
#define LED_R_PIN 4
#define LED_G_PIN 16
#define LED_B_PIN 17
#define LDR_PIN 34
#define TOUCH_CLK_PIN 25
#define TOUCH_MISO_PIN 39
#define TOUCH_MOSI_PIN 32
#define TOUCH_CS_PIN 33
#define TOUCH_IRQ_PIN 36
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3700

