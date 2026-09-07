# CardFrame — Build & Flash Guide

Firmware for a Wi-Fi trading-card display frame on an ESP32-S3. See
`PROJECT_SUMMARY.md` for what the project does; this document covers how to
build, wire, and flash it.

## 1. Required Arduino libraries

Install these through **Arduino IDE → Tools → Manage Libraries...** (Library
Manager), searching by the exact name below. Versions shown are what this
firmware was written and compiled against; newer minor/patch versions should
also work.

| Library | Version used | Search name in Library Manager |
|---|---|---|
| ArduinoJson | 7.4.3 | `ArduinoJson` (by Benoit Blanchon) |
| U8g2 | 2.36.19 | `U8g2` (by oliver) |
| FastLED | 3.10.5 | `FastLED` (by Daniel Garcia) |

`Preferences`, `WiFi`, `WiFiClientSecure`, and `HTTPClient` are already
bundled with the ESP32 board package (installed in the next step) — no
separate install needed.

## 2. Install the ESP32 board package

1. Arduino IDE → **File → Preferences** → "Additional Boards Manager URLs" →
   add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. **Tools → Board → Boards Manager...** → search "esp32" → install
   **esp32 by Espressif Systems** (this firmware was built/tested against
   version 3.3.11).
3. **Tools → Board** → select **ESP32S3 Dev Module** (or your specific
   board's ESP32-S3 entry, e.g. "ESP32-S3-DevKitC-1").

## 3. Flashing over USB-C

1. Connect the board's USB-C port to your computer.
2. **Tools → Port** → select the port that appears (on Linux/macOS something
   like `/dev/ttyACM0` or `/dev/cu.usbmodem...`; on Windows a `COM` port).
   If no port appears, install the CP210x/CH340 USB-serial driver your board
   needs (check its product page) and reconnect.
3. Recommended **Tools** settings for a plain ESP32-S3 dev board:
   - USB CDC On Boot: **Enabled** (lets Serial output show up over the same
     USB-C cable without a separate UART adapter)
   - Upload Speed: **921600** (drop to 115200 if you get upload errors)
   - Flash Mode: **QIO**, Flash Size: **4MB** (match your board; check its
     silkscreen/datasheet if unsure)
   - Partition Scheme: **Default 4MB with spiffs** — this firmware compiles
     to about 91% of that scheme's 1.2MB app partition, so it fits, but
     there isn't much headroom left. If you add more features later and hit
     "sketch too big", switch to **Huge APP (3MB No OTA/1MB SPIFFS)**.
   - USB Mode: **Hardware CDC and JTAG** (the default on most boards)
4. Click **Upload** (the arrow icon), or **Sketch → Upload**.
5. **If the upload fails to start** (a common ESP32-S3 quirk on some boards):
   hold the **BOOT** button, tap **RESET** once while still holding BOOT,
   release BOOT, then click Upload again. Once the IDE prints "Connecting...."
   and starts writing, you can let go.
6. After a successful upload, open **Tools → Serial Monitor** at **115200
   baud** to see startup logs (Wi-Fi status, refresh timing, etc. — never
   passwords or API keys, see the security note in Config.h).

## 4. Wiring table

All pin numbers below are the current values in `Config.h` — change them
there (not anywhere else in the firmware) if your board's layout needs
different GPIOs. They were chosen to avoid ESP32-S3 strapping pins
(GPIO0/45/46) and the native-USB pins (GPIO19/20).

| Signal | ESP32-S3 GPIO | Connects to |
|---|---|---|
| I2C SDA | GPIO8 | MC242GX display SDA |
| I2C SCL | GPIO9 | MC242GX display SCL |
| Encoder CLK (A) | GPIO4 | KY-040 `CLK` |
| Encoder DT (B) | GPIO5 | KY-040 `DT` |
| Momentary button | GPIO6 | SW_Push, other leg to GND (uses internal pull-up) |
| WS2812B data (pre-buffer) | GPIO7 | 74AHCT14 input (pin 1) — **not** straight to the LED strip |

The KY-040's own `SW` pin (its built-in push switch) is intentionally left
unconnected — all select/confirm actions use the separate SW_Push button
per the project spec.

## 5. Power architecture (text diagram)

```
[18650 Li-ion cell, PROTECTED — see safety note below]
        |  (+)
        v
   [KCD01 SPST switch]  -- switches the battery's positive leg only
        |
        v
   [5V boost converter]  IN+ / IN-  <-- battery (-) also goes to boost IN-
        |  OUT+ (regulated 5V)              and becomes the system's common
        |                                    ground reference
        +------------------------------------------------+
        |                                                 |
        v                                                 v
 [ESP32-S3 5V/VIN pin]                        [WS2812B strip 5V input]
   (NOT 3.3V pin — feed the                     with a 1000uF capacitor
    board's 5V input so its                     across 5V/GND right at
    onboard regulator makes 3.3V                the first LED (observe
    for the chip itself)                        capacitor polarity!)

 Common GND runs from the boost converter's OUT- to: ESP32-S3 GND, the
 MC242GX display GND, the KY-040 GND, the SW_Push's GND leg, the 74AHCT14
 GND (pin 7), and the WS2812B strip GND. Every device shares one ground.
```

WS2812B power comes directly from the regulated 5V boost output — never
from the ESP32-S3's own 5V or 3.3V pin, which cannot supply the current a
full LED strip needs (see the current budget below).

## 6. 74AHCT14 level-shifter wiring (WS2812B data line)

The ESP32-S3 drives its GPIOs at 3.3V logic, but WS2812B strips are
specified against a ~5V-referenced signal (they often work at 3.3V logic
for a short data trace, but this gets unreliable as wire length, LED count,
or noise increase — hence the buffer stage the BOM already calls for).

The 74AHCT14 is a **hex inverting Schmitt-trigger buffer**: every gate
inverts its input, so passing a signal through **one** gate would flip its
polarity (idle-high instead of idle-low). WS2812B's protocol encodes bits by
pulse width, not by absolute polarity, and its receivers are self-clocked
from those pulse edges — so a single inversion typically still "works" on
many strips because each inverted pulse's width is preserved. **This
firmware instead uses two gates in series to restore true non-inverted
logic**, which is the safer, protocol-correct choice and avoids relying on a
receiver's tolerance for inverted timing:

```
ESP32-S3 GPIO7 (3.3V logic)
        |
        v
   74AHCT14 pin 1 (1A, input)  ---->  pin 2 (1Y, output — INVERTED, ~5V logic)
                                            |
                                            v
                                  74AHCT14 pin 3 (2A, input)  ---->  pin 4 (2Y, output — NON-INVERTED again, ~5V logic)
                                                                          |
                                                                          v
                                                                  [330 ohm resistor]
                                                                          |
                                                                          v
                                                              WS2812B strip DIN (first LED)
```

Supporting connections on the 74AHCT14 (standard 14-pin DIP/SOIC pinout):

- Pin 14 (`VCC`) → regulated 5V rail (same rail powering the LED strip)
- Pin 7 (`GND`) → common ground
- Add a 0.1uF ceramic decoupling capacitor directly across pins 14 and 7,
  as close to the chip as possible — standard practice for any logic IC and
  cheap insurance against glitches on the LED data line.
- The chip has six inverter gates total; this design only uses two (gates 1
  and 2). Tie every **unused input** pin (5, 9, 11, 13) to GND so those
  unused gates don't float and draw extra current or inject noise — leave
  their corresponding outputs (6, 8, 10, 12) unconnected.

If you'd rather use only a single inverter stage (simpler wiring, one fewer
jumper), that is a documented valid alternative for WS2812B specifically —
just route GPIO7 into one gate's input and that gate's output straight to
the 330 ohm resistor and DIN. Two stages is the choice made in this design
because it is protocol-correct for any addressable LED that isn't as
timing-tolerant as WS2812B usually is (e.g. if you ever swap to a stricter
variant), and because it makes the signal's logic sense ("HIGH means HIGH")
obvious to anyone reading the wiring later.

The 1000uF capacitor goes directly across the WS2812B strip's 5V and GND
pins, right at the first LED — **check its polarity**, electrolytic
capacitors are polarized and installing one backwards can damage it (or
worse). The 330 ohm resistor goes in series with the data line right before
DIN on the first LED, as shown above — it protects that first LED's input
from voltage spikes without meaningfully affecting signal timing at these
lengths.

## 7. Battery safety — read before wiring the 18650

An unprotected 18650 cell should **not** be connected directly to a boost
converter. Unprotected cells have no built-in defense against
over-discharge, over-current, or (if you ever add charging) overcharge, and
a boost converter will happily keep pulling current from a cell well past
its safe minimum voltage, which degrades or damages Li-ion cells and is a
fire-safety concern.

Use one of these instead:

- A **protected 18650 cell** (has a small protection PCB built into the
  cell itself, usually a couple mm longer than an unprotected cell) — the
  simplest fix, works with the wiring diagram above unchanged.
- An unprotected cell paired with a small **battery-management/charging
  module** that includes undervoltage (over-discharge) cutoff — e.g. a
  TP4056-with-protection module (look specifically for a variant that
  includes the protection IC, not just the bare charger chip) placed
  between the cell and the KCD01 switch.

Either way, undervoltage protection should sit between the raw cell and the
boost converter's input.

## 8. LED current budget

Config.h defines `LED_COUNT` (default **24**, change it to your strip's
actual length) as the single source of truth for this math:

- Each WS2812B draws up to **~60 mA at full brightness, pure white** (all
  three color channels at maximum).
- 24 LEDs x 60 mA = **1440 mA (1.44 A)** worst case for the strip alone.
- ESP32-S3 under active Wi-Fi use: budget **~500 mA**.
- MC242GX display: **52 mA max** per its specification.
- Total worst case: 1440 + 500 + 52 = **~1992 mA (about 2.0 A)**.

**Recommendation: choose a 5V boost converter rated for at least 2A
continuous output**, ideally with some headroom above that (2.5–3A) for
safety margin and to avoid running it at its absolute limit continuously.
If you set `LED_COUNT` higher, re-run this math (`LED_COUNT x 60mA + 500mA +
52mA`) before picking a converter.

In practice the firmware's default brightness (128/255, about 50%) and the
fact that few effects show pure white on every pixel simultaneously mean
typical current draw is well under this worst case — but the *boost
converter and any fuse/wiring* should be sized for the worst case, not the
typical case.

## 9. Assumptions to confirm

These are called out as `>>> ASSUMPTION` comments in `Config.h` /
`TcgApiClient.cpp` / `TcgApiClient.h` as well, collected here for a quick
pre-flight check:

1. **MC242GX display I2C address** — defaults to `0x3C` (the more common
   factory configuration). If the screen stays blank, check the address
   solder-pad on the back of your specific unit and change
   `DISPLAY_I2C_ADDR` to `0x3D` if needed.
2. **GPIO pin assignments** (I2C, encoder, button, LED data) — chosen to be
   safe defaults for a generic ESP32-S3 dev board; confirm against your
   specific board's pinout diagram, especially if using a compact variant
   (XIAO ESP32S3, ESP32-S3-Zero, etc.) which may reserve some of these pins
   for other functions.
3. **`LED_COUNT = 24`** — set this to your actual strip length before
   flashing; it drives both the animation code and the current-budget math
   above.
4. **tcgapi.dev response field names** — CONFIRMED on 2026-09-06 against a
   live, authenticated key for all four endpoints this firmware calls
   (`/v1/search`, `/v1/cards/:id`, `/v1/cards/:id/prices`,
   `/v1/cards/:id/history`). `TcgApiClient.cpp` parses the real field names
   observed (e.g. `market_price`, `set_name`, `meta.total`), not guesses —
   see the comment above each function there for the exact response shape.
   Two related things this testing also uncovered, both already handled in
   code: the prices/history endpoints return one row per card *printing*
   (Normal, Reverse Holofoil, ...) rather than a single value, and an
   invalid/missing key returns HTTP 402 rather than 401. If tcgapi.dev
   changes its schema in the future, these are the functions to revisit.
5. **TLS certificate validation** — `TcgApiClient.cpp` currently calls
   `tlsClient.setInsecure()` as a bring-up shortcut so HTTPS works without
   needing a pinned certificate right away. Before relying on this in the
   field, replace it with `tlsClient.setCACert(...)` using tcgapi.dev's
   actual CA certificate for proper certificate validation.
6. **Price-history availability** — tcgapi.dev's `/v1/cards/:id/history`
   endpoint requires a paid (Hobby+) API tier; a free-tier key gets a 403
   here by design, and the firmware treats that as a normal fallback to
   demo data rather than an error state. CONFIRMED by live testing: this
   endpoint ignores any range/window query parameter and always returns a
   fixed, tier-determined window of recent days — it cannot be asked for
   "30 days" vs "1 year" of history. The firmware checks how much of the
   on-screen graph range that fixed window actually covers, and falls back
   to demo data (clearly labeled) whenever it's too little to be honest —
   so on most free/Hobby keys, only the shortest graph range(s) will show
   real data and the rest will show demo data. A Pro-tier key unlocks
   `/v1/cards/:id/history/detailed` for deeper history, which this firmware
   does not currently call.
7. **Boost converter and protected-cell/BMS part numbers** — the BOM lists
   generic roles ("5V boost converter", "3.7V 18650") rather than specific
   part numbers; use the current-budget math in section 8 and the safety
   note in section 7 to pick specific parts.
8. **API key security** — `Config.h` in this build now contains a real,
   working tcgapi.dev key. Do not publish this file (e.g. to a public
   GitHub repo) without swapping `TCGAPI_API_KEY` back to a placeholder,
   and consider rotating the key on tcgapi.dev if it's ever exposed.

## 10. Source

Card and pricing data: [tcgapi.dev](https://tcgapi.dev/) — see
[tcgapi.dev/authentication/](https://tcgapi.dev/authentication/) and
[tcgapi.dev/quickstart/](https://tcgapi.dev/quickstart/) for the current API
documentation.
Display driver reference: MC242GX specification, LCDWIKI
([product page](https://www.lcdwiki.com/2.42inch_IIC_OLED_Module_SKU:MC242GX),
[specification PDF](https://www.lcdwiki.com/res/MC242GX/MC242GX_Specification_EN_V1.0.pdf)).
