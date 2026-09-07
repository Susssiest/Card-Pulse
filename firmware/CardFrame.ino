/*
 * CardFrame.ino
 * =============================================================================
 * Wi-Fi Trading-Card Display Frame — main sketch (ESP32-S3)
 * =============================================================================
 * This file is deliberately short: it owns setup()/loop(), the single
 * `currentState` variable that drives the whole UI state machine, and the
 * handful of moments where a BLOCKING network call to tcgapi.dev is allowed
 * to happen. Every actual screen, driver, and data module lives in its own
 * .h/.cpp pair (see the tab bar in the Arduino IDE) — read the comment block
 * at the top of each one for what it's responsible for.
 *
 * WHY NETWORK CALLS LIVE HERE AND NOT IN DisplayUI.cpp:
 * TcgApiClient's functions use the synchronous ArduinoHTTPClient library, so
 * each call can block for up to TCGAPI_HTTPS_TIMEOUT_MS (8s) if the network
 * is slow. If DisplayUI called them directly from inside handleInput() (which
 * runs on every single button press), a slow request would freeze button
 * input, encoder input, AND the LED animation for that whole time. Instead,
 * DisplayUI only ever transitions into a lightweight "waiting" state
 * (ADD_CARD_SEARCHING) or exposes what it currently needs
 * (DisplayUI::consumePendingSearch / DisplayUI::currentGraphRange), and this
 * file performs the actual blocking call at one clearly-marked point in
 * loop(), right after the non-blocking input/animation work for that
 * iteration is done. This keeps 100% of the blocking-network-call logic in
 * one auditable place.
 *
 * A fully non-blocking HTTP implementation (manual chunked reads spread
 * across multiple loop() iterations) would remove even this brief pause, but
 * is substantially more code; it's called out as a possible future upgrade
 * in README.md. For a first working version this is a reasonable trade-off:
 * WiFiManager's scan/connect (the other network-ish operations) ARE fully
 * non-blocking already, since those use ESP32 WiFi.h's own async APIs.
 * =============================================================================
 */
#include "Config.h"
#include "Types.h"
#include "InputManager.h"
#include "DisplayUI.h"
#include "CardLibrary.h"
#include "WiFiManager.h"
#include "TcgApiClient.h"
#include "LedController.h"
#include "MockData.h"
#include <Preferences.h>

// -----------------------------------------------------------------------------
// The master state variable. Everything else in the firmware answers the
// question "given this state, what do we draw / what input do we accept?"
// -----------------------------------------------------------------------------
UIState currentState = UIState::HOME;

// Bookkeeping so we know when a fresh tcgapi.dev fetch is actually needed,
// instead of re-fetching on every single loop() iteration.
String lastFetchedCardId = "";
GraphRange lastFetchedRange = GraphRange::DAY_7;
unsigned long lastAutoRefreshMs = 0;
unsigned long lastDrawMs = 0;
unsigned long wifiConnectedScreenSinceMs = 0; // for the "auto-return home" timeout

// =============================================================================
// Small NVS-backed cache for the CURRENT PRICE only (per library slot).
// Price history is cheap to regenerate as a mock curve, but the current
// price is the one thing worth remembering across a power cycle so Home
// doesn't show "Data Unavailable" the moment the frame is unplugged and
// tcgapi.dev is unreachable. Keyed by library slot index (0..MAX_SAVED_CARDS)
// rather than by the (potentially long) tcgapi.dev card id, since ESP32
// Preferences keys are limited to 15 characters.
// =============================================================================
void cachePriceForSlot(int slot, const PriceData& p) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE_CACHE, false)) return;
  char kPrice[8], kCur[8], kTs[8];
  snprintf(kPrice, sizeof(kPrice), "p%d", slot);
  snprintf(kCur, sizeof(kCur), "c%d", slot);
  snprintf(kTs, sizeof(kTs), "t%d", slot);
  prefs.putFloat(kPrice, p.price);
  prefs.putString(kCur, p.currency);
  prefs.putULong(kTs, p.lastUpdatedEpoch);
  prefs.end();
}

bool loadCachedPriceForSlot(int slot, PriceData& outPrice) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE_CACHE, true)) return false;
  char kPrice[8];
  snprintf(kPrice, sizeof(kPrice), "p%d", slot);
  if (!prefs.isKey(kPrice)) { prefs.end(); return false; }
  outPrice.price = prefs.getFloat(kPrice, 0.0f);
  char kCur[8], kTs[8];
  snprintf(kCur, sizeof(kCur), "c%d", slot);
  snprintf(kTs, sizeof(kTs), "t%d", slot);
  String cur = prefs.getString(kCur, "USD");
  strncpy(outPrice.currency, cur.c_str(), sizeof(outPrice.currency) - 1);
  outPrice.currency[sizeof(outPrice.currency) - 1] = '\0';
  outPrice.lastUpdatedEpoch = prefs.getULong(kTs, 0);
  outPrice.valid = true;
  prefs.end();
  return true;
}

// =============================================================================
// The one function that talks to tcgapi.dev for the currently selected card.
// Called: right after boot, right after the user picks a different card,
// right after the user changes the graph range, and periodically every
// API_REFRESH_INTERVAL_MS. Falls back live -> cached (NVS) -> mock, and tags
// the result with the correct DataSource so the UI never lies about it.
// =============================================================================
void refreshCurrentCardData() {
  const CardData& card = CardLibrary::selectedCard();
  if (!card.valid) {
    PriceData empty; // valid=false
    PricePoint noPoints[1];
    DisplayUI::setCurrentPrice(empty);
    DisplayUI::setCurrentGraph(noPoints, 0, DisplayUI::currentGraphRange(), DataSource::DEMO);
    return;
  }

  int slot = CardLibrary::selectedIndex();
  GraphRange range = DisplayUI::currentGraphRange();
  bool wifiUp = (WiFiManager::status() == WifiStatus::CONNECTED);

  // ---- Current price ----
  PriceData price;
  bool gotLive = wifiUp && TcgApiClient::fetchCurrentPrice(card.cardId, price);
  if (gotLive) {
    price.source = DataSource::LIVE;
    price.valid = true;
    cachePriceForSlot(slot, price);
  } else if (loadCachedPriceForSlot(slot, price)) {
    price.source = DataSource::CACHED;
  } else {
    price = MockData::getMockPrice(card.cardId);
    price.source = DataSource::DEMO;
  }
  DisplayUI::setCurrentPrice(price);

  // ---- Price history ----
  PricePoint points[MAX_GRAPH_POINTS];
  int count = 0;
  bool gotHistory = wifiUp && TcgApiClient::fetchPriceHistory(card.cardId, range, points, MAX_GRAPH_POINTS, count);
  DataSource graphSource;
  if (gotHistory && count >= 2) {
    graphSource = DataSource::LIVE;
  } else {
    // tcgapi.dev history requires a paid tier and/or the endpoint's schema
    // isn't confirmed yet (see TcgApiClient.cpp ADAPTER notes) -> fall back
    // to a clearly-labeled synthetic curve rather than pretending it's real.
    count = MockData::getMockHistory(card.cardId, range, points, MAX_GRAPH_POINTS);
    graphSource = DataSource::DEMO;
  }
  DisplayUI::setCurrentGraph(points, count, range, graphSource);

  lastFetchedCardId = card.cardId;
  lastFetchedRange = range;
  lastAutoRefreshMs = millis();
}

// =============================================================================
// setup()
// =============================================================================
void setup() {
#if SERIAL_DEBUG_ENABLED
  Serial.begin(115200);
  delay(200); // brief pause so the first log lines aren't lost on some boards
  DBG_PRINTLN("\n[CardFrame] Booting...");
#endif

  InputManager::begin();
  LedController::begin();
  CardLibrary::begin();      // loads saved cards from NVS, or seeds demo cards
  WiFiManager::begin();      // attempts auto-connect using saved credentials
  TcgApiClient::begin();
  DisplayUI::begin();        // brings up the MC242GX/SSD1309 display over I2C

  DisplayUI::onEnterState(currentState);

  // Populate Home with *something* immediately (cached or mock), so the
  // screen never shows an empty flash while the first live fetch is pending.
  refreshCurrentCardData();
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
  unsigned long now = millis();

  // ---- 1. Non-blocking input sampling (cheap, every iteration) ----
  InputManager::update();
  int steps = InputManager::takeEncoderSteps();
  bool pressed = InputManager::wasButtonPressed();

  // ---- 2. Let the active screen react to input, possibly changing state ----
  UIState nextState = DisplayUI::handleInput(currentState, steps, pressed);
  if (nextState != currentState) {
    UIState previousState = currentState;
    currentState = nextState;
    DisplayUI::onEnterState(currentState);

    // Leaving the Card Library having selected a *different* card, or
    // leaving Graph Range Select having applied a *different* range, both
    // land back on HOME — refresh data right away rather than waiting for
    // the periodic timer.
    bool cardChanged = (CardLibrary::selectedCard().cardId != lastFetchedCardId);
    bool rangeChanged = (DisplayUI::currentGraphRange() != lastFetchedRange);
    if (currentState == UIState::HOME && (previousState == UIState::CARD_LIBRARY ||
                                           previousState == UIState::GRAPH_RANGE_SELECT ||
                                           previousState == UIState::ADD_CARD_RESULTS) &&
        (cardChanged || rangeChanged)) {
      refreshCurrentCardData();
    }
    if (currentState == UIState::WIFI_CONNECTING) {
      wifiConnectedScreenSinceMs = 0; // reset; set once CONNECTED is observed below
    }
  }

  // ---- 3. The ONE place a blocking tcgapi.dev search call is allowed ----
  if (currentState == UIState::ADD_CARD_SEARCHING) {
    String query;
    if (DisplayUI::consumePendingSearch(query)) {
      CardResult results[MAX_SEARCH_RESULTS];
      int resultCount = 0;
      bool ok = false;
      const char* statusMsg = "";
      if (WiFiManager::status() != WifiStatus::CONNECTED) {
        statusMsg = "No Wi-Fi connection";
      } else {
        ok = TcgApiClient::searchCards(query, results, MAX_SEARCH_RESULTS, resultCount);
        if (!ok) {
          int http = TcgApiClient::lastHttpStatus();
          // Status codes confirmed against the live API on 2026-09-06:
          // 402 = missing/invalid key (tcgapi.dev's x402 payment challenge,
          // used in place of a plain 401), 403 = valid key but this
          // endpoint needs a higher paid tier, 404 = not found, 429 =
          // conventional rate-limit status (daily cap is 100 requests on
          // the free tier; not directly observed during testing but
          // handled defensively since it's easy to hit).
          if (http == 429) statusMsg = "Rate limited - try later";
          else if (http == 402) statusMsg = "Invalid or missing API key";
          else if (http == 403) statusMsg = "Needs a higher API tier";
          else if (http == 404) statusMsg = "Not found";
          else if (http <= 0) statusMsg = "Network error";
          else statusMsg = "tcgapi.dev error";
        }
      }
      DisplayUI::setSearchResults(results, resultCount, ok, statusMsg);
      currentState = UIState::ADD_CARD_RESULTS;
      DisplayUI::onEnterState(currentState);
    }
  }

  // ---- 4. Auto-advance out of the Wi-Fi connecting screen on success ----
  if (currentState == UIState::WIFI_CONNECTING && WiFiManager::status() == WifiStatus::CONNECTED) {
    if (wifiConnectedScreenSinceMs == 0) wifiConnectedScreenSinceMs = now;
    if (now - wifiConnectedScreenSinceMs > 1200) { // let the user see "Connected!" briefly
      currentState = UIState::HOME;
      DisplayUI::onEnterState(currentState);
      refreshCurrentCardData(); // Wi-Fi just came up -> worth a fresh fetch
    }
  }

  // ---- 5. Periodic background refresh (respects API_REFRESH_INTERVAL_MS) ----
  if (currentState == UIState::HOME && (now - lastAutoRefreshMs) >= API_REFRESH_INTERVAL_MS) {
    refreshCurrentCardData();
  }

  // ---- 6. Always-on non-blocking background work ----
  WiFiManager::update();
  LedController::update();

  // ---- 7. Throttled redraw ----
  if (now - lastDrawMs >= DISPLAY_FRAME_INTERVAL_MS) {
    DisplayUI::draw(currentState);
    lastDrawMs = now;
  }
}
