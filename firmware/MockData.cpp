#include "MockData.h"
#include <math.h>

// ---- GraphRange helpers (declared in Types.h, defined here since MockData
// is the module most concerned with time spans) ----
const char* graphRangeLabel(GraphRange r) {
  switch (r) {
    case GraphRange::DAY_1:  return "1 Day";
    case GraphRange::DAY_7:  return "7 Days";
    case GraphRange::DAY_30: return "30 Days";
    case GraphRange::DAY_90: return "90 Days";
    case GraphRange::YEAR_1: return "1 Year";
    default: return "?";
  }
}

unsigned long graphRangeSeconds(GraphRange r) {
  const unsigned long DAY = 86400UL;
  switch (r) {
    case GraphRange::DAY_1:  return DAY;
    case GraphRange::DAY_7:  return DAY * 7;
    case GraphRange::DAY_30: return DAY * 30;
    case GraphRange::DAY_90: return DAY * 90;
    case GraphRange::YEAR_1: return DAY * 365;
    default: return DAY * 7;
  }
}

const char* ledEffectLabel(LedEffect e) {
  switch (e) {
    case LedEffect::OFF:        return "Off";
    case LedEffect::SOLID:      return "Solid Color";
    case LedEffect::RAINBOW:    return "Rainbow";
    case LedEffect::COLOR_WIPE: return "Color Wipe";
    case LedEffect::BREATHING:  return "Breathing";
    default: return "?";
  }
}

namespace MockData {

namespace {
  // A handful of hard-coded demo cards so the device is useful out of the
  // box with zero setup. Card ids "demo-N" are recognized by getMockPrice/
  // getMockHistory below and never sent to the real API.
  CardData demoCards[4];
  bool demoInitialized = false;

  void initDemoCards() {
    if (demoInitialized) return;
    demoCards[0] = { "demo-1", "Charizard ex", "Obsidian Flames", "125/197", "Double Rare", "pokemon", true };
    demoCards[1] = { "demo-2", "Black Lotus", "Alpha", "", "", "magic", true };
    demoCards[2] = { "demo-3", "Blue-Eyes White Dragon", "Legend of Blue Eyes White Dragon", "LOB-001", "1st Edition", "yugioh", true };
    demoCards[3] = { "demo-4", "Elsa, Spirit of Winter", "The First Chapter", "42/204", "Enchanted", "lorcana", true };
    demoInitialized = true;
  }

  // Simple deterministic hash so the same cardId always maps to the same
  // "random-looking" base price and wiggle pattern (no real RNG needed).
  uint32_t hashString(const String& s) {
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < s.length(); i++) {
      h ^= (uint8_t)s[i];
      h *= 16777619UL;
    }
    return h;
  }
}

const CardData* getDemoCards(int& count) {
  initDemoCards();
  count = 4;
  return demoCards;
}

PriceData getMockPrice(const String& cardId) {
  PriceData p;
  uint32_t h = hashString(cardId);
  // Base price between ~$2 and ~$202, deterministic per card id.
  p.price = 2.0f + (h % 20000) / 100.0f;
  strncpy(p.currency, "USD", sizeof(p.currency));
  p.lastUpdatedEpoch = millis() / 1000UL;
  p.source = DataSource::DEMO;
  p.valid = true;
  return p;
}

int getMockHistory(const String& cardId, GraphRange range, PricePoint outPoints[], int maxPoints) {
  PriceData base = getMockPrice(cardId);
  uint32_t h = hashString(cardId + (char)range);
  unsigned long span = graphRangeSeconds(range);
  int n = maxPoints < MAX_GRAPH_POINTS ? maxPoints : MAX_GRAPH_POINTS;
  unsigned long now = millis() / 1000UL;

  for (int i = 0; i < n; i++) {
    float t = (float)i / (float)(n - 1); // 0..1 across the range
    // Smooth, deterministic wiggle: sum of two sine waves with a per-card
    // phase offset derived from the hash, plus a gentle overall drift.
    float phase = (h % 1000) / 1000.0f * TWO_PI;
    float wiggle = 0.12f * sinf(t * 6.0f + phase) + 0.05f * sinf(t * 17.0f + phase * 2.0f);
    float drift = 0.10f * (t - 0.5f); // slight up/down trend across the window
    float price = base.price * (1.0f + wiggle + drift);
    if (price < 0.01f) price = 0.01f;

    outPoints[i].timestampEpoch = now - (unsigned long)((1.0f - t) * span);
    outPoints[i].price = price;
  }
  return n;
}

} // namespace MockData
