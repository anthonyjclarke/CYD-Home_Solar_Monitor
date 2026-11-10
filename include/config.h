#pragma once

#define PROJECT_NAME "CYD-Home-Solar-Monitor"
#define FIRMWARE_VERSION "1.0.2"
#define IMPROV_SETUP_ENABLED 1
#define IMPROV_DEVICE_PREFIX "CYD-Solar"
#define APP_DEFAULT_DEBUG_LEVEL 3
#define APP_DEFAULT_BRIGHTNESS 180
#define APP_OTA_PORT 3232
// mDNS name: the device answers at http://solar.local/ (ArduinoOTA advertises it too).
#define APP_MDNS_NAME "solar"
// Boot screen: held this long after Wi-Fi connects so the IP can be read, and
// this long if Wi-Fi never connects. A late connection gets the full hold.
// A tap skips the screen.
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

// ---- Board selection -------------------------------------------------------
// Each PlatformIO env sets one BOARD_* flag; none selects the classic 2.8-inch
// ESP32-2432S028R (also used by the host preview tools). The UI is always a
// 320 x 240 frame; larger panels show it centred (see display::pushFrame).
//
// Shared by every supported board: display on HSPI's native pins. USE_HSPI_PORT
// is required - TFT_eSPI defaults to VSPI, which own-bus touch also claims.
#define USER_SETUP_LOADED
#define USE_HSPI_PORT
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define CYD_EXPECTED_WIDTH 320
#define CYD_EXPECTED_HEIGHT 240
#define CYD_LANDSCAPE_ROT 1
#define CYD_RGB_ACTIVE_LOW 1
#define LED_G_PIN 16
#define LED_B_PIN 17
// XPT2046 chip select. Own-bus boards drive it with XPT2046_Touchscreen on
// VSPI; shared-bus boards define TOUCH_CS below so TFT_eSPI drives it instead.
#define TOUCH_CS_PIN 33

#if defined(BOARD_CYD40)
// 4.0-inch ESP32-32E (LCDwiki E32R40T / Freenove FNK0114S), ST7796S 480 x 320.
// Touch shares the display's SPI lines. Red LED moves to GPIO 22; GPIO 4 is
// the speaker amplifier enable (pulled up = off) and is left alone. No LDR.
#define ST7796_DRIVER
#define TFT_WIDTH 320
#define TFT_HEIGHT 480
#define TFT_BL 27
#define TFT_RGB_ORDER TFT_BGR
#define SPI_FREQUENCY 40000000
#define TOUCH_CS TOUCH_CS_PIN
#define CYD_BOARD_KEY "cyd40"
#define CYD_BOARD_NAME "ESP32-32E 4.0in"
#define CYD_DRIVER_NAME "ST7796S"
#define CYD_NATIVE_WIDTH 320
#define CYD_NATIVE_HEIGHT 480
#define TFT_BL_PIN 27
#define CYD_HAS_RGB_LED 1
#define CYD_HAS_LDR 0
#define LED_R_PIN 22
#define LDR_PIN -1
// The touch film reads X/Y swapped relative to the landscape panel (owner's
// board, 10-11-2026: history taps missed and the old calibration could not
// recover). Calibration re-detects the swap and stores it.
#define TOUCH_SWAP_XY 1
// The default raw span (200-3700 across the whole panel) narrowed to the
// centred 320 x 240 frame at x 80-399, y 40-279. Calibration replaces it.
#define TOUCH_X_MIN 783
#define TOUCH_X_MAX 3117
#define TOUCH_Y_MIN 637
#define TOUCH_Y_MAX 3262

#elif defined(BOARD_E32R28T_ST7789) || defined(BOARD_E32R28T_ILI9341)
// 2.8-inch ESP32-32E (LCDwiki E32R28T / Freenove FNK0114B and FNK0114F).
// Pins from Freenove's schematic and TFT_eSPI setups: display and touch match
// the classic CYD, but red LED is GPIO 22, GPIO 4 is the speaker amplifier
// enable (pulled up = off, left alone) and GPIO 34 is battery voltage, not an
// LDR. Freenove run the ST7789 at 80 MHz; 40 MHz is what the classic board
// actually gets from its 55 MHz request.
#if defined(BOARD_E32R28T_ST7789)
#define ST7789_DRIVER
#define TFT_INVERSION_OFF
#define CYD_BOARD_KEY "e32r28t-st7789"
#define CYD_BOARD_NAME "ESP32-32E 2.8in (FNK0114B)"
#define CYD_DRIVER_NAME "ST7789"
#else
#define ILI9341_2_DRIVER
#define TFT_INVERSION_OFF
#define CYD_BOARD_KEY "e32r28t-ili9341"
#define CYD_BOARD_NAME "ESP32-32E 2.8in (FNK0114F)"
#define CYD_DRIVER_NAME "ILI9341"
#endif
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_BL 21
#define TFT_RGB_ORDER TFT_BGR
#define SPI_FREQUENCY 40000000
#define CYD_NATIVE_WIDTH 240
#define CYD_NATIVE_HEIGHT 320
#define TFT_BL_PIN 21
#define CYD_HAS_RGB_LED 1
#define CYD_HAS_LDR 0
#define LED_R_PIN 22
#define LDR_PIN -1
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3700

#else
// Classic CYD: ESP32-2432S028R (2.8-inch), or ESP32-2432S024R (2.4-inch) with
// BOARD_CYD24. ILI9341 320 x 240. Touch has its own VSPI pins, so TOUCH_CS
// stays undefined (TFT_eSPI would otherwise also drive CS 33).
#define ILI9341_DRIVER
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_BL 21
#define TFT_RGB_ORDER TFT_BGR
#define SPI_FREQUENCY 55000000
#if defined(BOARD_CYD24)
#define CYD_BOARD_KEY "cyd24"
#define CYD_BOARD_NAME "ESP32-2432S024R"
#else
#define CYD_BOARD_KEY "cyd28"
#define CYD_BOARD_NAME "ESP32-2432S028R"
#endif
#define CYD_DRIVER_NAME "ILI9341"
#define CYD_NATIVE_WIDTH 240
#define CYD_NATIVE_HEIGHT 320
#define TFT_BL_PIN 21
#define CYD_HAS_RGB_LED 1
#define CYD_HAS_LDR 1
#define LED_R_PIN 4
#define LDR_PIN 34
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3700
#endif

#if !defined(TOUCH_SWAP_XY)
#define TOUCH_SWAP_XY 0
#endif

#if !defined(TOUCH_CS)
// Own-bus XPT2046 wiring (every board except the 4.0-inch).
#define TOUCH_CLK_PIN 25
#define TOUCH_MISO_PIN 39
#define TOUCH_MOSI_PIN 32
#define TOUCH_IRQ_PIN 36
#endif

