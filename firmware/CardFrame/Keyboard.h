/*
 * Keyboard.h
 * ---------------------------------------------------------------------------
 * Reusable on-screen keyboard widget driven entirely by the rotary encoder
 * (highlight movement) and the separate momentary button (select highlighted
 * key). Used by both the Add-Card search screen (text visible) and the
 * Wi-Fi password screen (text masked as bullets) — the masking is just a
 * draw-time flag, the navigation logic is identical either way.
 *
 * Layout: 4 character rows (A-Z, a-z, 0-9, symbols) plus a 5th "special key"
 * row (Backspace, Space, Confirm, Cancel). Rotating the encoder moves the
 * highlighted column; rotating past the last character of a row wraps to
 * the first character of the next row (and vice versa for the first
 * character wrapping back to the previous row's last character) — including
 * wrapping into/out of the special-key row, so everything is reachable with
 * pure rotation.
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

namespace Keyboard {

enum class Result : uint8_t {
  NONE,      // still editing
  CONFIRMED, // user selected "Search"/"Connect" — text() has the final value
  CANCELLED  // user selected "Cancel" — caller should discard text()
};

// Resets the widget to an empty string. `masked` = true shows bullets
// instead of typed characters (used for Wi-Fi passwords, never for card
// search text per the project spec).
void begin(bool masked);

// Feed encoder movement (+/- steps) into the highlight cursor.
void handleEncoderSteps(int steps);

// Feed one momentary-button press. Returns CONFIRMED/CANCELLED when the
// user activated one of those special keys; otherwise returns NONE (a
// character was typed, or nothing happened yet).
Result handleButtonPress();

// Current typed text (always the real text, even if masked on screen).
const String& text();

// Draws the current query/password line plus the keyboard grid with the
// highlighted key. `caption` is a short instruction shown above the field,
// e.g. "Search for a card" or "Enter Wi-Fi password".
void draw(U8G2& d, const char* caption);

} // namespace Keyboard
