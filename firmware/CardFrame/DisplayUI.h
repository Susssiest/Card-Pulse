/*
 * DisplayUI.h
 * ---------------------------------------------------------------------------
 * Owns the U8g2 display object, every screen's drawing code, and the menu /
 * state-machine navigation logic (which encoder/button events move you
 * between UIState values). It reads hardware/data modules directly
 * (CardLibrary, WiFiManager, LedController) since those are all
 * non-blocking and safe to call from anywhere.
 *
 * It deliberately does NOT call TcgApiClient directly: those HTTP calls can
 * block for up to a few seconds, and CardFrame.ino is the one place that
 * orchestrates *when* a blocking network call is allowed to happen (see the
 * "pending action" functions below and the big comment in CardFrame.ino).
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <U8g2lib.h>
#include "Types.h"

namespace DisplayUI {

void begin();

// Reset a screen's transient state (scroll position, keyboard buffer, etc.)
// Call this once every time CardFrame.ino changes `state`.
void onEnterState(UIState state);

// Applies one input "tick" (accumulated encoder steps + at-most-one button
// press) to the currently active screen. Returns the next UIState (equal to
// `state` if nothing changed).
UIState handleInput(UIState state, int encoderSteps, bool buttonPressed);

// Renders the given state. Call at a throttled rate (see DISPLAY_FRAME_INTERVAL_MS).
void draw(UIState state);

// ---- Data pushed in from CardFrame.ino after a (blocking) network call ----
void setCurrentPrice(const PriceData& p);
void setCurrentGraph(const PricePoint* points, int count, GraphRange range, DataSource source);
void setSearchResults(const CardResult* results, int count, bool success, const char* statusMessage);

// ---- "Please do a blocking network thing now" request, consumed once ----
// Returns true (and clears the flag) exactly once per search submitted from
// the on-screen keyboard. CardFrame.ino polls this once per loop() iteration.
bool consumePendingSearch(String& outQuery);

// The graph range currently selected/shown on Home (changed by the Graph
// Range Select screen). CardFrame.ino compares this against the range it
// last fetched data for, and re-fetches when it differs.
GraphRange currentGraphRange();

} // namespace DisplayUI
