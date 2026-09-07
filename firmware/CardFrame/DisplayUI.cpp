#include "DisplayUI.h"
#include "Config.h"
#include "Keyboard.h"
#include "CardLibrary.h"
#include "WiFiManager.h"
#include "LedController.h"
#include <Wire.h>

namespace DisplayUI {

namespace {
  // Display driver selection lives entirely in this one block, per
  // Config.h's "isolate all display settings in one place" requirement.
  // MC242GX (SKU) is confirmed as a 128x64 SSD1309 OLED over I2C.
  U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, /*reset=*/U8X8_PIN_NONE);

  // ---- Home screen state ----
  HomeFocus homeFocus = HomeFocus::WIFI_ICON;
  PriceData currentPrice;
  PricePoint currentGraphPoints[MAX_GRAPH_POINTS];
  int currentGraphCount = 0;
  GraphRange currentGraphRangeUsed = GraphRange::DAY_7;
  DataSource currentGraphSource = DataSource::DEMO;

  // ---- Card library screen state ----
  int librarySelectedIndex = 0;
  bool libraryRemoveMode = false;
  int removeConfirmTargetIndex = -1;
  bool removeConfirmYesHighlighted = false;

  // ---- Add-card flow state ----
  bool pendingSearchFlag = false;
  String pendingSearchQuery;
  CardResult searchResults[MAX_SEARCH_RESULTS];
  int searchResultCount = 0;
  int searchResultSelected = 0;
  bool lastSearchSuccess = false;
  String lastSearchMessage;

  // ---- Wi-Fi flow state ----
  int wifiSelectedIndex = 0;
  String wifiChosenSsid;

  // ---- Graph range picker state ----
  GraphRange graphRangeTemp = GraphRange::DAY_7;

  // ---- LED settings state ----
  int ledSelectedRow = 0; // 0..(count of LedSetting)-1, last index = Back
  const int LED_ROW_COUNT = static_cast<int>(LedSetting::COUNT) + 1; // + Back row
  const LedEffect PALETTE_EFFECTS[5] = {
    LedEffect::OFF, LedEffect::SOLID, LedEffect::RAINBOW, LedEffect::COLOR_WIPE, LedEffect::BREATHING
  };
  // A small fixed palette for the COLOR setting, since picking a full RGB
  // value with a single-axis encoder is impractical. TODO: replace with an
  // R/G/B sub-editor if per-channel control is wanted later.
  struct PaletteColor { const char* name; uint8_t r, g, b; };
  const PaletteColor COLOR_PALETTE[8] = {
    {"Red", 255,0,0}, {"Orange",255,80,0}, {"Yellow",255,220,0}, {"Green",0,255,0},
    {"Cyan",0,255,255}, {"Blue",0,80,255}, {"Purple",160,0,255}, {"White",255,255,255}
  };
  int colorPaletteIndex = 1; // matches LedController's default amber-ish orange
  int effectIndex = 1;       // matches LedController's default SOLID

  // ---- Small drawing helpers ----

  void drawWifiIcon(int x, int y, WifiStatus status, bool highlighted) {
    if (highlighted) u8g2.drawFrame(x - 2, y - 10, 16, 13);
    switch (status) {
      case WifiStatus::CONNECTED:
        u8g2.drawStr(x, y, "W"); // simple glyph placeholder; swap for a bitmap icon if desired
        break;
      case WifiStatus::CONNECTING:
        u8g2.drawStr(x, y, "w?");
        break;
      case WifiStatus::ERROR:
        u8g2.drawStr(x, y, "W!");
        break;
      default:
        u8g2.drawStr(x, y, "Wx");
        break;
    }
  }

  void drawLedIcon(int x, int y, bool highlighted) {
    if (highlighted) u8g2.drawFrame(x - 2, y - 10, 16, 13);
    u8g2.drawStr(x, y, "LED");
  }

  void drawLibraryIcon(int x, int y, bool highlighted) {
    if (highlighted) u8g2.drawFrame(x - 2, y - 10, 16, 13);
    u8g2.drawStr(x, y, "LIB");
  }

  // Renders a small sparkline-style price graph in the given box.
  void drawGraph(int x, int y, int w, int h, bool highlighted, const PricePoint* points, int count, GraphRange range, DataSource source) {
    if (highlighted) u8g2.drawFrame(x - 1, y - 1, w + 2, h + 2);
    else u8g2.drawFrame(x, y, w, h);

    char header[24];
    snprintf(header, sizeof(header), "%s", graphRangeLabel(range));
    u8g2.drawStr(x + 2, y + 9, header);

    const char* tag = (source == DataSource::LIVE) ? "LIVE" : (source == DataSource::CACHED) ? "CACHED" : "DEMO";
    int tagWidth = u8g2.getStrWidth(tag);
    u8g2.drawStr(x + w - tagWidth - 2, y + 9, tag);

    if (count < 2) {
      u8g2.drawStr(x + 4, y + h / 2 + 4, "No data");
      return;
    }

    float minP = points[0].price, maxP = points[0].price;
    for (int i = 1; i < count; i++) {
      if (points[i].price < minP) minP = points[i].price;
      if (points[i].price > maxP) maxP = points[i].price;
    }
    if (maxP - minP < 0.01f) maxP = minP + 0.01f; // avoid divide-by-zero on a flat line

    int plotTop = y + 11;
    int plotBottom = y + h - 8;
    int plotHeight = plotBottom - plotTop;
    int plotLeft = x + 2;
    int plotWidth = w - 4;

    int prevX = -1, prevY = -1;
    for (int i = 0; i < count; i++) {
      int px = plotLeft + (int)((float)i / (float)(count - 1) * plotWidth);
      float norm = (points[i].price - minP) / (maxP - minP);
      int py = plotBottom - (int)(norm * plotHeight);
      if (prevX >= 0) u8g2.drawLine(prevX, prevY, px, py);
      prevX = px; prevY = py;
    }

    char minBuf[12], maxBuf[12];
    snprintf(minBuf, sizeof(minBuf), "$%.2f", minP);
    snprintf(maxBuf, sizeof(maxBuf), "$%.2f", maxP);
    u8g2.drawStr(x + 2, y + h - 1, minBuf);
    int maxW = u8g2.getStrWidth(maxBuf);
    u8g2.drawStr(x + w - maxW - 2, y + h - 1, maxBuf);
  }

  // ---- Screen: HOME ----
  void drawHome() {
    const CardData& card = CardLibrary::selectedCard();
    u8g2.setFont(u8g2_font_6x10_tf);

    if (card.valid) {
      u8g2.drawStr(0, 9, card.name.c_str());
      String meta = card.setName;
      if (card.collectorNumber.length()) { meta += " #"; meta += card.collectorNumber; }
      if (card.variant.length()) { meta += " ("; meta += card.variant; meta += ")"; }
      u8g2.drawStr(0, 19, meta.c_str());
    } else {
      u8g2.drawStr(0, 9, "No card selected");
      u8g2.drawStr(0, 19, "Open the library icon ->");
    }

    // Price line, honestly labeled per data source / availability.
    char priceLine[40];
    if (currentPrice.valid) {
      const char* tag = (currentPrice.source == DataSource::LIVE) ? "" :
                         (currentPrice.source == DataSource::CACHED) ? " (saved)" : " (demo)";
      snprintf(priceLine, sizeof(priceLine), "%.2f %s%s", currentPrice.price, currentPrice.currency, tag);
    } else {
      snprintf(priceLine, sizeof(priceLine), "Data Unavailable");
    }
    u8g2.drawStr(0, 29, priceLine);

    // Icon row: Wi-Fi | LED | Library
    drawWifiIcon(2, 40, WiFiManager::status(), homeFocus == HomeFocus::WIFI_ICON);
    drawLedIcon(30, 40, homeFocus == HomeFocus::LED_ICON);
    drawLibraryIcon(60, 40, homeFocus == HomeFocus::LIBRARY_ICON);

    // Graph, bottom portion of the screen.
    drawGraph(0, 44, 128, 20, homeFocus == HomeFocus::GRAPH,
              currentGraphPoints, currentGraphCount, currentGraphRangeUsed, currentGraphSource);
  }

  UIState handleHomeInput(int steps, bool pressed) {
    if (steps != 0) {
      int n = static_cast<int>(HomeFocus::COUNT);
      int cur = static_cast<int>(homeFocus);
      cur = (cur + steps) % n;
      if (cur < 0) cur += n;
      homeFocus = static_cast<HomeFocus>(cur);
    }
    if (pressed) {
      switch (homeFocus) {
        case HomeFocus::WIFI_ICON:    return UIState::WIFI_SCAN_LIST;
        case HomeFocus::LED_ICON:     return UIState::LED_SETTINGS_LIST;
        case HomeFocus::LIBRARY_ICON: return UIState::CARD_LIBRARY;
        case HomeFocus::GRAPH:        return UIState::GRAPH_RANGE_SELECT;
        default: break;
      }
    }
    return UIState::HOME;
  }

  // ---- Screen: CARD LIBRARY ----
  int libraryRowCount() {
    // Normal mode: [cards...] + Add Card + Remove Card + Back
    // Remove mode: [cards...] + Cancel
    return libraryRemoveMode ? CardLibrary::count() + 1 : CardLibrary::count() + 3;
  }

  void drawCardLibrary() {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 9, libraryRemoveMode ? "Select card to remove" : "Card Library");

    int count = CardLibrary::count();
    int total = libraryRowCount();
    // Show up to 4 rows, scrolled so the selection stays visible.
    int visibleRows = 4;
    int start = librarySelectedIndex - visibleRows / 2;
    if (start < 0) start = 0;
    if (start + visibleRows > total) start = total - visibleRows;
    if (start < 0) start = 0;

    int y = 20;
    for (int row = start; row < total && row < start + visibleRows; row++) {
      bool hl = (row == librarySelectedIndex);
      String label;
      if (row < count) {
        const CardData& c = CardLibrary::get(row);
        label = c.name + " (" + c.setName + ")";
      } else if (!libraryRemoveMode && row == count) {
        label = "+ Add Card";
      } else if (!libraryRemoveMode && row == count + 1) {
        label = "- Remove Card";
      } else {
        label = libraryRemoveMode ? "Cancel" : "Back";
      }
      if (hl) { u8g2.drawBox(0, y - 8, 128, 10); u8g2.setDrawColor(0); }
      u8g2.drawStr(2, y, label.c_str());
      if (hl) u8g2.setDrawColor(1);
      y += 11;
    }
  }

  UIState handleCardLibraryInput(int steps, bool pressed) {
    int total = libraryRowCount();
    if (steps != 0 && total > 0) {
      librarySelectedIndex = (librarySelectedIndex + steps) % total;
      if (librarySelectedIndex < 0) librarySelectedIndex += total;
    }
    if (!pressed) return UIState::CARD_LIBRARY;

    int count = CardLibrary::count();
    if (!libraryRemoveMode) {
      if (librarySelectedIndex < count) {
        // Selected an existing card: make it current and go Home.
        CardLibrary::setSelectedIndex(librarySelectedIndex);
        return UIState::HOME; // CardFrame.ino sees the selection changed and
                               // triggers a data refresh (see loop() there).
      } else if (librarySelectedIndex == count) {
        return UIState::ADD_CARD_KEYBOARD;
      } else if (librarySelectedIndex == count + 1) {
        libraryRemoveMode = true;
        librarySelectedIndex = 0;
        return UIState::CARD_LIBRARY;
      } else {
        return UIState::HOME; // Back
      }
    } else {
      if (librarySelectedIndex < count) {
        removeConfirmTargetIndex = librarySelectedIndex;
        removeConfirmYesHighlighted = false;
        return UIState::CARD_LIBRARY_REMOVE_CONFIRM;
      } else {
        libraryRemoveMode = false;
        librarySelectedIndex = 0;
        return UIState::CARD_LIBRARY;
      }
    }
  }

  // ---- Screen: REMOVE CONFIRM ----
  void drawRemoveConfirm() {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 9, "Remove this card?");
    if (removeConfirmTargetIndex >= 0 && removeConfirmTargetIndex < CardLibrary::count()) {
      u8g2.drawStr(0, 22, CardLibrary::get(removeConfirmTargetIndex).name.c_str());
    }
    int y = 40;
    bool yesHl = removeConfirmYesHighlighted;
    if (yesHl) { u8g2.drawBox(10, y - 8, 30, 10); u8g2.setDrawColor(0); }
    u8g2.drawStr(12, y, "Yes");
    if (yesHl) u8g2.setDrawColor(1);

    if (!yesHl) { u8g2.drawBox(60, y - 8, 34, 10); u8g2.setDrawColor(0); }
    u8g2.drawStr(62, y, "No");
    if (!yesHl) u8g2.setDrawColor(1);
  }

  UIState handleRemoveConfirmInput(int steps, bool pressed) {
    if (steps != 0) removeConfirmYesHighlighted = !removeConfirmYesHighlighted;
    if (pressed) {
      if (removeConfirmYesHighlighted && removeConfirmTargetIndex >= 0) {
        CardLibrary::removeCard(removeConfirmTargetIndex);
      }
      libraryRemoveMode = false;
      librarySelectedIndex = 0;
      return UIState::CARD_LIBRARY;
    }
    return UIState::CARD_LIBRARY_REMOVE_CONFIRM;
  }

  // ---- Screen: ADD CARD (keyboard) ----
  void drawAddCardKeyboard() {
    Keyboard::draw(u8g2, "Search tcgapi.dev");
  }

  UIState handleAddCardKeyboardInput(int steps, bool pressed) {
    if (steps != 0) Keyboard::handleEncoderSteps(steps);
    if (pressed) {
      Keyboard::Result r = Keyboard::handleButtonPress();
      if (r == Keyboard::Result::CONFIRMED) {
        if (Keyboard::text().length() > 0) {
          pendingSearchQuery = Keyboard::text();
          pendingSearchFlag = true;
          return UIState::ADD_CARD_SEARCHING;
        }
      } else if (r == Keyboard::Result::CANCELLED) {
        libraryRemoveMode = false;
        librarySelectedIndex = 0;
        return UIState::CARD_LIBRARY;
      }
    }
    return UIState::ADD_CARD_KEYBOARD;
  }

  void drawAddCardSearching() {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 20, "Searching tcgapi.dev...");
    u8g2.drawStr(0, 32, pendingSearchQuery.c_str());
  }

  // ---- Screen: ADD CARD (results) ----
  void drawAddCardResults() {
    u8g2.setFont(u8g2_font_6x10_tf);
    if (!lastSearchSuccess) {
      u8g2.drawStr(0, 9, "Search problem:");
      u8g2.drawStr(0, 20, lastSearchMessage.c_str());
      u8g2.drawStr(0, 40, "Press to go back");
      return;
    }
    if (searchResultCount == 0) {
      u8g2.drawStr(0, 9, "No results found");
      u8g2.drawStr(0, 40, "Press to go back");
      return;
    }
    u8g2.drawStr(0, 9, "Search results");
    int total = searchResultCount + 1; // + Back row
    int visibleRows = 4;
    int start = searchResultSelected - visibleRows / 2;
    if (start < 0) start = 0;
    if (start + visibleRows > total) start = total - visibleRows;
    if (start < 0) start = 0;
    int y = 20;
    for (int row = start; row < total && row < start + visibleRows; row++) {
      bool hl = (row == searchResultSelected);
      String label = (row < searchResultCount)
        ? (searchResults[row].name + " (" + searchResults[row].setName + ")")
        : String("Back");
      if (hl) { u8g2.drawBox(0, y - 8, 128, 10); u8g2.setDrawColor(0); }
      u8g2.drawStr(2, y, label.c_str());
      if (hl) u8g2.setDrawColor(1);
      y += 11;
    }
  }

  UIState handleAddCardResultsInput(int steps, bool pressed) {
    if (!lastSearchSuccess || searchResultCount == 0) {
      if (pressed) { libraryRemoveMode = false; librarySelectedIndex = 0; return UIState::CARD_LIBRARY; }
      return UIState::ADD_CARD_RESULTS;
    }
    int total = searchResultCount + 1;
    if (steps != 0) {
      searchResultSelected = (searchResultSelected + steps) % total;
      if (searchResultSelected < 0) searchResultSelected += total;
    }
    if (pressed) {
      if (searchResultSelected < searchResultCount) {
        const CardResult& r = searchResults[searchResultSelected];
        CardData newCard;
        newCard.cardId = r.cardId;
        newCard.name = r.name;
        newCard.setName = r.setName;
        newCard.collectorNumber = r.number;
        newCard.valid = true;
        CardLibrary::addCard(newCard);
      }
      librarySelectedIndex = 0;
      libraryRemoveMode = false;
      return UIState::CARD_LIBRARY;
    }
    return UIState::ADD_CARD_RESULTS;
  }

  // ---- Screen: WIFI SCAN LIST ----
  void drawWifiScanList() {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 9, "Wi-Fi Networks");
    if (!WiFiManager::isScanComplete()) {
      u8g2.drawStr(0, 24, "Scanning...");
      return;
    }
    int count = WiFiManager::scanResultCount();
    int total = count + 1; // + Back
    int visibleRows = 4;
    int start = wifiSelectedIndex - visibleRows / 2;
    if (start < 0) start = 0;
    if (start + visibleRows > total) start = total - visibleRows;
    if (start < 0) start = 0;
    int y = 20;
    for (int row = start; row < total && row < start + visibleRows; row++) {
      bool hl = (row == wifiSelectedIndex);
      String label;
      if (row < count) {
        label = WiFiManager::scanResultSSID(row);
        label += WiFiManager::scanResultIsSecure(row) ? " [lock]" : " [open]";
      } else {
        label = "Back";
      }
      if (hl) { u8g2.drawBox(0, y - 8, 128, 10); u8g2.setDrawColor(0); }
      u8g2.drawStr(2, y, label.c_str());
      if (hl) u8g2.setDrawColor(1);
      y += 11;
    }
  }

  UIState handleWifiScanListInput(int steps, bool pressed) {
    if (!WiFiManager::isScanComplete()) return UIState::WIFI_SCAN_LIST; // ignore input mid-scan
    int count = WiFiManager::scanResultCount();
    int total = count + 1;
    if (steps != 0 && total > 0) {
      wifiSelectedIndex = (wifiSelectedIndex + steps) % total;
      if (wifiSelectedIndex < 0) wifiSelectedIndex += total;
    }
    if (pressed) {
      if (wifiSelectedIndex < count) {
        wifiChosenSsid = WiFiManager::scanResultSSID(wifiSelectedIndex);
        if (WiFiManager::scanResultIsSecure(wifiSelectedIndex)) {
          return UIState::WIFI_PASSWORD_KEYBOARD;
        } else {
          WiFiManager::beginConnect(wifiChosenSsid, "");
          return UIState::WIFI_CONNECTING;
        }
      } else {
        return UIState::HOME;
      }
    }
    return UIState::WIFI_SCAN_LIST;
  }

  // ---- Screen: WIFI PASSWORD KEYBOARD ----
  void drawWifiPasswordKeyboard() {
    Keyboard::draw(u8g2, "Enter Wi-Fi password");
  }

  UIState handleWifiPasswordKeyboardInput(int steps, bool pressed) {
    if (steps != 0) Keyboard::handleEncoderSteps(steps);
    if (pressed) {
      Keyboard::Result r = Keyboard::handleButtonPress();
      if (r == Keyboard::Result::CONFIRMED) {
        WiFiManager::beginConnect(wifiChosenSsid, Keyboard::text());
        return UIState::WIFI_CONNECTING;
      } else if (r == Keyboard::Result::CANCELLED) {
        return UIState::WIFI_SCAN_LIST;
      }
    }
    return UIState::WIFI_PASSWORD_KEYBOARD;
  }

  void drawWifiConnecting() {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 20, "Connecting to:");
    u8g2.drawStr(0, 32, wifiChosenSsid.c_str());
    WifiStatus s = WiFiManager::status();
    if (s == WifiStatus::CONNECTED) u8g2.drawStr(0, 50, "Connected!");
    else if (s == WifiStatus::ERROR) u8g2.drawStr(0, 50, "Failed - press to retry");
    else u8g2.drawStr(0, 50, "Please wait...");
  }

  // While connecting, no encoder/button input is consulted UNLESS the
  // attempt has already finished (success or failure) - then a button press
  // dismisses the screen and returns Home. CardFrame.ino also auto-returns
  // Home a couple of seconds after success even with no button press (see
  // the main loop), so the user is never stuck staring at "Connected!".
  UIState handleWifiConnectingInput(int steps, bool pressed) {
    (void)steps;
    if (pressed && WiFiManager::isConnectAttemptFinished()) {
      return (WiFiManager::status() == WifiStatus::CONNECTED) ? UIState::HOME : UIState::WIFI_SCAN_LIST;
    }
    return UIState::WIFI_CONNECTING;
  }

  // ---- Screen: GRAPH RANGE SELECT ----
  void drawGraphRangeSelect() {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 9, "Graph Time Range");
    int y = 22;
    for (int i = 0; i < static_cast<int>(GraphRange::COUNT); i++) {
      bool hl = (i == static_cast<int>(graphRangeTemp));
      if (hl) { u8g2.drawBox(0, y - 8, 128, 10); u8g2.setDrawColor(0); }
      u8g2.drawStr(2, y, graphRangeLabel(static_cast<GraphRange>(i)));
      if (hl) u8g2.setDrawColor(1);
      y += 11;
    }
  }

  UIState handleGraphRangeSelectInput(int steps, bool pressed) {
    int n = static_cast<int>(GraphRange::COUNT);
    if (steps != 0) {
      int cur = (static_cast<int>(graphRangeTemp) + steps) % n;
      if (cur < 0) cur += n;
      graphRangeTemp = static_cast<GraphRange>(cur);
    }
    if (pressed) {
      currentGraphRangeUsed = graphRangeTemp;
      return UIState::HOME; // CardFrame.ino requests fresh data for this range
    }
    return UIState::GRAPH_RANGE_SELECT;
  }

  // ---- Screen: LED SETTINGS LIST ----
  const char* ledRowName(int row) {
    switch (row) {
      case 0: return "Power";
      case 1: return "Brightness";
      case 2: return "Color";
      case 3: return "Effect";
      case 4: return "Speed";
      default: return "Back";
    }
  }

  void drawLedSettingsList() {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 9, "LED Settings");
    int y = 20;
    for (int row = 0; row < LED_ROW_COUNT; row++) {
      bool hl = (row == ledSelectedRow);
      String label = ledRowName(row);
      if (row == 0) label += LedController::getPower() ? ": On" : ": Off";
      else if (row == 1) { label += ": "; label += String(LedController::getBrightness()); }
      else if (row == 2) { label += ": "; label += COLOR_PALETTE[colorPaletteIndex].name; }
      else if (row == 3) { label += ": "; label += ledEffectLabel(LedController::getEffect()); }
      else if (row == 4) { label += ": "; label += String(LedController::getSpeed()); }

      if (hl) { u8g2.drawBox(0, y - 8, 128, 10); u8g2.setDrawColor(0); }
      u8g2.drawStr(2, y, label.c_str());
      if (hl) u8g2.setDrawColor(1);
      y += 11;
    }
  }

  UIState handleLedSettingsListInput(int steps, bool pressed) {
    if (steps != 0) {
      ledSelectedRow = (ledSelectedRow + steps) % LED_ROW_COUNT;
      if (ledSelectedRow < 0) ledSelectedRow += LED_ROW_COUNT;
    }
    if (pressed) {
      if (ledSelectedRow == LED_ROW_COUNT - 1) return UIState::HOME; // Back row
      return UIState::LED_SETTINGS_EDIT;
    }
    return UIState::LED_SETTINGS_LIST;
  }

  // ---- Screen: LED SETTINGS EDIT ----
  void drawLedSettingsEdit() {
    u8g2.setFont(u8g2_font_6x10_tf);
    String title = String("Edit: ") + ledRowName(ledSelectedRow);
    u8g2.drawStr(0, 9, title.c_str());

    switch (ledSelectedRow) {
      case 0: u8g2.drawStr(0, 30, LedController::getPower() ? "On" : "Off"); break;
      case 1: u8g2.drawStr(0, 30, String(LedController::getBrightness()).c_str()); break;
      case 2: u8g2.drawStr(0, 30, COLOR_PALETTE[colorPaletteIndex].name); break;
      case 3: u8g2.drawStr(0, 30, ledEffectLabel(static_cast<LedEffect>(PALETTE_EFFECTS[effectIndex]))); break;
      case 4: u8g2.drawStr(0, 30, String(LedController::getSpeed()).c_str()); break;
    }
    u8g2.drawStr(0, 55, "Rotate to change, press to save");
  }

  UIState handleLedSettingsEditInput(int steps, bool pressed) {
    switch (ledSelectedRow) {
      case 0: // Power
        if (steps != 0) LedController::setPower(!LedController::getPower());
        break;
      case 1: { // Brightness
        if (steps != 0) {
          int b = (int)LedController::getBrightness() + steps * 8;
          if (b < 0) b = 0; if (b > LED_MAX_BRIGHTNESS) b = LED_MAX_BRIGHTNESS;
          LedController::setBrightness((uint8_t)b);
        }
        break;
      }
      case 2: { // Color (palette cycle)
        if (steps != 0) {
          int n = 8;
          colorPaletteIndex = (colorPaletteIndex + steps) % n;
          if (colorPaletteIndex < 0) colorPaletteIndex += n;
          LedController::setColor(COLOR_PALETTE[colorPaletteIndex].r, COLOR_PALETTE[colorPaletteIndex].g, COLOR_PALETTE[colorPaletteIndex].b);
        }
        break;
      }
      case 3: { // Effect
        if (steps != 0) {
          int n = 5;
          effectIndex = (effectIndex + steps) % n;
          if (effectIndex < 0) effectIndex += n;
          LedController::setEffect(PALETTE_EFFECTS[effectIndex]);
        }
        break;
      }
      case 4: { // Speed
        if (steps != 0) {
          int s = (int)LedController::getSpeed() + steps * 8;
          if (s < 0) s = 0; if (s > 255) s = 255;
          LedController::setSpeed((uint8_t)s);
        }
        break;
      }
    }
    if (pressed) {
      LedController::saveSettings();
      return UIState::LED_SETTINGS_LIST;
    }
    return UIState::LED_SETTINGS_EDIT;
  }

} // anonymous namespace

void begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  u8g2.setI2CAddress(DISPLAY_I2C_ADDR << 1);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  colorPaletteIndex = 1;
  effectIndex = static_cast<int>(LedController::getEffect());
  if (effectIndex < 0 || effectIndex > 4) effectIndex = 1;
}

void onEnterState(UIState state) {
  switch (state) {
    case UIState::CARD_LIBRARY:
      libraryRemoveMode = false;
      librarySelectedIndex = 0;
      break;
    case UIState::ADD_CARD_KEYBOARD:
      Keyboard::begin(/*masked=*/false);
      break;
    case UIState::WIFI_SCAN_LIST:
      WiFiManager::startScan();
      wifiSelectedIndex = 0;
      break;
    case UIState::WIFI_PASSWORD_KEYBOARD:
      Keyboard::begin(/*masked=*/true);
      break;
    case UIState::GRAPH_RANGE_SELECT:
      graphRangeTemp = currentGraphRangeUsed;
      break;
    case UIState::LED_SETTINGS_LIST:
      ledSelectedRow = 0;
      break;
    default:
      break;
  }
}

UIState handleInput(UIState state, int encoderSteps, bool buttonPressed) {
  switch (state) {
    case UIState::HOME:                        return handleHomeInput(encoderSteps, buttonPressed);
    case UIState::CARD_LIBRARY:                return handleCardLibraryInput(encoderSteps, buttonPressed);
    case UIState::CARD_LIBRARY_REMOVE_CONFIRM: return handleRemoveConfirmInput(encoderSteps, buttonPressed);
    case UIState::ADD_CARD_KEYBOARD:           return handleAddCardKeyboardInput(encoderSteps, buttonPressed);
    case UIState::ADD_CARD_SEARCHING:          return UIState::ADD_CARD_SEARCHING; // no input while waiting
    case UIState::ADD_CARD_RESULTS:            return handleAddCardResultsInput(encoderSteps, buttonPressed);
    case UIState::WIFI_SCAN_LIST:              return handleWifiScanListInput(encoderSteps, buttonPressed);
    case UIState::WIFI_PASSWORD_KEYBOARD:      return handleWifiPasswordKeyboardInput(encoderSteps, buttonPressed);
    case UIState::WIFI_CONNECTING:             return handleWifiConnectingInput(encoderSteps, buttonPressed);
    case UIState::GRAPH_RANGE_SELECT:          return handleGraphRangeSelectInput(encoderSteps, buttonPressed);
    case UIState::LED_SETTINGS_LIST:           return handleLedSettingsListInput(encoderSteps, buttonPressed);
    case UIState::LED_SETTINGS_EDIT:           return handleLedSettingsEditInput(encoderSteps, buttonPressed);
    default:                                   return state;
  }
}

void draw(UIState state) {
  u8g2.clearBuffer();
  switch (state) {
    case UIState::HOME:                        drawHome(); break;
    case UIState::CARD_LIBRARY:                drawCardLibrary(); break;
    case UIState::CARD_LIBRARY_REMOVE_CONFIRM: drawRemoveConfirm(); break;
    case UIState::ADD_CARD_KEYBOARD:           drawAddCardKeyboard(); break;
    case UIState::ADD_CARD_SEARCHING:          drawAddCardSearching(); break;
    case UIState::ADD_CARD_RESULTS:            drawAddCardResults(); break;
    case UIState::WIFI_SCAN_LIST:              drawWifiScanList(); break;
    case UIState::WIFI_PASSWORD_KEYBOARD:      drawWifiPasswordKeyboard(); break;
    case UIState::WIFI_CONNECTING:             drawWifiConnecting(); break;
    case UIState::GRAPH_RANGE_SELECT:          drawGraphRangeSelect(); break;
    case UIState::LED_SETTINGS_LIST:           drawLedSettingsList(); break;
    case UIState::LED_SETTINGS_EDIT:           drawLedSettingsEdit(); break;
  }
  u8g2.sendBuffer();
}

void setCurrentPrice(const PriceData& p) { currentPrice = p; }

void setCurrentGraph(const PricePoint* points, int count, GraphRange range, DataSource source) {
  if (count > MAX_GRAPH_POINTS) count = MAX_GRAPH_POINTS;
  for (int i = 0; i < count; i++) currentGraphPoints[i] = points[i];
  currentGraphCount = count;
  currentGraphRangeUsed = range;
  currentGraphSource = source;
}

void setSearchResults(const CardResult* results, int count, bool success, const char* statusMessage) {
  if (count > MAX_SEARCH_RESULTS) count = MAX_SEARCH_RESULTS;
  for (int i = 0; i < count; i++) searchResults[i] = results[i];
  searchResultCount = count;
  searchResultSelected = 0;
  lastSearchSuccess = success;
  lastSearchMessage = statusMessage ? statusMessage : "";
}

bool consumePendingSearch(String& outQuery) {
  if (!pendingSearchFlag) return false;
  outQuery = pendingSearchQuery;
  pendingSearchFlag = false;
  return true;
}

GraphRange currentGraphRange() {
  return currentGraphRangeUsed;
}

} // namespace DisplayUI
