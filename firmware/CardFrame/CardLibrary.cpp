#include "CardLibrary.h"
#include "Config.h"
#include "MockData.h"
#include <Preferences.h>

namespace CardLibrary {

namespace {
  CardData cards[MAX_SAVED_CARDS];
  int cardCount = 0;
  int selected = 0;
  Preferences prefs;

  // Preferences (NVS) has no native "array of structs" support, so each
  // field of each card is stored under its own key, e.g. "n3"/"s3"/"c3"/"v3"/"g3"/"i3"
  // for card index 3. This is simple and plenty fast for <= MAX_SAVED_CARDS
  // entries; if you outgrow it, that's exactly the seam described at the top
  // of this file where SD-card or cloud storage would plug in instead.
  String keyFor(const char* prefix, int index) {
    return String(prefix) + String(index);
  }
}

void seedDemoCards() {
  int n = 0;
  const CardData* demo = MockData::getDemoCards(n);
  cardCount = 0;
  for (int i = 0; i < n && i < MAX_SAVED_CARDS; i++) {
    cards[cardCount++] = demo[i];
  }
  selected = 0;
  saveToNVS();
}

void loadFromNVS() {
  prefs.begin(NVS_NAMESPACE_CARDS, /*readOnly=*/true);
  cardCount = prefs.getInt("count", -1);
  selected = prefs.getInt("selected", 0);

  if (cardCount < 0) {
    // First boot: nothing saved yet.
    prefs.end();
    seedDemoCards();
    return;
  }

  if (cardCount > MAX_SAVED_CARDS) cardCount = MAX_SAVED_CARDS;
  for (int i = 0; i < cardCount; i++) {
    cards[i].cardId          = prefs.getString(keyFor("i", i).c_str(), "");
    cards[i].name            = prefs.getString(keyFor("n", i).c_str(), "");
    cards[i].setName         = prefs.getString(keyFor("s", i).c_str(), "");
    cards[i].collectorNumber = prefs.getString(keyFor("c", i).c_str(), "");
    cards[i].variant         = prefs.getString(keyFor("v", i).c_str(), "");
    cards[i].game            = prefs.getString(keyFor("g", i).c_str(), "");
    cards[i].valid = cards[i].cardId.length() > 0;
  }
  prefs.end();

  if (selected >= cardCount) selected = cardCount > 0 ? cardCount - 1 : 0;
}

void saveToNVS() {
  prefs.begin(NVS_NAMESPACE_CARDS, /*readOnly=*/false);
  prefs.putInt("count", cardCount);
  prefs.putInt("selected", selected);
  for (int i = 0; i < cardCount; i++) {
    prefs.putString(keyFor("i", i).c_str(), cards[i].cardId);
    prefs.putString(keyFor("n", i).c_str(), cards[i].name);
    prefs.putString(keyFor("s", i).c_str(), cards[i].setName);
    prefs.putString(keyFor("c", i).c_str(), cards[i].collectorNumber);
    prefs.putString(keyFor("v", i).c_str(), cards[i].variant);
    prefs.putString(keyFor("g", i).c_str(), cards[i].game);
  }
  prefs.end();
}

void begin() {
  loadFromNVS();
}

int count() { return cardCount; }

const CardData& get(int index) {
  static CardData invalid;
  if (index < 0 || index >= cardCount) return invalid;
  return cards[index];
}

int selectedIndex() { return selected; }

void setSelectedIndex(int index) {
  if (index >= 0 && index < cardCount) {
    selected = index;
    saveToNVS();
  }
}

const CardData& selectedCard() {
  return get(selected);
}

int addCard(const CardData& card) {
  if (cardCount >= MAX_SAVED_CARDS) return -1;
  cards[cardCount] = card;
  cards[cardCount].valid = true;
  int newIndex = cardCount;
  cardCount++;
  saveToNVS();
  return newIndex;
}

void removeCard(int index) {
  if (index < 0 || index >= cardCount) return;
  for (int i = index; i < cardCount - 1; i++) {
    cards[i] = cards[i + 1];
  }
  cardCount--;
  if (selected >= cardCount) selected = cardCount > 0 ? cardCount - 1 : 0;
  saveToNVS();
}

} // namespace CardLibrary
