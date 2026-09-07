/*
 * Types.h
 * ---------------------------------------------------------------------------
 * Shared data structures and enums used across every module.
 *
 * Keeping these in one header (instead of duplicating structs inside each
 * .h/.cpp pair) avoids circular #include problems: DisplayUI needs to know
 * what a CardData looks like, TcgApiClient needs to fill one in, and
 * CardLibrary needs to store one — nobody needs to include anybody else's
 * "big" header just to see a struct definition.
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>

// ============================================================================
// Overall UI state machine
// ============================================================================
// The whole firmware is one big state machine. `CardFrame.ino` holds the
// current state and calls the matching "enter/update/draw" functions in
// DisplayUI.cpp. Adding a new screen = add an enum value + a case in the
// switch statements in CardFrame.ino and DisplayUI.cpp.
enum class UIState : uint8_t {
  HOME,                     // main dashboard
  CARD_LIBRARY,             // scrollable list of saved cards
  CARD_LIBRARY_REMOVE_CONFIRM, // "are you sure?" before deleting a card
  ADD_CARD_KEYBOARD,        // on-screen keyboard to type a search query
  ADD_CARD_SEARCHING,       // waiting on the HTTP request (non-blocking)
  ADD_CARD_RESULTS,         // paginated list of search results
  WIFI_SCAN_LIST,           // list of nearby Wi-Fi networks
  WIFI_PASSWORD_KEYBOARD,   // masked password entry keyboard
  WIFI_CONNECTING,          // connection in progress (non-blocking)
  GRAPH_RANGE_SELECT,       // 1d/7d/30d/90d/1y picker
  LED_SETTINGS_LIST,        // list of LED settings (on/off, brightness, ...)
  LED_SETTINGS_EDIT         // editing the highlighted LED setting's value
};

// Which item is highlighted on the home screen. Rotating the encoder on the
// HOME state cycles through these in order.
enum class HomeFocus : uint8_t {
  WIFI_ICON = 0,
  LED_ICON,
  LIBRARY_ICON,
  GRAPH,
  COUNT // sentinel, not a real focus target — used for wraparound math
};

// ============================================================================
// Wi-Fi status (drives the little icon on the home screen)
// ============================================================================
enum class WifiStatus : uint8_t {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  ERROR
};

// ============================================================================
// Data-freshness label. Every price/graph value shown on screen is tagged
// with where it came from, so the UI can honestly show "Demo Data" instead
// of pretending a mocked number is a live market price.
// ============================================================================
enum class DataSource : uint8_t {
  LIVE,       // fetched from tcgapi.dev this session
  CACHED,     // loaded from NVS, fetched in a previous session
  DEMO        // synthetic / mock data, no network involved
};

// ============================================================================
// Card catalog + pricing structures
// ============================================================================

// One row in a search-results list (lightweight — just enough to show a
// picker and to resolve to a full CardData on selection).
struct CardResult {
  String cardId;      // tcgapi.dev card id (as a string; API ids are numeric
                       // but we keep them as strings so we never lose
                       // precision and so mock ids like "demo-1" also work)
  String name;
  String setName;
  String number;       // collector number, e.g. "125/197"
};

// A fully resolved card, as stored in the card library and shown on Home.
struct CardData {
  String cardId;
  String name;
  String setName;
  String collectorNumber;
  String variant;       // e.g. "Holo", "Foil", "1st Edition" — optional
  String game;           // e.g. "pokemon", "magic" — optional, for display
  bool valid = false;    // false = slot/struct not populated yet
};

// A single current-price snapshot.
struct PriceData {
  float price = 0.0f;
  char currency[4] = "USD";
  unsigned long lastUpdatedEpoch = 0; // seconds since boot or NTP epoch
  DataSource source = DataSource::DEMO;
  bool valid = false;
};

// One point on the price-history graph.
struct PricePoint {
  unsigned long timestampEpoch;
  float price;
};

// Selectable graph time ranges (Graph Settings screen).
enum class GraphRange : uint8_t {
  DAY_1 = 0,
  DAY_7,
  DAY_30,
  DAY_90,
  YEAR_1,
  COUNT // sentinel
};

// Human-readable labels for GraphRange, defined once in MockData.cpp / used
// everywhere so the UI text always matches the enum.
const char* graphRangeLabel(GraphRange r);
// Approximate span of a range, used by MockData to spread synthetic points.
unsigned long graphRangeSeconds(GraphRange r);

// ============================================================================
// LED effects
// ============================================================================
enum class LedEffect : uint8_t {
  OFF = 0,
  SOLID,
  RAINBOW,
  COLOR_WIPE,
  BREATHING,
  COUNT // sentinel
};

const char* ledEffectLabel(LedEffect e);

// Which row is highlighted in the LED settings list.
enum class LedSetting : uint8_t {
  POWER = 0,   // on/off
  BRIGHTNESS,
  COLOR,
  EFFECT,
  SPEED,
  COUNT
};

// ============================================================================
// Small fixed-size limits (kept centralized so array sizes stay consistent)
// ============================================================================
static const int MAX_SAVED_CARDS   = 20;  // card library capacity
static const int MAX_SEARCH_RESULTS = 10; // results shown per search page
static const int MAX_GRAPH_POINTS  = 32;  // points plotted on the graph
static const int MAX_WIFI_NETWORKS = 20;  // scan results kept
static const int MAX_TEXT_LEN      = 32;  // search query / password length
