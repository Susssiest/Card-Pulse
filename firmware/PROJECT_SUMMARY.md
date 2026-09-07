# CardFrame — Project Summary

## What this is

CardFrame is a battery-powered, Wi-Fi-connected desk display for a single
trading card at a time. It shows the card's name, set/number/variant, and
current market price, plots a price-history graph, and drives an addressable
LED strip for ambient lighting/effects around the frame. All navigation is
done with a rotary encoder (scroll/adjust) and a separate physical button
(select/confirm), with an on-screen keyboard for any text entry (Wi-Fi
passwords, card search). Card and pricing data comes from
[tcgapi.dev](https://tcgapi.dev); when Wi-Fi or the API is unavailable, the
frame falls back to the last cached price or clearly-labeled demo data so it
never freezes or lies about what it's showing.

## Goals

- Run on an ESP32-S3, flashed over its native USB-C port from the Arduino IDE.
- Organize the firmware as a clean, well-commented, modular Arduino sketch
  that's easy to extend later (new screens, a different display, a second
  data source, etc.).
- Make every screen navigable with exactly two physical controls: a KY-040
  rotary encoder (scroll / adjust value) and a separate 12mm momentary push
  button (select / confirm / back). The encoder's own built-in push switch is
  intentionally never used.
- Keep the UI responsive at all times — input, LED animation, and Wi-Fi
  housekeeping all use `millis()`-based non-blocking code, with the one
  unavoidable exception of the tcgapi.dev HTTP calls themselves (bounded to
  an 8-second timeout, triggered only at specific user-initiated moments —
  see `CardFrame.ino`'s top comment for the full rationale).
- Be honest about data provenance: every price and every graph is tagged as
  Live, Cached, or Demo, and the UI never presents mock numbers as real
  market data.

## Core user-facing behavior

**Home screen** — shows the selected card's name, set/number/variant, price,
and last-updated/data-source indicator; a Wi-Fi status icon, an LED-settings
icon, and a card-library icon; and a 7-day (default) price-history graph.
Rotating the encoder moves a highlight between the three icons and the
graph; pressing the button opens whichever is highlighted.

**Card library** — a scrollable list of saved cards plus "Add Card",
"Remove Card" (with a Yes/No confirmation screen), and "Back". Selecting a
card makes it current, returns Home, and refreshes its data.

**Add a card** — an on-screen keyboard (rotary-encoder driven, wraps between
character rows) for typing a search query, which is sent to tcgapi.dev;
results are shown as a scrollable list to pick from and add to the library.
Network errors, empty results, and rate limits are shown as a message
instead of freezing the device.

**Wi-Fi setup** — scans and lists nearby networks, lets you select one and
type its password on the same on-screen keyboard (masked as dots), and shows
connecting/success/failure without blocking the rest of the firmware.
Credentials are saved so the frame reconnects automatically after that.

**Graph settings** — switch the price-history range between 1 day, 7 days,
30 days, 90 days, and 1 year.

**LED settings** — power on/off, brightness, a color, an effect (Solid,
Rainbow, Color Wipe, Breathing, Off), and effect speed, for the WS2812B
strip.

**Offline/demo mode** — if Wi-Fi or tcgapi.dev is unavailable, or a specific
endpoint's response can't be parsed, the frame shows the last cached price
(if any) or a clearly-labeled synthetic "Demo Data"/"Demo History" value
instead of failing silently or showing something misleading.

## Hardware (bill of materials)

| # | Part | Role | Qty |
|---|------|------|-----|
| 1 | ESP32-S3 | Microcontroller, USB-C programming | 1 |
| 2 | WS2812B | Addressable LED strip | 1 |
| 3 | 3.7V 18650 Li-ion | Battery | 1 |
| 4 | MC242GX | 128x64 SSD1309 OLED display, I2C | 1 |
| 5 | KY-040 | Rotary encoder (CLK/DT only; built-in switch unused) | 1 |
| 6 | SW_Push | 12mm momentary push button | 1 |
| 7 | KCD01 | SPST power switch | 1 |
| 8 | (5V boost converter) | Steps battery voltage up to a regulated 5V rail | 1 |
| 9 | 74AHCT14 | Logic-level shifter/buffer for WS2812B data | 1 |
| 10 | 1000µF capacitor | Bulk capacitor on the LED power rail | 1 |
| 11 | 330Ω resistor | Series resistor on the WS2812B data line | 1 |
| 12 | Wire connector kit | General wiring/connectors | 1 |

Full wiring table, power diagram, and the exact 74AHCT14 pin-by-pin plan are
in `README.md`.

## Software architecture

The sketch is split into one file per concern so each piece can be modified,
tested, or swapped independently:

- `CardFrame.ino` — setup()/loop(), the master UI state machine, and the
  handful of moments a blocking tcgapi.dev call is allowed to happen.
- `Types.h` — shared structs/enums (`CardData`, `PriceData`, `UIState`, ...).
- `Config.h` — every pin, timing constant, display setting, and tcgapi.dev
  endpoint/auth placeholder, all in one place.
- `InputManager` — debounced rotary-encoder and button reading.
- `DisplayUI` — the U8g2 display driver plus every screen's drawing and
  navigation logic.
- `Keyboard` — the reusable on-screen keyboard widget (search text and
  masked Wi-Fi passwords both use it).
- `CardLibrary` — saved-card storage (NVS-backed), deliberately decoupled
  from the API and UI so it can later be swapped for SD-card storage, a web
  interface, or cloud sync.
- `WiFiManager` — non-blocking scan/connect state machine wrapping
  ESP32 `WiFi.h`.
- `TcgApiClient` — all HTTPS calls to tcgapi.dev, isolated so its exact
  endpoints/response fields can be updated later without touching any UI
  code. Endpoints whose response schema isn't publicly documented are marked
  `ADAPTER/TODO` and fall back to mock data rather than crashing.
- `LedController` — WS2812B/FastLED driver with non-blocking animation.
- `MockData` — hard-coded demo cards and deterministic mock price/graph
  generators, used both as the initial library seed and as the final
  fallback when the network and cache both come up empty.

## What still needs to be confirmed

The exact `tcgapi.dev` response field names for card details, current price,
and price history are not publicly documented (only `/v1/search` has a
published example response) — those calls are implemented against the
documented paths and authentication, but their JSON parsing is written as an
adapter that tries a couple of plausible field names and falls back to mock
data if the shape doesn't match, so the firmware always compiles and runs
even before those details are pinned down. See the assumptions list in
`README.md` for the complete set of things to double-check before finalizing
pin mapping and the display driver.
