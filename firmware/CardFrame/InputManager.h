/*
 * InputManager.h
 * ---------------------------------------------------------------------------
 * Reads the KY-040 rotary encoder (CLK/DT only — its built-in switch is
 * intentionally unused) and the separate 12mm momentary push button.
 *
 * Everything here is non-blocking and millis()-based: call
 * InputManager::update() once per loop() iteration, then ask it what
 * happened since the last call.
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>

namespace InputManager {

// Call once from setup().
void begin();

// Call once per loop() iteration, as early as possible. Cheap and fast.
void update();

// Returns the net encoder movement (+1 per detent clockwise, -1 per detent
// counter-clockwise) accumulated since the last call, then resets to 0.
// Using "steps since last call" (rather than a raw running total) keeps
// every screen's scrolling logic simple: `int steps = InputManager::takeEncoderSteps();`
int takeEncoderSteps();

// True exactly once per physical press of the separate momentary button
// (already debounced). This is the ONLY input used for select / OK /
// confirm / open / save / connect / back actions everywhere in the UI.
bool wasButtonPressed();

} // namespace InputManager
