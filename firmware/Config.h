/*
 * Config.h
 * ---------------------------------------------------------------------------
 * SINGLE SOURCE OF TRUTH for hardware pins, the display driver, the
 * tcgapi.dev connection, timing constants, and LED defaults.
 *
 * If you rewire something, change the pin here — never hunt through the
 * other .cpp files for a hard-coded GPIO number. Every other file includes
 * this one and uses these names.
 * ---------------------------------------------------------------------------
 */
#pragma once

// #############################################################################
// # 1. DISPLAY CONFIGURATION  — SKU: MC242GX
// #############################################################################
// CONFIRMED from the manufacturer (LCDWIKI/QDtech) specification for
// SKU MC242GX: 2.42" monochrome OLED, 128x64 pixels, SSD1309 driver IC,
// I2C ("IIC") interface, 5-pin header (GND, VCC, SCL, SDA, RES-unpopulated),
// runs from 3.3V-5V. Two I2C addresses are available depending on how the
// board's address-select resistor is soldered from the factory:
//   solder pad -> 0x78 (write byte) = 7-bit address 0x3C  (MOST COMMON / default)
//   solder pad -> 0x7A (write byte) = 7-bit address 0x3D
// Source: LCDWIKI MC242GX specification & user manual (lcdwiki.com/res/MC242GX).
//
// >>> ASSUMPTION TO CONFIRM: if your specific board came with the address
// >>> pad soldered to 0x7A, change DISPLAY_I2C_ADDR below to 0x3D. Most units
// >>> ship as 0x3C. If the screen stays blank, this is the first thing to check.
#define DISPLAY_DRIVER_SSD1309      1     // driver IC confirmed by datasheet
#define DISPLAY_WIDTH_PX            128
#define DISPLAY_HEIGHT_PX           64
#define DISPLAY_I2C_ADDR            0x3C  // 7-bit address (0x78 >> 1)
#define DISPLAY_USE_I2C             1     // MC242GX in this BOM is the IIC variant
#define DISPLAY_RESET_PIN           -1    // RES pin is unpopulated on MC242GX by
                                          // default; -1 tells U8g2 "no reset pin,
                                          // use software reset". Solder RES and
                                          // set a real GPIO here if you need it.

// #############################################################################
// # 2. GPIO PIN MAP  (ESP32-S3, generic dev board with USB-C)
// #############################################################################
// >>> ASSUMPTION TO CONFIRM: these are sensible, commonly-free GPIOs on most
// >>> ESP32-S3 dev boards (DevKitC-1 style). GPIO0, 45, 46 are strapping pins
// >>> (boot mode) and GPIO19/20 are the native USB D-/D+ on most boards —
// >>> all four are intentionally AVOIDED below. If your specific board (XIAO
// >>> ESP32S3, ESP32-S3-Zero, etc.) reserves different pins, just edit the
// >>> numbers here; nothing else in the firmware needs to change.

// --- I2C bus (shared by the MC242GX OLED) ---
#define PIN_I2C_SDA        8
#define PIN_I2C_SCL        9

// --- KY-040 rotary encoder (quadrature outputs only — see note below) ---
#define PIN_ENCODER_CLK    4   // KY-040 "CLK" output (A phase)
#define PIN_ENCODER_DT     5   // KY-040 "DT"  output (B phase)
// NOTE: The KY-040 module's own built-in push switch (its "SW" pin) is
// intentionally NOT wired/read anywhere in this firmware, per project spec.
// All select/confirm/back actions use the separate 12mm momentary button below.

// --- Separate 12mm momentary push button (SW_Push) ---
#define PIN_BUTTON_SELECT  6   // OK / Select / Confirm / Save / Connect / Back

// --- WS2812B data line (goes through the 74AHCT14 level shifter, see the
//     wiring plan in README.md before the LED strip) ---
#define PIN_LED_DATA       7

// #############################################################################
// # 3. WS2812B LED STRIP CONFIGURATION
// #############################################################################
// >>> ASSUMPTION TO CONFIRM: set this to the ACTUAL number of LEDs on your
// >>> strip before flashing. Everything else (current budget math in
// >>> README.md, animation timing) is derived from this constant.
#define LED_COUNT          24
#define LED_COLOR_ORDER    GRB   // WS2812B is almost always GRB; swap if colors
                                  // look wrong (e.g. red shows as green)
#define LED_DEFAULT_BRIGHTNESS 128   // 0-255
#define LED_MAX_BRIGHTNESS      255  // hard ceiling exposed in the UI

// #############################################################################
// # 4. tcgapi.dev API CONFIGURATION
// #############################################################################
// Verified with a real, authenticated API key on 2026-09-06 by calling every
// endpoint this firmware uses and inspecting the actual responses:
//   - Base host:         https://api.tcgapi.dev
//   - Auth:               header "X-API-Key: <your key>"
//   - Key format:         "tcg_live_" + random string
//   - Public (no key):    GET /v1/games , /v1/games/:slug , /v1/games/:slug/sets
//   - Free tier:          GET /v1/search , /v1/cards/:id , /v1/cards/:id/prices
//   - Hobby+ tier:        GET /v1/cards/:id/history            (this key's tier)
//   - Pro+ tier:          GET /v1/cards/:id/history/detailed   (confirmed NOT
//                         included in this key's tier — returns 403 TIER_REQUIRED)
// Response field names for every endpoint below are confirmed from real
// response bodies, not guessed — see the per-function comments in
// TcgApiClient.cpp for the exact JSON shapes observed. Two things are worth
// knowing if you inspect traffic yourself: an invalid/missing key returns
// HTTP 402 (an x402 micropayment challenge) rather than a plain 401, and
// error bodies look like {"error":{"message":"...","code":"..."}}.
//
// >>> SECURITY: this is now a REAL key, not a placeholder. Never commit this
// >>> file to a public repo as-is, never post it in a forum/Discord/issue,
// >>> and never let SERIAL_DEBUG_ENABLED code print it (TcgApiClient.cpp
// >>> deliberately never logs this value). If you do publish this project's
// >>> source (e.g. on GitHub), replace the line below with the
// >>> "YOUR_TCGAPI_KEY_HERE" placeholder before pushing, and keep your real
// >>> key only in a local, untracked copy of this file.
static const char* TCGAPI_BASE_URL   = "https://api.tcgapi.dev";
static const char* TCGAPI_API_KEY    = "YOUR_TCGAPI_KEY_HERE";
static const char* TCGAPI_HEADER_NAME = "X-API-Key";

// Documented endpoint paths (base URL + path). "%s" is replaced with the
// card id or query string by TcgApiClient — see buildUrl() there.
static const char* TCGAPI_PATH_SEARCH        = "/v1/search";          // ?q=&game=
static const char* TCGAPI_PATH_CARD_DETAILS  = "/v1/cards/%s";
static const char* TCGAPI_PATH_CARD_PRICES   = "/v1/cards/%s/prices";
static const char* TCGAPI_PATH_CARD_HISTORY  = "/v1/cards/%s/history"; // confirmed: no range query param; server returns its tier's fixed window

// Free tier = 100 requests/day. Refuse to hammer the API — see
// API_REFRESH_INTERVAL_MS below and the manual-refresh-only rule after
// changing cards.
#define TCGAPI_HTTPS_TIMEOUT_MS   8000

// #############################################################################
// # 5. TIMING / REFRESH INTERVALS  (all non-blocking, millis()-based)
// #############################################################################
#define ENCODER_POLL_INTERVAL_MS      2      // how often we sample the encoder
#define BUTTON_DEBOUNCE_MS            35     // momentary button debounce window
#define ENCODER_DEBOUNCE_MS           2      // per-transition debounce
#define WIFI_SCAN_INTERVAL_MS         0      // 0 = only scan on demand
#define WIFI_CONNECT_TIMEOUT_MS       15000  // give up and show an error after this
#define API_REFRESH_INTERVAL_MS       (30UL * 60UL * 1000UL) // 30 min auto refresh
#define DISPLAY_FRAME_INTERVAL_MS     66     // ~15 fps redraw budget
#define LED_ANIM_INTERVAL_MS          16     // ~60 fps LED animation step

// #############################################################################
// # 6. NVS / Preferences namespaces & keys
// #############################################################################
static const char* NVS_NAMESPACE_WIFI   = "cf_wifi";
static const char* NVS_NAMESPACE_CARDS  = "cf_cards";
static const char* NVS_NAMESPACE_LED    = "cf_led";
static const char* NVS_NAMESPACE_CACHE  = "cf_cache";

// #############################################################################
// # 7. FEATURE / DEBUG FLAGS
// #############################################################################
#define SERIAL_DEBUG_ENABLED   1   // set to 0 to silence Serial.print debugging
// SECURITY: even with debug enabled, Wi-Fi passwords and the tcgapi.dev API
// key must NEVER be printed to Serial. See WiFiManager.cpp / TcgApiClient.cpp.

#if SERIAL_DEBUG_ENABLED
  #define DBG_PRINT(...)   Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
#endif
