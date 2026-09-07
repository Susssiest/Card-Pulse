#include "TcgApiClient.h"
#include "Config.h"
#include "MockData.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace TcgApiClient {

namespace {
  WiFiClientSecure tlsClient;
  int httpStatus = 0;

  // Refuse to allocate/parse a JSON body bigger than this. Protects a
  // memory-constrained ESP32-S3 against a malformed or unexpectedly huge
  // response. A page of tcgapi.dev search/history results is a few KB, so
  // this is a generous ceiling.
  const size_t MAX_JSON_RESPONSE_BYTES = 24576;

  // URL-encodes a query string for GET params (spaces, punctuation, etc.).
  // Minimal implementation covering what card/set search text needs.
  String urlEncode(const String& s) {
    String out;
    out.reserve(s.length() * 3);
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
        out += c;
      } else if (c == ' ') {
        out += '+';
      } else {
        out += '%';
        out += hex[(c >> 4) & 0xF];
        out += hex[c & 0xF];
      }
    }
    return out;
  }

  // Converts a "YYYY-MM-DD" (or "YYYY-MM-DDTHH:MM:SS..." — only the date
  // part is used) date string, as returned by tcgapi.dev's history and
  // prices endpoints, into a UTC epoch timestamp (seconds). Implemented as
  // a small self-contained calendar calculation (Howard Hinnant's
  // "days_from_civil") instead of relying on mktime()/timegm(), since the
  // latter depend on the toolchain's timezone database being set up and
  // it's easy to get UTC-vs-local subtly wrong on an embedded target.
  long daysFromCivil(long y, int m, int d) {
    y -= (m <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);              // [0, 399]
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;   // [0, 146096]
    return era * 146097 + (long)doe - 719468;
  }

  unsigned long epochFromDateString(const String& s) {
    int y = 0, m = 0, d = 0;
    if (sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || y == 0) return 0;
    long days = daysFromCivil(y, m, d);
    return (unsigned long)days * 86400UL;
  }

  // Performs a GET request against TCGAPI_BASE_URL + path, with the
  // X-API-Key header attached. Returns true and fills `outBody` on HTTP 200.
  // Any other outcome (network error, non-200 status, oversized body)
  // returns false; httpStatus is updated either way for the caller to
  // inspect via lastHttpStatus().
  bool doGet(const String& path, String& outBody) {
    outBody = "";
    httpStatus = 0;

    if (WiFi.status() != WL_CONNECTED) {
      httpStatus = -1;
      DBG_PRINTLN("[TcgApi] No Wi-Fi connection; skipping request.");
      return false;
    }

    String url = String(TCGAPI_BASE_URL) + path;
    HTTPClient http;
    http.setTimeout(TCGAPI_HTTPS_TIMEOUT_MS);

    // NOTE: setInsecure() skips certificate validation. This is a common
    // placeholder for early bring-up on memory-constrained boards, but for
    // a "real" build you should pin tcgapi.dev's root CA with
    // tlsClient.setCACert(...) instead. Left as an explicit TODO so it is
    // never accidentally mistaken for a production-hardened setting.
    tlsClient.setInsecure(); // TODO: replace with setCACert() for production

    if (!http.begin(tlsClient, url)) {
      httpStatus = -1;
      return false;
    }
    http.addHeader(TCGAPI_HEADER_NAME, TCGAPI_API_KEY);

    int code = http.GET();
    httpStatus = code;

    // Confirmed against the live API (2026-09-06) with a real key:
    //   200            success
    //   402            missing/invalid API key — tcgapi.dev falls back to an
    //                  x402 micropayment challenge rather than a plain 401
    //                  when no valid subscription key is presented
    //   403            endpoint requires a higher paid tier than this key has
    //                  (body: {"error":{"message":"...","code":"TIER_REQUIRED"}})
    //   404            resource not found (e.g. bad card id)
    //                  (body: {"error":{"message":"...","code":"NOT_FOUND"}})
    //   429            (not observed directly — daily cap wasn't exhausted
    //                  during testing — but handled defensively below since
    //                  it's the conventional rate-limit status code and the
    //                  free tier's 100/day cap makes hitting it plausible)
    // None of these respond with a body worth deserializing as card data,
    // so every non-200 case below just records the status and bails out;
    // CardFrame.ino turns the status code into a user-facing message.
    if (code != HTTP_CODE_OK) {
      DBG_PRINT("[TcgApi] HTTP error: ");
      DBG_PRINTLN(code);
      http.end();
      return false;
    }

    int len = http.getSize();
    if (len > 0 && (size_t)len > MAX_JSON_RESPONSE_BYTES) {
      DBG_PRINTLN("[TcgApi] Response too large, aborting parse.");
      http.end();
      httpStatus = -2;
      return false;
    }

    outBody = http.getString();
    http.end();

    if (outBody.length() > MAX_JSON_RESPONSE_BYTES) {
      DBG_PRINTLN("[TcgApi] Response too large after read, aborting parse.");
      httpStatus = -2;
      return false;
    }
    return true;
  }

  // Several endpoints return pricing per *printing* (e.g. "Normal" vs
  // "Reverse Holofoil" vs "Holofoil") as separate rows in the same array,
  // since a single tcgapi.dev card id covers all printings of that card in
  // that set. This firmware shows one price/graph per saved card (no
  // per-printing selector in the UI yet — see README's assumptions list),
  // so we deterministically pick the plain "Normal" printing when present,
  // and fall back to whichever row appears first otherwise.
  bool isPreferredPrinting(const char* printing) {
    return printing && strcasecmp(printing, "Normal") == 0;
  }
}

void begin() {
  // WiFiClientSecure needs no special init here; TLS session setup happens
  // per-request in doGet(). Kept as its own function so setup() reads
  // cleanly and so a future cert-pinning step has an obvious home.
}

int lastHttpStatus() { return httpStatus; }

// ---------------------------------------------------------------------------
// searchCards — CONFIRMED against a live authenticated call on 2026-09-06.
//   GET /v1/search?q=<query>
// Real response shape (note: this differs slightly from the example on the
// tcgapi.dev marketing page, which shows "pagination"/"price" — the actual
// live API returns "meta"/"market_price" as parsed below; verified data
// wins over documentation examples):
//   { "data": [ { "id": 21939, "name": "Charizard", "clean_name": "...",
//                 "number": "025/185", "rarity": "Rare", "game_name": "Pokemon",
//                 "set_name": "SWSH04: Vivid Voltage", "printing": "Normal",
//                 "market_price": 3.66, "low_price": 1.34, ... }, ... ],
//     "meta": { "total": 507, "page": 1, "per_page": 50, "has_more": true },
//     "rate_limit": { "daily_limit": 100, "daily_remaining": 86, ... } }
// ---------------------------------------------------------------------------
bool searchCards(const String& query, CardResult results[], int maxResults, int& resultCount) {
  resultCount = 0;
  String path = String(TCGAPI_PATH_SEARCH) + "?q=" + urlEncode(query);

  String body;
  if (!doGet(path, body)) {
    return false; // caller (DisplayUI/CardFrame) decides how to react: show
                  // "no results" / "offline" — this function never blocks
                  // or freezes on failure.
  }

  JsonDocument doc; // ArduinoJson v7: grows dynamically, no manual sizing
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    DBG_PRINT("[TcgApi] JSON parse error (search): ");
    DBG_PRINTLN(err.c_str());
    return false;
  }

  JsonArray arr = doc["data"].as<JsonArray>();
  if (arr.isNull()) {
    DBG_PRINTLN("[TcgApi] Malformed search response (no 'data' array).");
    return false;
  }

  for (JsonObject item : arr) {
    if (resultCount >= maxResults) break;
    CardResult r;
    // "id" is a JSON number in the real response; stringify it since
    // CardResult keeps ids as String throughout the firmware (so mock ids
    // like "demo-1" also work uniformly - see Types.h).
    r.cardId  = String(item["id"].as<long>());
    r.name    = item["name"] | "";
    r.setName = item["set_name"] | "";
    r.number  = item["number"] | "";
    results[resultCount++] = r;
  }
  return true;
}

// ---------------------------------------------------------------------------
// fetchCardDetails — CONFIRMED against a live authenticated call on
// 2026-09-06.
//   GET /v1/cards/:id
//   { "data": { "id": 21939, "name": "Charizard", "number": "025/185",
//               "rarity": "Rare", "game_name": "Pokemon", "game_slug": "pokemon",
//               "set_name": "SWSH04: Vivid Voltage", ... many extra fields
//               (hp, attacks, abilities, custom_attributes, ...) that this
//               firmware doesn't need and ignores },
//     "rate_limit": {...} }
// Note: this endpoint has no dedicated "variant" (foil/holo/1st-edition)
// field, so `card.variant` is populated from `rarity` as the closest
// available stand-in — update this mapping here if tcgapi.dev later adds a
// proper variant field, or if you'd rather leave it blank.
// ---------------------------------------------------------------------------
bool fetchCardDetails(const String& cardId, CardData& card) {
  char pathBuf[64];
  snprintf(pathBuf, sizeof(pathBuf), TCGAPI_PATH_CARD_DETAILS, cardId.c_str());

  String body;
  if (!doGet(String(pathBuf), body)) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    DBG_PRINTLN("[TcgApi] JSON parse error (card details).");
    return false;
  }

  JsonObject obj = doc["data"].as<JsonObject>();
  if (obj.isNull() || obj["name"].isNull()) {
    DBG_PRINTLN("[TcgApi] Card details response missing expected fields.");
    return false;
  }

  card.cardId = cardId;
  card.name = obj["name"] | "";
  card.setName = obj["set_name"] | "";
  card.collectorNumber = obj["number"] | "";
  card.variant = obj["rarity"] | ""; // stand-in for a true variant field, see note above
  card.game = obj["game_name"] | "";
  card.valid = card.name.length() > 0;
  return card.valid;
}

// ---------------------------------------------------------------------------
// fetchCurrentPrice — CONFIRMED against a live authenticated call on
// 2026-09-06.
//   GET /v1/cards/:id/prices
//   { "data": [ { "card_id": 21939, "printing": "Normal", "market_price": 3.66,
//                 "low_price": 1.34, "median_price": 4, "buylist_price": null,
//                 "price_change_24h": 6.09, "last_updated_at":
//                 "2026-09-06T08:54:04.721Z", ... }, { "printing":
//                 "Reverse Holofoil", ... }, ... ],
//     "rate_limit": {...} }
// The array holds one row PER PRINTING of the card (Normal, Holofoil,
// Reverse Holofoil, ...) — see isPreferredPrinting() above for how this
// firmware picks a single row to show. `market_price` is occasionally null
// for a low-volume printing; low_price is used as a fallback in that case.
// There is no currency field — tcgapi.dev prices are USD (TCGPlayer-sourced).
// ---------------------------------------------------------------------------
bool fetchCurrentPrice(const String& cardId, PriceData& price) {
  char pathBuf[64];
  snprintf(pathBuf, sizeof(pathBuf), TCGAPI_PATH_CARD_PRICES, cardId.c_str());

  String body;
  if (!doGet(String(pathBuf), body)) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    DBG_PRINTLN("[TcgApi] JSON parse error (prices).");
    return false;
  }

  JsonArray arr = doc["data"].as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) {
    DBG_PRINTLN("[TcgApi] Prices response has no rows.");
    return false;
  }

  JsonObject chosen = arr[0].as<JsonObject>(); // default: first row
  for (JsonObject item : arr) {
    if (isPreferredPrinting(item["printing"] | "")) {
      chosen = item;
      break;
    }
  }

  float value = NAN;
  if (!chosen["market_price"].isNull()) value = chosen["market_price"].as<float>();
  else if (!chosen["low_price"].isNull()) value = chosen["low_price"].as<float>();

  if (isnan(value)) {
    DBG_PRINTLN("[TcgApi] No usable price field on the chosen printing row.");
    return false;
  }

  price.price = value;
  strncpy(price.currency, "USD", sizeof(price.currency) - 1);
  price.currency[sizeof(price.currency) - 1] = '\0';
  String ts = chosen["last_updated_at"] | "";
  price.lastUpdatedEpoch = ts.length() ? epochFromDateString(ts) : (millis() / 1000UL);
  price.source = DataSource::LIVE;
  price.valid = true;
  return true;
}

// ---------------------------------------------------------------------------
// fetchPriceHistory — CONFIRMED against a live authenticated call on
// 2026-09-06.
//   GET /v1/cards/:id/history   (requires Hobby tier or higher; a free-tier
//                                 key gets HTTP 403 TIER_REQUIRED here, which
//                                 is treated as a normal `return false`)
//   { "data": [ { "date": "2026-08-30", "printing": "Normal",
//                 "market_price": 3.69, "low_price": null,
//                 "avg_sales_price": 3.06, "sales_volume": 9 }, ... one row
//               per (date, printing) pair ... ],
//     "meta": { "total": 15 }, "rate_limit": {...} }
// IMPORTANT — confirmed by direct testing: this endpoint does NOT accept a
// range/window query parameter; it always returns whatever depth of history
// your subscription tier includes (a handful of most-recent days on this
// key), regardless of the GraphRange the user picked on-screen. Because of
// that, this function checks whether the data it got back actually covers a
// reasonable fraction of the REQUESTED range before calling it a success —
// e.g. if the user asked for "1 Year" but the API can only supply about a
// week, that is correctly treated as "not enough real data" so the UI falls
// back to a clearly-labeled Demo History curve instead of stretching a few
// real days across a full year and implying more coverage than exists.
// ---------------------------------------------------------------------------
bool fetchPriceHistory(const String& cardId, GraphRange range, PricePoint points[], int maxPoints, int& pointCount) {
  pointCount = 0;
  char pathBuf[64];
  snprintf(pathBuf, sizeof(pathBuf), TCGAPI_PATH_CARD_HISTORY, cardId.c_str());

  String body;
  if (!doGet(String(pathBuf), body)) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    DBG_PRINTLN("[TcgApi] JSON parse error (history).");
    return false;
  }

  JsonArray arr = doc["data"].as<JsonArray>();
  if (arr.isNull()) {
    DBG_PRINTLN("[TcgApi] History response missing an array of points.");
    return false;
  }

  for (JsonObject item : arr) {
    if (pointCount >= maxPoints) break;
    if (!isPreferredPrinting(item["printing"] | "")) continue; // one printing only, see note above
    if (item["market_price"].isNull()) continue; // e.g. a day with no sales recorded

    PricePoint pt;
    String dateStr = item["date"] | "";
    pt.timestampEpoch = epochFromDateString(dateStr);
    pt.price = item["market_price"].as<float>();
    if (pt.timestampEpoch == 0) continue; // couldn't parse the date; skip rather than plot garbage
    points[pointCount++] = pt;
  }

  if (pointCount < 2) return false;

  // Coverage check: does the data we actually got span a meaningful chunk
  // of the range the user asked to see? (See the big comment above.)
  unsigned long spanSeconds = points[pointCount - 1].timestampEpoch - points[0].timestampEpoch;
  unsigned long requestedSeconds = graphRangeSeconds(range);
  const float MIN_COVERAGE_FRACTION = 0.5f;
  if ((float)spanSeconds < (float)requestedSeconds * MIN_COVERAGE_FRACTION) {
    DBG_PRINTLN("[TcgApi] History covers too little of the requested range; using demo data instead.");
    return false;
  }

  return true;
}

} // namespace TcgApiClient
