/*
 * WiFiManager.h
 * ---------------------------------------------------------------------------
 * Wraps ESP32 WiFi.h with a small non-blocking state machine: scan, connect,
 * and status reporting, all driven by update() so the rest of the firmware
 * (menus, LEDs, input) never freezes while Wi-Fi does its thing.
 * ---------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "Types.h"

namespace WiFiManager {

void begin(); // call once from setup(); attempts auto-connect with saved creds

// Call every loop() iteration. Advances scan/connect state machines.
void update();

WifiStatus status();

// ---- Scanning ----
// Starts an async scan. Non-blocking; poll isScanComplete().
void startScan();
bool isScanComplete();
int scanResultCount();
String scanResultSSID(int index);
int8_t scanResultRSSI(int index);
bool scanResultIsSecure(int index);

// ---- Connecting ----
// Starts a non-blocking connection attempt. Poll status() for progress;
// becomes CONNECTED or ERROR. On success, credentials are saved to NVS
// automatically (see WiFiManager.cpp) so future boots auto-reconnect.
void beginConnect(const String& ssid, const String& password);

// True once a connect attempt (success or failure) has finished, so the UI
// knows it can stop showing "Connecting..." and show a result instead.
bool isConnectAttemptFinished();

// Clears any saved credentials (not exposed in the UI yet, but handy for
// future "forget network" feature / debugging).
void forgetSavedCredentials();

} // namespace WiFiManager
