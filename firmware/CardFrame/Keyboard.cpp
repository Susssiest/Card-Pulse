#include "Keyboard.h"
#include "Config.h"
#include "Types.h"

namespace Keyboard {

namespace {
  // ---- Layout data ----
  const char* ROW_UPPER   = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const char* ROW_LOWER   = "abcdefghijklmnopqrstuvwxyz";
  const char* ROW_DIGITS  = "0123456789";
  const char* ROW_SYMBOLS = "!@#$%^&*()-_=+[]{}:;,.?/";

  // Special-key row: fixed labels. Index meaning is fixed (0=DEL,1=SPACE,
  //2=CONFIRM,3=CANCEL) and referenced directly in handleButtonPress().
  const char* SPECIAL_LABELS[4] = { "DEL", "SPACE", "OK", "CANCEL" };
  const int SPECIAL_COUNT = 4;

  int rowLengths[5]; // 4 char rows + 1 special row, filled in begin()
  const char* ROW_STRS[4]; // pointers to the 4 char-row strings

  int flatCursor = 0;   // single linear cursor across all rows, flattened
  int totalKeys = 0;

  bool isMasked = false;
  String buffer;

  void computeLayout() {
    ROW_STRS[0] = ROW_UPPER;
    ROW_STRS[1] = ROW_LOWER;
    ROW_STRS[2] = ROW_DIGITS;
    ROW_STRS[3] = ROW_SYMBOLS;
    rowLengths[0] = strlen(ROW_UPPER);
    rowLengths[1] = strlen(ROW_LOWER);
    rowLengths[2] = strlen(ROW_DIGITS);
    rowLengths[3] = strlen(ROW_SYMBOLS);
    rowLengths[4] = SPECIAL_COUNT;
    totalKeys = rowLengths[0] + rowLengths[1] + rowLengths[2] + rowLengths[3] + rowLengths[4];
  }

  // Converts the flattened cursor into (row, col).
  void unflatten(int flat, int& row, int& col) {
    for (int r = 0; r < 5; r++) {
      if (flat < rowLengths[r]) { row = r; col = flat; return; }
      flat -= rowLengths[r];
    }
    row = 4; col = 0; // shouldn't happen; safe fallback
  }

  // Returns a short label for the currently highlighted key (single char
  // rows return a 1-character C string via `outChar`; the special row
  // returns a multi-character label string).
  void currentKeyLabel(char* outChar, const char** outLabel) {
    int row, col;
    unflatten(flatCursor, row, col);
    if (row < 4) {
      outChar[0] = ROW_STRS[row][col];
      outChar[1] = '\0';
      *outLabel = outChar;
    } else {
      *outLabel = SPECIAL_LABELS[col];
    }
  }
}

void begin(bool masked) {
  computeLayout();
  flatCursor = 0;
  isMasked = masked;
  buffer = "";
}

void handleEncoderSteps(int steps) {
  if (steps == 0 || totalKeys == 0) return;
  flatCursor = (flatCursor + steps) % totalKeys;
  if (flatCursor < 0) flatCursor += totalKeys;
}

Result handleButtonPress() {
  int row, col;
  unflatten(flatCursor, row, col);

  if (row < 4) {
    // A normal character key.
    if (buffer.length() < MAX_TEXT_LEN) {
      buffer += ROW_STRS[row][col];
    }
    return Result::NONE;
  }

  // Special-key row.
  switch (col) {
    case 0: // DEL
      if (buffer.length() > 0) buffer.remove(buffer.length() - 1);
      return Result::NONE;
    case 1: // SPACE
      if (buffer.length() < MAX_TEXT_LEN) buffer += ' ';
      return Result::NONE;
    case 2: // OK / CONFIRM
      return Result::CONFIRMED;
    case 3: // CANCEL
      return Result::CANCELLED;
    default:
      return Result::NONE;
  }
}

const String& text() { return buffer; }

void draw(U8G2& d, const char* caption) {
  d.setFont(u8g2_font_6x10_tf);
  d.drawStr(0, 8, caption);

  // Typed text field (masked as bullets if this is a password keyboard).
  String shown;
  if (isMasked) {
    for (size_t i = 0; i < buffer.length(); i++) shown += '*';
  } else {
    shown = buffer;
  }
  // Keep only the tail visible if it's too long for the screen width.
  const int maxVisibleChars = 20;
  if ((int)shown.length() > maxVisibleChars) {
    shown = shown.substring(shown.length() - maxVisibleChars);
  }
  d.drawFrame(0, 11, 128, 12);
  d.drawStr(3, 20, shown.c_str());

  // Keyboard grid: one row of the active character row is shown at a time
  // (the row containing the current cursor), plus the special-key row
  // always shown at the bottom, so the display stays readable at 128x64.
  int curRow, curCol;
  unflatten(flatCursor, curRow, curCol);
  int displayRow = (curRow < 4) ? curRow : 0; // if on special row, still show row 0 chars above it for context

  int y = 34;
  if (curRow < 4) {
    const char* rowStr = ROW_STRS[curRow];
    int len = rowLengths[curRow];
    // Show a sliding window of ~16 chars centered on the cursor.
    const int windowSize = 16;
    int start = curCol - windowSize / 2;
    if (start < 0) start = 0;
    if (start + windowSize > len) start = len - windowSize;
    if (start < 0) start = 0;

    int x = 2;
    for (int i = start; i < len && i < start + windowSize; i++) {
      bool hl = (i == curCol);
      char label[2] = { rowStr[i], '\0' };
      if (hl) d.drawBox(x - 1, y - 8, 7, 10);
      d.setDrawColor(hl ? 0 : 1);
      d.drawStr(x, y, label);
      d.setDrawColor(1);
      x += 7;
    }
  } else {
    d.drawStr(2, y, "(scroll to a letter row)");
  }

  // Special-key row, always visible at the bottom.
  int sy = 50;
  int sx = 2;
  for (int i = 0; i < SPECIAL_COUNT; i++) {
    bool hl = (curRow == 4 && curCol == i);
    int w = strlen(SPECIAL_LABELS[i]) * 6 + 4;
    if (hl) d.drawBox(sx - 1, sy - 8, w, 10);
    d.setDrawColor(hl ? 0 : 1);
    d.drawStr(sx, sy, SPECIAL_LABELS[i]);
    d.setDrawColor(1);
    sx += w + 3;
  }

  d.drawStr(0, 63, "Rotate: move  Press: select");
}

} // namespace Keyboard
