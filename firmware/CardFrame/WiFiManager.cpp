#include "WiFiManager.h"
#include "Config.h"
#include <WiFi.h>
#include <Preferences.h>

namespace WiFiManager {

namespace {
  WifiStatus currentStatus = WifiStatus::DISCONNECTED;
  Preferences prefs;

  bool scanInProgress = false;
  int scanCount = 0;

  bool connectInProgress = false;
  bool connectFinished = true;
  unsigned long connectStartMs = 0;
  String pendingSsid;
  String pendingPassword;

  void loadSavedCredentials(String& ssid, String& password) {
    prefs.begin(NVS_NAMESPACE_WIFI, /*readOnly=*/true);
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("pass", "");
    prefs.end();
  }

  void saveCredentials(const String& ssid, const String& password) {
    prefs.begin(NVS_NAMESPACE_WIFI, /*readOnly=*/false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.end();
    // SECURITY: intentionally never log `password` here or anywhere else.
    DBG_PRINT("[WiFi] Saved credentials for SSID: ");
    DBG_PRINTLN(ssid);
  }
}

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  String ssid, password;
  loadSavedCredentials(ssid, password);
  if (ssid.length() > 0) {
    DBG_PRINT("[WiFi] Auto-connecting to saved SSID: ");
    DBG_PRINTLN(ssid);
    beginConnect(ssid, password);
  } else {
    currentStatus = WifiStatus::DISCONNECTED;
  }
}

void update() {
  // --- Scan progress ---
  if (scanInProgress) {
    int result = WiFi.scanComplete();
    if (result >= 0) {
      scanCount = result;
      scanInProgress = false;
    } else if (result == WIFI_SCAN_FAILED) {
      scanCount = 0;
      scanInProgress = false;
    }
    // WIFI_SCAN_RUNNING: keep waiting, non-blocking.
  }

  // --- Connect progress ---
  if (connectInProgress) {
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) {
      currentStatus = WifiStatus::CONNECTED;
      connectInProgress = false;
      connectFinished = true;
      saveCredentials(pendingSsid, pendingPassword);
      pendingPassword = ""; // clear from RAM once no longer needed
      DBG_PRINTLN("[WiFi] Connected.");
    } else if (millis() - connectStartMs > WIFI_CONNECT_TIMEOUT_MS) {
      currentStatus = WifiStatus::ERROR;
      connectInProgress = false;
      connectFinished = true;
      pendingPassword = "";
      DBG_PRINTLN("[WiFi] Connect attempt timed out.");
    } else {
      currentStatus = WifiStatus::CONNECTING;
    }
  }
}

WifiStatus status() { return currentStatus; }

void startScan() {
  scanInProgress = true;
  scanCount = 0;
  WiFi.scanNetworks(/*async=*/true);
}

bool isScanComplete() { return !scanInProgress; }
int scanResultCount() { return scanCount; }

String scanResultSSID(int index) {
  if (index < 0 || index >= scanCount) return "";
  return WiFi.SSID(index);
}

int8_t scanResultRSSI(int index) {
  if (index < 0 || index >= scanCount) return -127;
  return WiFi.RSSI(index);
}

bool scanResultIsSecure(int index) {
  if (index < 0 || index >= scanCount) return true;
  return WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
}

void beginConnect(const String& ssid, const String& password) {
  pendingSsid = ssid;
  pendingPassword = password;
  connectInProgress = true;
  connectFinished = false;
  connectStartMs = millis();
  currentStatus = WifiStatus::CONNECTING;
  DBG_PRINT("[WiFi] Connecting to SSID: ");
  DBG_PRINTLN(ssid); // SSID is not secret; password never printed
  WiFi.begin(ssid.c_str(), password.c_str());
}

bool isConnectAttemptFinished() { return connectFinished; }

void forgetSavedCredentials() {
  prefs.begin(NVS_NAMESPACE_WIFI, /*readOnly=*/false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
}

} // namespace WiFiManager
