/*
 * LedController.cpp
 * ---------------------------------------------------------------------------
 * IMPORTANT WIRING NOTE (see README.md for the full explanation):
 * PIN_LED_DATA (an ESP32-S3 3.3V GPIO) drives the 74AHCT14 buffer/inverter,
 * whose OUTPUT (already boosted to a clean 5V logic level) feeds the 330ohm
 * series resistor and then the first WS2812B's DIN pin. The strip's 5V and
 * GND come straight from the boost converter, NOT from the ESP32-S3.
 * ---------------------------------------------------------------------------
 */
#include "LedController.h"
#include "Config.h"
#include <FastLED.h>
#include <Preferences.h>

namespace LedController {

namespace {
  CRGB leds[LED_COUNT];
  Preferences prefs;

  bool powerOn = true;
  uint8_t brightness = LED_DEFAULT_BRIGHTNESS;
  uint8_t colorR = 255, colorG = 80, colorB = 0; // warm amber default
  LedEffect effect = LedEffect::SOLID;
  uint8_t speed = 128;

  unsigned long lastStepMs = 0;
  uint16_t animPhase = 0; // generic animation counter, meaning depends on effect

  // ---- Effect renderers -----------------------------------------------
  // Each is a pure function of `animPhase` so effects stay perfectly
  // non-blocking: no delay() anywhere in this file.

  void renderSolid() {
    fill_solid(leds, LED_COUNT, CRGB(colorR, colorG, colorB));
  }

  void renderRainbow() {
    fill_rainbow(leds, LED_COUNT, animPhase & 0xFF, 256 / LED_COUNT + 1);
  }

  void renderColorWipe() {
    // Wipes the chosen color down the strip, then wipes back off, and repeats.
    uint16_t cycle = LED_COUNT * 2;
    uint16_t pos = animPhase % cycle;
    for (int i = 0; i < LED_COUNT; i++) {
      bool lit = (pos < LED_COUNT) ? (i <= pos) : (i > (pos - LED_COUNT));
      leds[i] = lit ? CRGB(colorR, colorG, colorB) : CRGB::Black;
    }
  }

  void renderBreathing() {
    // Sine-based breathing looks smoother than a linear ramp.
    uint8_t level = beatsin8((speed / 8) + 1, 10, 255, 0, 0);
    fill_solid(leds, LED_COUNT, CRGB(colorR, colorG, colorB));
    nscale8(leds, LED_COUNT, level);
  }

  void renderOff() {
    fill_solid(leds, LED_COUNT, CRGB::Black);
  }
} // anonymous namespace

void begin() {
  FastLED.addLeds<WS2812B, PIN_LED_DATA, LED_COLOR_ORDER>(leds, LED_COUNT);
  FastLED.setBrightness(brightness);
  loadSettings();
}

void update() {
  unsigned long now = millis();
  if (now - lastStepMs < LED_ANIM_INTERVAL_MS) return;
  lastStepMs = now;

  if (!powerOn || effect == LedEffect::OFF) {
    renderOff();
    FastLED.show();
    return;
  }

  // Advance the phase counter at a rate controlled by `speed`. Mapping speed
  // (0-255) to an increment keeps every effect's perceived pace consistent.
  uint16_t increment = 1 + (speed / 16);
  animPhase += increment;

  switch (effect) {
    case LedEffect::SOLID:      renderSolid();     break;
    case LedEffect::RAINBOW:    renderRainbow();   break;
    case LedEffect::COLOR_WIPE: renderColorWipe(); break;
    case LedEffect::BREATHING:  renderBreathing(); break;
    default:                    renderOff();       break;
  }

  FastLED.setBrightness(brightness);
  FastLED.show();
}

void setPower(bool on) { powerOn = on; }
bool getPower() { return powerOn; }

void setBrightness(uint8_t b) { brightness = b; }
uint8_t getBrightness() { return brightness; }

void setColor(uint8_t r, uint8_t g, uint8_t b) { colorR = r; colorG = g; colorB = b; }
void getColor(uint8_t &r, uint8_t &g, uint8_t &b) { r = colorR; g = colorG; b = colorB; }

void setEffect(LedEffect e) { effect = e; animPhase = 0; }
LedEffect getEffect() { return effect; }

void setSpeed(uint8_t s) { speed = s; }
uint8_t getSpeed() { return speed; }

void loadSettings() {
  prefs.begin(NVS_NAMESPACE_LED, /*readOnly=*/true);
  powerOn    = prefs.getBool("power", true);
  brightness = prefs.getUChar("bright", LED_DEFAULT_BRIGHTNESS);
  colorR     = prefs.getUChar("r", colorR);
  colorG     = prefs.getUChar("g", colorG);
  colorB     = prefs.getUChar("b", colorB);
  effect     = static_cast<LedEffect>(prefs.getUChar("effect", static_cast<uint8_t>(LedEffect::SOLID)));
  speed      = prefs.getUChar("speed", 128);
  prefs.end();
}

void saveSettings() {
  prefs.begin(NVS_NAMESPACE_LED, /*readOnly=*/false);
  prefs.putBool("power", powerOn);
  prefs.putUChar("bright", brightness);
  prefs.putUChar("r", colorR);
  prefs.putUChar("g", colorG);
  prefs.putUChar("b", colorB);
  prefs.putUChar("effect", static_cast<uint8_t>(effect));
  prefs.putUChar("speed", speed);
  prefs.end();
}

} // namespace LedController
