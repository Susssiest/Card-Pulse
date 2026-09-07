/*
 * LedController.h
 * ---------------------------------------------------------------------------
 * Drives the WS2812B strip via FastLED. Strip length, GPIO pin, and color
 * order all come from Config.h so they can be changed in one place.
 *
 * All animation is non-blocking: call LedController::update() every loop()
 * iteration; it internally paces itself with millis() and only actually
 * pushes new pixel data every LED_ANIM_INTERVAL_MS.
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "Types.h"

namespace LedController {

void begin();

// Call every loop() iteration. Cheap no-op most of the time; does real work
// only every LED_ANIM_INTERVAL_MS.
void update();

// ---- Settings (persisted to NVS by CardFrame.ino / settings screens) ----
void setPower(bool on);
bool getPower();

void setBrightness(uint8_t brightness); // 0-255
uint8_t getBrightness();

void setColor(uint8_t r, uint8_t g, uint8_t b); // used by SOLID/COLOR_WIPE
void getColor(uint8_t &r, uint8_t &g, uint8_t &b);

void setEffect(LedEffect effect);
LedEffect getEffect();

void setSpeed(uint8_t speed); // 0-255, higher = faster animation
uint8_t getSpeed();

// Load / save all of the above to Preferences (NVS namespace NVS_NAMESPACE_LED).
void loadSettings();
void saveSettings();

} // namespace LedController
