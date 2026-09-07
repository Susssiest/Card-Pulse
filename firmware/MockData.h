/*
 * MockData.h
 * ---------------------------------------------------------------------------
 * Everything the firmware needs to look alive with zero network connection:
 * a few hard-coded demo cards, a deterministic mock price, and a deterministic
 * mock price-history graph. Used by CardLibrary (initial seed) and by
 * TcgApiClient (fallback when Wi-Fi/API is unavailable).
 * ---------------------------------------------------------------------------
 */
#pragma once
#include "Types.h"

namespace MockData {

// Returns a pointer to a static array of demo CardData and sets `count`.
const CardData* getDemoCards(int& count);

// Deterministic mock price for a given card id (same id -> same price,
// so the UI doesn't jitter between redraws).
PriceData getMockPrice(const String& cardId);

// Generates a consistent, deterministic mock price-history curve centered
// around the mock current price for `cardId`, spread across `range`.
// Always labeled DataSource::DEMO by the caller — this function only
// produces numbers, DisplayUI.cpp is responsible for the "Demo History" label.
int getMockHistory(const String& cardId, GraphRange range, PricePoint outPoints[], int maxPoints);

} // namespace MockData
