#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "device_config.h"

constexpr int BACKLIGHT_PIN = 4;
constexpr uint16_t DNS_PORT = 53;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;

Preferences preferences;
TFT_eSPI tft = TFT_eSPI();
DNSServer dnsServer;
WebServer portalServer(80);

bool portalActive = false;
bool wifiReady = false;
unsigned long lastOtaCheckAt = 0;
unsigned long portalStartedAt = 0;
uint32_t screenUptimeS = 0;
unsigned long lastScreenTickAt = 0;

void drawStatus(const String &line1, const String &line2 = "", const String &line3 = "") {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("LILYGO T-Display v1.1", 120, 20, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String("FW: ") + FW_VERSION, 120, 36, 2);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(line1, 120, 62, 2);
  if (line2.length() > 0) {
    tft.drawString(line2, 120, 87, 2);
  }
  if (line3.length() > 0) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(line3, 120, 114, 2);
  }
}

bool connectToWiFi(const String &ssid, const String &password) {
  if (ssid.isEmpty()) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  drawStatus("Connecting to Wi-Fi", ssid);

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startedAt) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    drawStatus("Wi-Fi connected", WiFi.localIP().toString(), ssid);
    return true;
  }

  WiFi.disconnect(true, true);
  return false;
}

void saveWiFiConfig(const String &ssid, const String &password) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.end();
}

bool tryStoredWiFi() {
  preferences.begin("wifi", true);
  const String ssid = preferences.getString("ssid", "");
  const String pass = preferences.getString("pass", "");
  preferences.end();

  return connectToWiFi(ssid, pass);
}

bool tryBuiltInWiFi() {
  if (String(DEFAULT_WIFI_SSID).isEmpty()) {
    return false;
  }

  if (connectToWiFi(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD)) {
    saveWiFiConfig(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
    return true;
  }

  return false;
}

String buildPortalHtml() {
  const auto htmlEscape = [](const String &value) -> String {
    String out = value;
    out.replace("&", "&amp;");
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    out.replace("\"", "&quot;");
    out.replace("'", "&#39;");
    return out;
  };

  String networkOptions;
  const int networkCount = WiFi.scanNetworks(false, true);
  if (networkCount <= 0) {
    networkOptions = "<option value=''>No networks found</option>";
  } else {
    for (int i = 0; i < networkCount; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) {
        continue;
      }
      const String escapedSsid = htmlEscape(ssid);
      const int rssi = WiFi.RSSI(i);
      networkOptions += "<option value='" + escapedSsid + "'>" + escapedSsid + " (" + String(rssi) + " dBm)</option>";
    }
  }
  WiFi.scanDelete();

  String html = F(
      "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,"
      "initial-scale=1'><title>Wi-Fi Setup</title></head><body><h2>Device Wi-Fi setup</h2>"
      "<form method='POST' action='/save'><label>Available networks</label><br>"
      "<select id='ssidSelect' onchange='document.getElementById(\"ssidInput\").value=this.value'>");
  html += networkOptions;
  html += F(
      "</select><br><br>"
      "<label>SSID</label><br><input id='ssidInput' name='ssid' required><br><br>"
      "<label>Password</label><br><input name='pass' type='password'><br><br>"
      "<button type='submit'>Save and connect</button></form>"
      "<p>If your network is hidden, type SSID manually in the field above.</p>"
      "<p>After successful connection the device will reboot."
      "</p></body></html>");
  return html;
}

void stopPortalAndReboot() {
  portalActive = false;
  portalServer.stop();
  dnsServer.stop();
  delay(1000);
  ESP.restart();
}

void startSetupPortal() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD);

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  portalActive = true;
  portalStartedAt = millis();
  drawStatus("Setup portal active", String("AP: ") + SETUP_AP_SSID, WiFi.softAPIP().toString());

  portalServer.on("/", HTTP_GET, []() { portalServer.send(200, "text/html", buildPortalHtml()); });

  portalServer.on("/save", HTTP_POST, []() {
    const String ssid = portalServer.arg("ssid");
    const String pass = portalServer.arg("pass");
    if (ssid.isEmpty()) {
      portalServer.send(400, "text/plain", "SSID is required");
      return;
    }

    drawStatus("Trying Wi-Fi", ssid);
    if (!connectToWiFi(ssid, pass)) {
      portalServer.send(400, "text/plain", "Failed to connect. Check credentials and try again.");
      drawStatus("Setup portal active", "Connection failed", "Try again");
      return;
    }

    saveWiFiConfig(ssid, pass);
    portalServer.send(200, "text/plain", "Saved. Rebooting...");
    delay(300);
    stopPortalAndReboot();
  });

  portalServer.onNotFound([]() { portalServer.sendHeader("Location", "/", true); portalServer.send(302, "text/plain", ""); });
  portalServer.begin();
}

bool fetchLatestReleaseBinUrl(String &latestTag, String &firmwareUrl) {
  String repo = GITHUB_REPO;
  repo.trim();
  while (repo.startsWith("/")) {
    repo.remove(0, 1);
  }
  if (repo == "owner/repo" || repo.indexOf('/') < 1) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  const String url = String("https://api.github.com/repos/") + repo + "/releases/latest";
  if (!http.begin(client, url)) {
    return false;
  }

  http.addHeader("Accept", "application/vnd.github+json");
  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  JsonDocument doc;
  const auto err = deserializeJson(doc, http.getString());
  http.end();
  if (err) {
    return false;
  }

  const bool prerelease = doc["prerelease"] | false;
  const bool draft = doc["draft"] | false;
  if (prerelease || draft) {
    return false;
  }

  latestTag = doc["tag_name"] | "";
  const String expectedAssetName = OTA_ASSET_NAME;
  JsonArray assets = doc["assets"].as<JsonArray>();
  String fallbackBinUrl;
  for (JsonVariant asset : assets) {
    const String name = asset["name"] | "";
    const String downloadUrl = asset["browser_download_url"] | "";
    if (!name.endsWith(".bin") || !downloadUrl.endsWith(".bin")) {
      continue;
    }

    if (expectedAssetName.length() > 0 && name == expectedAssetName) {
      firmwareUrl = downloadUrl;
      return !latestTag.isEmpty();
    }

    if (fallbackBinUrl.isEmpty()) {
      fallbackBinUrl = downloadUrl;
    }
  }

  if (expectedAssetName.length() == 0 && !fallbackBinUrl.isEmpty()) {
    firmwareUrl = fallbackBinUrl;
    return !latestTag.isEmpty();
  }

  return !latestTag.isEmpty() && !firmwareUrl.isEmpty();
}

void checkForOtaUpdate() {
  if (!wifiReady || WiFi.status() != WL_CONNECTED) {
    return;
  }

  String latestTag;
  String firmwareUrl;
  if (!fetchLatestReleaseBinUrl(latestTag, firmwareUrl)) {
    return;
  }

  if (latestTag == FW_VERSION) {
    return;
  }

  drawStatus("Updating firmware", latestTag, "Please wait...");

  WiFiClientSecure updateClient;
  updateClient.setInsecure();
  t_httpUpdate_return ret = httpUpdate.update(updateClient, firmwareUrl);

  if (ret == HTTP_UPDATE_FAILED) {
    drawStatus("Update failed", httpUpdate.getLastErrorString());
    delay(1500);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  drawStatus("Booting...");

  wifiReady = tryStoredWiFi();
  if (!wifiReady) {
    wifiReady = tryBuiltInWiFi();
  }

  if (!wifiReady) {
    startSetupPortal();
  } else {
    checkForOtaUpdate();
    lastOtaCheckAt = millis();
  }

  drawStatus("Device ready", wifiReady ? WiFi.localIP().toString() : "Portal mode");
}

void loop() {
  if (portalActive) {
    dnsServer.processNextRequest();
    portalServer.handleClient();
    if ((millis() - portalStartedAt) >= SETUP_PORTAL_TIMEOUT_MS) {
      drawStatus("Portal timeout", "Rebooting...");
      delay(1000);
      ESP.restart();
    }
  } else if (WiFi.status() != WL_CONNECTED) {
    wifiReady = false;
    startSetupPortal();
  } else {
    wifiReady = true;
    const unsigned long now = millis();
    if (now - lastOtaCheckAt >= OTA_CHECK_INTERVAL_MS) {
      checkForOtaUpdate();
      lastOtaCheckAt = now;
    }
  }

  const unsigned long now = millis();
  if (now - lastScreenTickAt >= 1000) {
    lastScreenTickAt = now;
    screenUptimeS++;
    tft.fillRect(0, 125, 240, 20, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String("Uptime: ") + screenUptimeS + " s", 120, 135, 2);
  }
}
