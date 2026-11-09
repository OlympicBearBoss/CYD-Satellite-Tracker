#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "RootCACert.h"

namespace SiteLink {
static constexpr char SITE_BASE[] = "https://cyd-satellite-tracker.pages.dev";
static WebServer server(80);
static Preferences prefs;
static String wifiName;
static String deviceToken;
static String deviceId;
static String pairingCode;
static String statusText = "НЕ НАСТРОЕНО";
static bool portalStarted = false;
static bool serverStarted = false;
static uint32_t lastHeartbeat = 0;
static uint32_t lastWifiAttempt = 0;

static String htmlPage(const String& message = "") {
  String page = F("<!doctype html><html lang='ru'><meta name='viewport' content='width=device-width,initial-scale=1'><meta charset='utf-8'><title>CYD Satellite Tracker</title><style>body{font:16px system-ui;background:#080f1d;color:#e8f1ff;max-width:520px;margin:32px auto;padding:18px}main{background:#111d30;padding:22px;border-radius:18px}input{box-sizing:border-box;width:100%;padding:13px;margin:7px 0 16px;border:1px solid #38506d;border-radius:9px;background:#0b1525;color:white}button{padding:14px 20px;background:#168bd2;color:white;border:0;border-radius:10px;font-weight:700;width:100%}.muted{color:#9fb2ca;font-size:13px}</style><main><h2>CYD Satellite Tracker</h2><p>Настройка Wi-Fi и привязка контроллера к сайту</p>");
  if (message.length()) page += "<p>" + message + "</p>";
  page += F("<form method='post' action='/save'><label>Имя Wi-Fi сети</label><input name='ssid' maxlength='64' required><label>Пароль Wi-Fi</label><input name='password' type='password' maxlength='64'><label>Код привязки с сайта (8 цифр)</label><input name='code' inputmode='numeric' pattern='[0-9]{8}' maxlength='8' required><label>Имя устройства</label><input name='device' value='CYD Satellite Tracker' maxlength='48'><button type='submit'>Сохранить и подключить</button></form><p class='muted'>Код создаётся в аккаунте на сайте: Профиль и устройства → Мои устройства. Он действует 10 минут и используется один раз. После привязки пароль и токен хранятся в памяти ESP32.</p></main></html>");
  return page;
}

static void handleRoot() { server.send(200, "text/html; charset=utf-8", htmlPage()); }

static void handleSave() {
  String ssid = server.arg("ssid"); ssid.trim();
  String password = server.arg("password");
  String code = server.arg("code"); code.trim();
  String name = server.arg("device"); name.trim();
  if (!ssid.length() || code.length() != 8) {
    server.send(400, "text/html; charset=utf-8", htmlPage("Проверьте Wi-Fi и восьмизначный код."));
    return;
  }
  for (size_t i = 0; i < code.length(); ++i) if (!isDigit(code[i])) {
    server.send(400, "text/html; charset=utf-8", htmlPage("Код должен содержать только цифры."));
    return;
  }
  prefs.begin("cyd-site", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  prefs.putString("code", code);
  prefs.putString("name", name.length() ? name : "CYD ESP32");
  prefs.end();
  server.send(200, "text/html; charset=utf-8", htmlPage("Настройки сохранены. Контроллер перезапускается..."));
  delay(700);
  ESP.restart();
}

static void startPortal() {
  if (portalStarted) return;
  WiFi.mode(WIFI_AP_STA);
  String apName = "CYD-Setup-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFF), HEX);
  WiFi.softAP(apName.c_str(), "cydsetup");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot);
  server.begin();
  portalStarted = true;
  serverStarted = true;
  statusText = "ТОЧКА НАСТРОЙКИ: " + apName + " / 192.168.4.1";
  Serial.println(statusText);
}

static bool securePost(const String& path, const String& body, const String& bearer, String& response) {
  WiFiClientSecure client;
  client.setCACert(ROOT_CA_BUNDLE);
  HTTPClient http;
  String url = String(SITE_BASE) + path;
  if (!http.begin(client, url)) { statusText = "ОШИБКА HTTPS"; return false; }
  http.setTimeout(12000);
  http.addHeader("Content-Type", "application/json");
  if (bearer.length()) http.addHeader("Authorization", "Bearer " + bearer);
  int code = http.POST(body);
  response = http.getString();
  http.end();
  if (code < 200 || code >= 300) {
    statusText = "САЙТ: HTTP " + String(code);
    Serial.printf("Site API HTTP %d\n", code);
    return false;
  }
  return true;
}

static bool claimDevice() {
  if (!pairingCode.length() || !WiFi.isConnected()) return false;
  prefs.begin("cyd-site", true);
  String name = prefs.getString("name", "CYD ESP32");
  prefs.end();
  JsonDocument request;
  request["code"] = pairingCode;
  request["device_name"] = name;
  String body; serializeJson(request, body);
  String response;
  if (!securePost("/api/devices/claim", body, "", response)) return false;
  JsonDocument result;
  if (deserializeJson(result, response) || !result["device_token"].is<const char*>()) {
    statusText = "ОШИБКА ОТВЕТА САЙТА";
    return false;
  }
  deviceToken = result["device_token"].as<String>();
  deviceId = result["device_id"] | "";
  prefs.begin("cyd-site", false);
  prefs.putString("token", deviceToken);
  prefs.putString("deviceId", deviceId);
  prefs.remove("code");
  prefs.end();
  pairingCode = "";
  statusText = "ПРИВЯЗАНО К САЙТУ";
  Serial.println("Device pairing successful; token stored in NVS.");
  return true;
}

static bool heartbeat() {
  if (!WiFi.isConnected() || !deviceToken.length()) return false;
  String response;
  if (!securePost("/api/devices/heartbeat", "{}", deviceToken, response)) {
    if (response.indexOf("не авторизовано") >= 0) {
      prefs.begin("cyd-site", false);
      prefs.remove("token"); prefs.remove("deviceId");
      prefs.end();
      deviceToken = ""; deviceId = "";
      statusText = "НУЖНА ПОВТОРНАЯ ПРИВЯЗКА";
    }
    return false;
  }
  statusText = "САЙТ: НА СВЯЗИ";
  lastHeartbeat = millis();
  return true;
}

static void begin() {
  prefs.begin("cyd-site", true);
  wifiName = prefs.getString("ssid", "");
  String password = prefs.getString("pass", "");
  pairingCode = prefs.getString("code", "");
  deviceToken = prefs.getString("token", "");
  deviceId = prefs.getString("deviceId", "");
  prefs.end();
  if (!wifiName.length()) { startPortal(); return; }
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(wifiName.c_str(), password.c_str());
  lastWifiAttempt = millis();
  statusText = "ПОДКЛЮЧЕНИЕ К WI-FI";
  // Keep the setup portal available for pairing or changing credentials.
  startPortal();
}

static void loop() {
  if (serverStarted) server.handleClient();
  if (!wifiName.length()) return;
  if (!WiFi.isConnected()) {
    if (millis() - lastWifiAttempt > 20000) {
      lastWifiAttempt = millis();
      WiFi.disconnect();
      prefs.begin("cyd-site", true);
      String password = prefs.getString("pass", "");
      prefs.end();
      WiFi.begin(wifiName.c_str(), password.c_str());
      statusText = "WI-FI НЕТ СВЯЗИ";
    }
    return;
  }
  if (!deviceToken.length() && pairingCode.length()) {
    if (millis() - lastHeartbeat > 5000) {
      lastHeartbeat = millis();
      claimDevice();
    }
    return;
  }
  if (deviceToken.length() && millis() - lastHeartbeat > 60000) heartbeat();
}

static String status() {
  if (WiFi.isConnected()) return "Wi-Fi OK · " + WiFi.localIP().toString() + " · " + statusText;
  return statusText;
}
}
