/*
 * CardLibrary.h
 * ---------------------------------------------------------------------------
 * Stores the user's saved cards (name + tcgapi.dev id + display metadata)
 * and which one is "current". Deliberately knows NOTHING about the API or
 * the UI — it is a plain data-storage module so it can later be swapped for
 * SD-card storage, a web interface, or cloud sync without touching
 * DisplayUI.cpp or TcgApiClient.cpp.
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "Types.h"

namespace CardLibrary {

void begin(); // loads saved cards + selected index from NVS (or seeds demo cards)

int count();
const CardData& get(int index);

int selectedIndex();
void setSelectedIndex(int index);
const CardData& selectedCard();

// Returns the new index, or -1 if the library is full (MAX_SAVED_CARDS).
int addCard(const CardData& card);

// Removes a card by index and fixes up the selected index if needed.
void removeCard(int index);

void saveToNVS();
void loadFromNVS();

// Populates a few hard-coded demo cards. Called automatically by begin()
// the first time the device boots with an empty library.
void seedDemoCards();

} // namespace CardLibrary
