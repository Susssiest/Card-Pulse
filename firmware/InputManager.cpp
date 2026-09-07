/*
 * InputManager.cpp
 * ---------------------------------------------------------------------------
 * Encoder decoding uses the classic "quadrature state table" technique
 * (sometimes called the Ben Buxton / PJRC method). It is robust against
 * mechanical contact bounce because it only registers a step after a full,
 * valid A/B transition sequence — a plain "did CLK just go LOW" edge
 * detector to the same job is far noisier on cheap KY-040 modules.
 * ---------------------------------------------------------------------------
 */
#include "InputManager.h"
#include "Config.h"

namespace InputManager {

namespace {
  // ---- Encoder state ----
  uint8_t encState = 0;          // current position in the transition table
  int32_t encAccumulator = 0;    // raw half-steps accumulated by the table
  int32_t encStepsAvailable = 0; // net full detents ready to be consumed
  unsigned long lastEncoderPollMs = 0;

  // Quadrature transition table. Index = (oldAB << 2) | newAB.
  // Produces +1 on a complete clockwise detent, -1 on counter-clockwise,
  // 0 otherwise (including bounce/invalid transitions, which are ignored).
  // This is a well-known table for encoders that produce one full quadrature
  // cycle (4 transitions) per detent, which matches the standard KY-040.
  const int8_t QUAD_TABLE[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
  };

  // ---- Button state ----
  bool buttonRawLast = HIGH;      // assume INPUT_PULLUP, idle = HIGH
  bool buttonStableState = HIGH;
  unsigned long buttonLastChangeMs = 0;
  bool buttonPressEventPending = false;

  void pollEncoder() {
    unsigned long now = millis();
    if (now - lastEncoderPollMs < ENCODER_POLL_INTERVAL_MS) return;
    lastEncoderPollMs = now;

    uint8_t a = digitalRead(PIN_ENCODER_CLK);
    uint8_t b = digitalRead(PIN_ENCODER_DT);
    uint8_t newAB = (a << 1) | b;
    uint8_t oldAB = encState & 0x3;
    int8_t movement = QUAD_TABLE[(oldAB << 2) | newAB];
    encState = ((encState << 2) | newAB) & 0x0F;

    if (movement != 0) {
      encAccumulator += movement;
      // 4 half-steps = 1 physical detent on a standard KY-040.
      if (encAccumulator >= 4) {
        encStepsAvailable += 1;
        encAccumulator = 0;
      } else if (encAccumulator <= -4) {
        encStepsAvailable -= 1;
        encAccumulator = 0;
      }
    }
  }

  void pollButton() {
    unsigned long now = millis();
    bool raw = digitalRead(PIN_BUTTON_SELECT); // INPUT_PULLUP: pressed = LOW

    if (raw != buttonRawLast) {
      buttonLastChangeMs = now; // reset debounce timer on any change
      buttonRawLast = raw;
    }

    if ((now - buttonLastChangeMs) > BUTTON_DEBOUNCE_MS && raw != buttonStableState) {
      buttonStableState = raw;
      if (buttonStableState == LOW) { // transitioned to "pressed"
        buttonPressEventPending = true;
      }
    }
  }
} // anonymous namespace

void begin() {
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_SELECT, INPUT_PULLUP);

  // Seed the encoder state table with the current pin levels so the very
  // first poll doesn't see a spurious transition from an assumed "00".
  uint8_t a = digitalRead(PIN_ENCODER_CLK);
  uint8_t b = digitalRead(PIN_ENCODER_DT);
  encState = (a << 1) | b;
}

void update() {
  pollEncoder();
  pollButton();
}

int takeEncoderSteps() {
  int steps = encStepsAvailable;
  encStepsAvailable = 0;
  return steps;
}

bool wasButtonPressed() {
  if (buttonPressEventPending) {
    buttonPressEventPending = false;
    return true;
  }
  return false;
}

} // namespace InputManager
