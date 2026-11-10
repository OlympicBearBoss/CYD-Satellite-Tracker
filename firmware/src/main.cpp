#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <TinyGPSPlus.h>
#include "BoardConfig.h"
#include "WifiConfig.h"

TFT_eSPI tft;
TinyGPSPlus gps;

static constexpr uint8_t DISPLAY_ROTATION = 1;
static constexpr uint32_t SPLASH_MS = 6000;
static constexpr uint32_t INIT_MS = 3500;
static constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
static constexpr uint32_t WIFI_RETRY_MS = 15000;

enum class Screen : uint8_t { Splash, Init, Connecting, Connected, NoCredentials, Failed };
static Screen screen = Screen::Splash;
static uint32_t screenStartedAt = 0;
static uint32_t wifiAttemptAt = 0;
static bool wifiStarted = false;
static bool gpsSeen = false;

static constexpr uint16_t BG = 0x020A1B;
static constexpr uint16_t PANEL = 0x0A1930;
static constexpr uint16_t BLUE = 0x168BD2;
static constexpr uint16_t MUTED = 0x7897B5;

static void printGeometry(const char* stage) {
  Serial.printf("CYD V29 | %s | rotation=%u | logical=%dx%d\\n",
                stage, tft.getRotation(), tft.width(), tft.height());
}

static void drawSatellite(int16_t cx, int16_t cy) {
  tft.fillRoundRect(cx - 8, cy - 6, 16, 12, 3, TFT_LIGHTGREY);
  tft.drawRoundRect(cx - 8, cy - 6, 16, 12, 3, TFT_WHITE);
  tft.fillRect(cx - 31, cy - 8, 19, 16, TFT_BLUE);
  tft.drawRect(cx - 31, cy - 8, 19, 16, TFT_CYAN);
  tft.fillRect(cx + 12, cy - 8, 19, 16, TFT_BLUE);
  tft.drawRect(cx + 12, cy - 8, 19, 16, TFT_CYAN);
  tft.drawLine(cx, cy - 6, cx + 5, cy - 15, TFT_WHITE);
  tft.fillCircle(cx + 5, cy - 16, 2, TFT_YELLOW);
}

static void showSplash() {
  const int16_t w = tft.width(), h = tft.height();
  tft.fillScreen(BG);
  const int16_t sx[] = {12,34,61,87,111,146,174,203,229,257,291,307,43,132,238,279};
  const int16_t sy[] = {18,51,29,76,14,42,17,64,31,19,54,91,113,96,120,147};
  for (uint8_t i=0; i<16; ++i)
    if (sx[i]<w && sy[i]<h) tft.fillCircle(sx[i],sy[i],(i%4==0)?2:1,TFT_WHITE);
  tft.drawEllipse(w/2,h/2+15,104,43,0x1C6B9A);
  tft.drawEllipse(w/2,h/2+15,108,47,0x123B65);
  const int16_t ex=w/2, ey=h/2+23;
  tft.fillCircle(ex,ey,48,0x0878B8);
  tft.drawCircle(ex,ey,49,TFT_CYAN);
  tft.fillCircle(ex-13,ey-12,19,0x168B70);
  tft.fillCircle(ex+17,ey+9,15,0x168B70);
  tft.fillCircle(ex-2,ey+25,9,0x168B70);
  tft.fillCircle(ex+3,ey-2,42,0x09699F);
  drawSatellite(ex+84,ey-28);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN,BG); tft.drawString("CYD",w/2,24,4);
  tft.setTextColor(TFT_WHITE,BG); tft.drawString("SATELLITE TRACKER",w/2,49,2);
  tft.setTextColor(0x9CCDE8,BG); tft.drawString("AUTOMATIC ANTENNA POINTING",w/2,h-18,1);
  tft.drawFastHLine(24,h-34,w-48,0x1C6B9A);
  printGeometry("SPLASH");
}

static void header(const char* title, const char* subtitle) {
  tft.fillScreen(BG);
  tft.fillRect(0,0,tft.width(),42,PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN,PANEL);
  tft.drawString("CYD  /  SATELLITE TRACKER",10,7,2);
  tft.setTextColor(TFT_WHITE,BG);
  tft.drawString(title,12,55,4);
  tft.setTextColor(MUTED,BG);
  tft.drawString(subtitle,13,87,2);
}

static void statusRow(int y, const char* label, const char* value, uint16_t color) {
  tft.fillRoundRect(10,y,tft.width()-20,27,5,PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE,PANEL); tft.drawString(label,18,y+14,2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(color,PANEL); tft.drawString(value,tft.width()-18,y+14,2);
}

static void showInitialization() {
  header("INITIALIZATION","Hardware and service checks");
  statusRow(112,"DISPLAY","OK",TFT_GREEN);
  statusRow(145,"ESP32","OK",TFT_GREEN);
  statusRow(178,"GPS MODULE",gpsSeen?"DATA":"SEARCHING",gpsSeen?TFT_GREEN:TFT_YELLOW);
  statusRow(211,"MOTOR DRIVERS","LOCKED",TFT_ORANGE);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(MUTED,BG);
  tft.drawString("Motors remain disabled during startup",tft.width()/2,tft.height()-3,1);
}

static void showConnecting() {
  header("WI-FI CONNECTION","Connecting to your home network");
  statusRow(120,"NETWORK",WIFI_SSID[0]?WIFI_SSID:"NOT CONFIGURED",WIFI_SSID[0]?TFT_CYAN:TFT_YELLOW);
  statusRow(155,"STATUS","CONNECTING...",TFT_YELLOW);
  tft.drawRoundRect(14,202,tft.width()-28,10,4,0x31516F);
  tft.fillRoundRect(16,204,((millis()/35)%(tft.width()-32)),6,3,BLUE);
  tft.setTextDatum(BC_DATUM); tft.setTextColor(MUTED,BG);
  tft.drawString("Please wait",tft.width()/2,tft.height()-7,1);
}

static void showNoCredentials() {
  header("WI-FI SETUP","Network credentials are not configured");
  statusRow(120,"MODE","WI-FI STATION",TFT_CYAN);
  statusRow(155,"NETWORK","ADD IN WifiConfig.h",TFT_YELLOW);
  tft.setTextDatum(TL_DATUM); tft.setTextColor(TFT_WHITE,BG);
  tft.drawString("Set your router name and password",14,205,1);
  tft.drawString("in firmware/include/WifiConfig.h",14,222,1);
}

static void showConnected() {
  header("CONNECTED","CYD is on your local network");
  statusRow(112,"WI-FI","CONNECTED",TFT_GREEN);
  char signalText[20];
  char ipText[20];
  snprintf(signalText,sizeof(signalText),"%d dBm",WiFi.RSSI());
  WiFi.localIP().toString().toCharArray(ipText,sizeof(ipText));
  statusRow(145,"SIGNAL",signalText,TFT_CYAN);
  statusRow(178,"IP ADDRESS",ipText,TFT_GREEN);
  statusRow(211,"GPS",gpsSeen?"DATA RECEIVED":"SEARCHING",gpsSeen?TFT_GREEN:TFT_YELLOW);
}

static void showFailed() {
  header("CONNECTION FAILED","Could not reach the selected router");
  statusRow(120,"WI-FI","RETRYING",TFT_ORANGE);
  statusRow(155,"NETWORK",WIFI_SSID[0]?WIFI_SSID:"NOT CONFIGURED",TFT_YELLOW);
  tft.setTextDatum(BC_DATUM); tft.setTextColor(MUTED,BG);
  tft.drawString("Check router range and credentials",tft.width()/2,tft.height()-8,1);
}

static void beginWiFi() {
  if (!WIFI_SSID[0]) {
    screen=Screen::NoCredentials;
    showNoCredentials();
    screenStartedAt=millis();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  wifiStarted=true;
  wifiAttemptAt=millis();
  screen=Screen::Connecting;
  showConnecting();
  screenStartedAt=millis();
  Serial.printf("WiFi: connecting to configured SSID '%s'\\n",WIFI_SSID);
}

void setup() {
  Serial.begin(115200);
  delay(150);
  pinMode(Pins::TFT_BACKLIGHT,OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT,HIGH);
  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  tft.fillScreen(TFT_BLACK);
  Serial2.begin(9600,SERIAL_8N1,Pins::GPS_RX,-1);
  showSplash();
  screen=Screen::Splash;
  screenStartedAt=millis();
}

void loop() {
  while (Serial2.available()) {
    if (gps.encode(Serial2.read())) {
      if (gps.location.isValid()) gpsSeen=true;
    }
  }

  const uint32_t now=millis();
  if (screen==Screen::Splash && now-screenStartedAt>=SPLASH_MS) {
    showInitialization();
    screen=Screen::Init;
    screenStartedAt=now;
  } else if (screen==Screen::Init && now-screenStartedAt>=INIT_MS) {
    beginWiFi();
  } else if (screen==Screen::Connecting) {
    if (WiFi.status()==WL_CONNECTED) {
      screen=Screen::Connected; showConnected(); screenStartedAt=now;
      Serial.printf("WiFi connected. IP=%s\\n",WiFi.localIP().toString().c_str());
    } else if (now-wifiAttemptAt>=WIFI_TIMEOUT_MS) {
      screen=Screen::Failed; showFailed(); screenStartedAt=now;
    }
  } else if (screen==Screen::Failed && now-screenStartedAt>=WIFI_RETRY_MS) {
    WiFi.disconnect();
    beginWiFi();
  } else if (screen==Screen::Connected && now-screenStartedAt>=1000) {
    showConnected(); screenStartedAt=now;
  }
}
