#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <ctype.h>
#include "BoardConfig.h"
#include "WifiConfig.h"

TFT_eSPI tft;
TinyGPSPlus gps;
Preferences prefs;

static constexpr uint8_t DISPLAY_ROTATION = 1;
static constexpr uint32_t SPLASH_MS = 6000;
static constexpr uint32_t INIT_MS = 3500;
static constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
static constexpr uint16_t TOUCH_THRESHOLD = 500;

enum class Screen : uint8_t { Splash, Init, WifiSetup, Connecting, Connected, Failed };
static Screen screen = Screen::Splash;
static uint32_t screenStartedAt = 0;
static uint32_t wifiAttemptAt = 0;
static uint32_t lastTouchAt = 0;
static bool gpsSeen = false;
static bool touchCalibrated = false;
static bool symbolsPage = false;
static bool shiftKey = false;
static bool passwordVisible = false;
static bool editingPassword = false;
static String wifiName;
static String wifiPassword;
static String setupMessage;

static constexpr uint16_t BG = 0x020A;
static constexpr uint16_t PANEL = 0x0A19;
static constexpr uint16_t BLUE = 0x168B;
static constexpr uint16_t MUTED = 0x7897;
static uint16_t touchCal[5];

static void printGeometry(const char* stage) {
  Serial.printf("CYD V30 | %s | rotation=%u | logical=%dx%d\n",
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
  tft.drawEllipse(w/2,h/2+15,104,43,0x1C6B);
  tft.drawEllipse(w/2,h/2+15,108,47,0x123B);
  const int16_t ex=w/2, ey=h/2+23;
  tft.fillCircle(ex,ey,48,0x0878);
  tft.drawCircle(ex,ey,49,TFT_CYAN);
  tft.fillCircle(ex-13,ey-12,19,0x168B);
  tft.fillCircle(ex+17,ey+9,15,0x168B);
  tft.fillCircle(ex-2,ey+25,9,0x168B);
  tft.fillCircle(ex+3,ey-2,42,0x0969);
  drawSatellite(ex+84,ey-28);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN,BG); tft.drawString("CYD",w/2,24,4);
  tft.setTextColor(TFT_WHITE,BG); tft.drawString("SATELLITE TRACKER",w/2,49,2);
  tft.setTextColor(0x9CCD,BG); tft.drawString("AUTOMATIC ANTENNA POINTING",w/2,h-18,1);
  tft.drawFastHLine(24,h-34,w-48,0x1C6B);
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

static void drawButton(int x,int y,int w,int h,const char* label,uint16_t color) {
  tft.fillRoundRect(x,y,w,h,5,color);
  tft.drawRoundRect(x,y,w,h,5,TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE,color);
  tft.drawString(label,x+w/2,y+h/2,2);
}

static void drawField(int y,const char* label,const String& value,bool selected,bool mask) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(selected?TFT_CYAN:MUTED,BG);
  tft.drawString(label,8,y-13,1);
  tft.fillRoundRect(7,y,226,27,4,selected?0x123B:PANEL);
  tft.drawRoundRect(7,y,226,27,4,selected?TFT_CYAN:0x3151);
  String shown;
  if(mask && !passwordVisible) {
    for(size_t i=0;i<value.length();++i) shown += '*';
  } else shown=value;
  if(shown.length()>24) shown="..."+shown.substring(shown.length()-21);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE,selected?0x123B:PANEL);
  tft.drawString(shown,13,y+14,2);
}

static void drawKey(int x,int y,int w,int h,const char* label) {
  tft.fillRoundRect(x,y,w,h,3,0x1930);
  tft.drawRoundRect(x,y,w,h,3,0x4160);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE,0x1930);
  tft.drawString(label,x+w/2,y+h/2,2);
}

static void drawKeyRow(const char* keys,int y,int x,int keyW,int gap) {
  for(int i=0;keys[i];++i) {
    char label[2]={keys[i],0};
    if(shiftKey && !symbolsPage && label[0]>='a' && label[0]<='z') label[0]=toupper(label[0]);
    drawKey(x+i*(keyW+gap),y,keyW,25,label);
  }
}

static void showWifiSetup() {
  screen=Screen::WifiSetup;
  header("WI-FI SETUP","Tap a field, then enter network details");
  drawField(62,"NETWORK NAME (SSID)",wifiName,!editingPassword,false);
  drawField(105,"PASSWORD",wifiPassword,editingPassword,true);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(setupMessage.length()?TFT_YELLOW:MUTED,BG);
  tft.drawString(setupMessage.length()?setupMessage.c_str():"Credentials are stored on this device",8,137,1);

  if(!symbolsPage) {
    drawKeyRow("qwertyuiop",157,4,22,1);
    drawKeyRow("asdfghjkl",186,14,23,2);
    drawKeyRow("zxcvbnm",215,7,25,3);
  } else {
    drawKeyRow("1234567890",157,4,22,1);
    drawKeyRow("@#$%&*-+()",186,4,22,1);
    drawKeyRow("!?.,_:;/'",215,8,23,2);
  }
  drawKey(207,215,29,25,"<");
  drawKey(4,246,42,25,shiftKey?"SHIFT*":"SHIFT");
  drawKey(49,246,87,25,"SPACE");
  drawKey(139,246,48,25,symbolsPage?"ABC":"123");
  drawKey(190,246,46,25,"CLEAR");
  drawButton(8,279,224,33,"CONNECT",BLUE);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(MUTED,BG);
  tft.drawString(passwordVisible?"HIDE":"SHOW",231,99,1);
}

static void showConnecting() {
  header("WI-FI CONNECTION","Connecting to the selected network");
  String shortName=wifiName;
  if(shortName.length()>18) shortName=shortName.substring(0,18)+"...";
  statusRow(120,"NETWORK",shortName.c_str(),TFT_CYAN);
  statusRow(155,"STATUS","CONNECTING...",TFT_YELLOW);
  tft.drawRoundRect(14,202,tft.width()-28,10,4,0x3151);
  tft.fillRoundRect(16,204,((millis()/35)%(tft.width()-32)),6,3,BLUE);
  tft.setTextDatum(BC_DATUM); tft.setTextColor(MUTED,BG);
  tft.drawString("Please wait",tft.width()/2,tft.height()-7,1);
}

static void showConnected() {
  header("CONNECTED","CYD is on your local network");
  statusRow(112,"WI-FI","CONNECTED",TFT_GREEN);
  char signalText[20], ipText[20];
  snprintf(signalText,sizeof(signalText),"%d dBm",WiFi.RSSI());
  WiFi.localIP().toString().toCharArray(ipText,sizeof(ipText));
  statusRow(145,"SIGNAL",signalText,TFT_CYAN);
  statusRow(178,"IP ADDRESS",ipText,TFT_GREEN);
  statusRow(211,"GPS",gpsSeen?"DATA RECEIVED":"SEARCHING",gpsSeen?TFT_GREEN:TFT_YELLOW);
  drawButton(18,260,204,38,"CHANGE WI-FI",BLUE);
}

static void showFailed() {
  header("CONNECTION FAILED","Check the network name and password");
  statusRow(120,"NETWORK",wifiName.c_str(),TFT_YELLOW);
  statusRow(155,"STATUS","NOT CONNECTED",TFT_ORANGE);
  drawButton(12,215,216,38,"EDIT WI-FI",BLUE);
}

static void saveCredentials() {
  prefs.putString("ssid",wifiName);
  prefs.putString("password",wifiPassword);
}

static void beginWiFi() {
  if(!wifiName.length()) {
    setupMessage="Enter a network name first";
    showWifiSetup();
    return;
  }
  saveCredentials();
  WiFi.disconnect(true,false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiName.c_str(),wifiPassword.c_str());
  wifiAttemptAt=millis();
  screen=Screen::Connecting;
  showConnecting();
  Serial.printf("Wi-Fi: connecting to saved network '%s'\n",wifiName.c_str());
}

static void calibrateTouchIfNeeded() {
  if(prefs.getBytesLength("touchcal")==sizeof(touchCal)) {
    prefs.getBytes("touchcal",touchCal,sizeof(touchCal));
    tft.setTouch(touchCal);
    touchCalibrated=true;
    return;
  }
  header("TOUCH SETUP","Touch the calibration targets");
  tft.calibrateTouch(touchCal,TFT_MAGENTA,TFT_BLACK,15);
  prefs.putBytes("touchcal",touchCal,sizeof(touchCal));
  tft.setTouch(touchCal);
  touchCalibrated=true;
  showInitialization();
}

static void appendKey(char c) {
  String& target=editingPassword?wifiPassword:wifiName;
  const size_t maxLen=editingPassword?63:32;
  if(target.length()<maxLen) target += c;
}

static void backspace() {
  String& target=editingPassword?wifiPassword:wifiName;
  if(target.length()) target.remove(target.length()-1);
}

static void handleKeyboardTap(uint16_t x,uint16_t y) {
  if(y>=279 && y<=315 && x>=8 && x<=232) {
    setupMessage="";
    beginWiFi();
    return;
  }
  if(y>=59 && y<=90) { editingPassword=false; showWifiSetup(); return; }
  if(y>=102 && y<=133) {
    if(x>190) passwordVisible=!passwordVisible;
    else editingPassword=true;
    showWifiSetup(); return;
  }
  if(y>=157 && y<=182) {
    const char* keys=symbolsPage?"1234567890":"qwertyuiop";
    int x0=symbolsPage?4:4, kw=22, gap=1;
    int index=(x-x0)/(kw+gap);
    if(index>=0 && index<10 && (x-x0)%(kw+gap)<kw) {
      char c=keys[index];
      if(shiftKey && !symbolsPage) c=toupper(c);
      appendKey(c); showWifiSetup();
    }
    return;
  }
  if(y>=186 && y<=211) {
    const char* keys=symbolsPage?"@#$%&*-+()":"asdfghjkl";
    int count=9, x0=symbolsPage?4:14, kw=symbolsPage?22:23, gap=symbolsPage?1:2;
    int index=(x-x0)/(kw+gap);
    if(index>=0 && index<count && (x-x0)%(kw+gap)<kw) {
      char c=keys[index];
      if(shiftKey && !symbolsPage && c>='a' && c<='z') c=toupper(c);
      appendKey(c); showWifiSetup();
    }
    return;
  }
  if(y>=215 && y<=240) {
    if(x>=207) { backspace(); showWifiSetup(); return; }
    const char* keys=symbolsPage?"!?.,_:;/'":"zxcvbnm";
    int count=symbolsPage?9:7, x0=symbolsPage?8:7, kw=symbolsPage?23:25, gap=symbolsPage?2:3;
    int index=(x-x0)/(kw+gap);
    if(index>=0 && index<count && (x-x0)%(kw+gap)<kw) {
      char c=keys[index];
      if(shiftKey && !symbolsPage && c>='a' && c<='z') c=toupper(c);
      appendKey(c); showWifiSetup();
    }
    return;
  }
  if(y>=246 && y<=273) {
    if(x<47) shiftKey=!shiftKey;
    else if(x<138) appendKey(' ');
    else if(x<189) symbolsPage=!symbolsPage;
    else { String& target=editingPassword?wifiPassword:wifiName; target=""; }
    showWifiSetup();
  }
}

static void handleTouch() {
  if(!touchCalibrated || millis()-lastTouchAt<180) return;
  uint16_t x,y;
  if(!tft.getTouch(&x,&y,TOUCH_THRESHOLD)) return;
  lastTouchAt=millis();
  if(screen==Screen::WifiSetup) handleKeyboardTap(x,y);
  else if(screen==Screen::Connected && y>=250) {
    WiFi.disconnect();
    setupMessage="";
    editingPassword=false;
    showWifiSetup();
  } else if(screen==Screen::Failed && y>=205) {
    setupMessage="Check password, then connect again";
    showWifiSetup();
  }
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
  prefs.begin("cyd-tracker",false);
  wifiName=prefs.getString("ssid","");
  wifiPassword=prefs.getString("password","");
  if(!wifiName.length() && WIFI_SSID[0]) {
    wifiName=WIFI_SSID;
    wifiPassword=WIFI_PASSWORD;
  }
  showSplash();
  screen=Screen::Splash;
  screenStartedAt=millis();
}

void loop() {
  while(Serial2.available()) {
    if(gps.encode(Serial2.read()) && gps.location.isValid()) gpsSeen=true;
  }
  const uint32_t now=millis();
  if(screen==Screen::Splash && now-screenStartedAt>=SPLASH_MS) {
    showInitialization();
    calibrateTouchIfNeeded();
    screen=Screen::Init;
    screenStartedAt=millis();
  } else if(screen==Screen::Init && now-screenStartedAt>=INIT_MS) {
    if(wifiName.length()) beginWiFi();
    else { setupMessage="Enter your router details"; showWifiSetup(); }
  } else if(screen==Screen::Connecting) {
    if(WiFi.status()==WL_CONNECTED) {
      screen=Screen::Connected;
      showConnected();
      Serial.printf("Wi-Fi connected. IP=%s\n",WiFi.localIP().toString().c_str());
    } else if(now-wifiAttemptAt>=WIFI_TIMEOUT_MS) {
      screen=Screen::Failed;
      showFailed();
    }
  } else if(screen==Screen::Connected && now-screenStartedAt>=1000) {
    showConnected();
    screenStartedAt=now;
  }
  handleTouch();
}
