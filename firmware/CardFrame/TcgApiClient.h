/*
 * TcgApiClient.h
 * ---------------------------------------------------------------------------
 * Talks to tcgapi.dev over HTTPS. Every function is synchronous from the
 * caller's point of view (it blocks for at most TCGAPI_HTTPS_TIMEOUT_MS
 * while the HTTP request is in flight) but CardFrame.ino only ever calls
 * these from a few well-defined, user-initiated moments (search submit,
 * card selected, manual refresh, scheduled refresh) — never from inside the
 * tight input/animation loop — so the UI feels responsive overall even
 * though a single call can take a second or two.
 *
 * WHAT IS DOCUMENTED (verified against tcgapi.dev/authentication and
 * tcgapi.dev/ on 2026-09-06) vs WHAT IS NOT:
 *   - Base host, auth header, and the /v1/search endpoint + its exact JSON
 *     shape ARE documented and are implemented for real below.
 *   - /v1/cards/:id, /v1/cards/:id/prices, and /v1/cards/:id/history are
 *     documented to EXIST (path + tier requirements) but their exact
 *     response JSON field names are NOT published anywhere we could find.
 *     Those three are marked "ADAPTER / TODO" in the .cpp: the HTTP call
 *     is real, but the JSON-parsing step tries a couple of plausible field
 *     names and falls back to mock data if parsing fails, so the app never
 *     crashes or freezes on a real but differently-shaped response.
 *     >>> Once you can see a real response body (e.g. via the API Explorer
 *     >>> at tcgapi.dev/api-explorer/, or your own logging), update the
 *     >>> field names in the ADAPTER sections of TcgApiClient.cpp.
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "Types.h"

namespace TcgApiClient {

void begin(); // sets up the TLS client; call once from setup()

// Searches tcgapi.dev for cards matching `query`. Fills `results` (caller-
// allocated array of size >= maxResults) and sets resultCount. Returns false
// on network/HTTP/parse failure (resultCount will be 0 in that case).
bool searchCards(const String& query, CardResult results[], int maxResults, int& resultCount);

// ADAPTER/TODO: response schema for this endpoint is unconfirmed (see header
// comment above). Returns false (not an app crash) if the response can't be
// parsed with our best-guess field names; caller should fall back to
// MockData in that case.
bool fetchCardDetails(const String& cardId, CardData& card);

// ADAPTER/TODO: same caveat as fetchCardDetails.
bool fetchCurrentPrice(const String& cardId, PriceData& price);

// ADAPTER/TODO: same caveat, plus: price history requires a Hobby+ plan.
// A free-tier key will get an HTTP error here by design (tier restriction) —
// that is treated as a normal failure (returns false), not a crash.
bool fetchPriceHistory(const String& cardId, GraphRange range, PricePoint points[], int maxPoints, int& pointCount);

// Returns the last HTTP status code seen (0 if no request has been made, or
// -1 for a transport-level failure e.g. no Wi-Fi / TLS error). Useful for
// showing a specific error message and for the rate-limit / offline logic
// in CardFrame.ino.
int lastHttpStatus();

} // namespace TcgApiClient
