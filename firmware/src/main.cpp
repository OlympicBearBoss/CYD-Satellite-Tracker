#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "splash_jpg.h"
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
static constexpr uint16_t TOUCH_THRESHOLD = 350;

enum class Screen : uint8_t { Splash, TouchTest, Init, WifiSetup, Connecting, Connected, Failed };
static Screen screen = Screen::Splash;
static uint32_t screenStartedAt = 0;
static uint32_t wifiAttemptAt = 0;
static uint32_t lastTouchAt = 0;
static bool gpsSeen = false;
static bool touchCalibrated = false;
static uint16_t rawTouchX = 0, rawTouchY = 0;
static bool rawTouchDetected = false;
static bool symbolsPage = false;
static bool russianLayout = false;
static bool shiftKey = false;
static bool passwordVisible = false;
static bool editingPassword = false;
static String wifiName;
static String wifiPassword;
static String setupMessage;

static constexpr uint16_t BG = 0x0000;
static constexpr uint16_t PANEL = 0x3186;
static constexpr uint16_t BLUE = 0x001F;
static constexpr uint16_t MUTED = 0xFFFF;
static uint16_t touchCal[5];

static bool jpgOutput(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t* bitmap) {
  if(y>=tft.height()) return false;
  tft.pushImage(x,y,w,h,bitmap);
  return true;
}

static void printGeometry(const char* stage) {
  Serial.printf("CYD V32 | %s | rotation=%u | logical=%dx%d\n",
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
  tft.fillScreen(BG);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(jpgOutput);
  const JRESULT result=TJpgDec.drawJpg(0,0,splash_jpg,sizeof(splash_jpg));
  if(result!=JDR_OK) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,BG);
    tft.drawString("CYD satellite tracker",tft.width()/2,tft.height()/2,2);
    tft.drawString("by Olympic Bear",tft.width()/2,tft.height()/2+24,2);
    Serial.printf("JPEG splash decode error: %d\\n",(int)result);
  }
  printGeometry("JPEG SPLASH V33");
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
  tft.fillRoundRect(7,y,306,25,4,selected?0x123B:PANEL);
  tft.drawRoundRect(7,y,306,25,4,selected?TFT_CYAN:0x3151);
  String shown;
  if(mask && !passwordVisible) {
    for(size_t i=0;i<value.length();++i) shown += '*';
  } else shown=value;
  if(shown.length()>36) shown="..."+shown.substring(shown.length()-33);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE,selected?0x123B:PANEL);
  tft.drawString(shown,13,y+13,2);
}

static void drawKey(int x,int y,int w,int h,const char* label, uint16_t fill=0x001F) {
  tft.fillRoundRect(x,y,w,h,3,fill);
  tft.drawRoundRect(x,y,w,h,3,TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE,fill);
  tft.drawString(label,x+w/2,y+h/2,2);
}
static void drawLetterRow(const char* keys,int y,int x,int keyW,int gap) {
  for(int i=0;keys[i];++i) {
    char label[2]={keys[i],0};
    if(shiftKey && !symbolsPage && label[0]>='a' && label[0]<='z') label[0]=toupper(label[0]);
    drawKey(x+i*(keyW+gap),y,keyW,22,label);
  }
}
static void showWifiSetup() {
  screen=Screen::WifiSetup;
  tft.fillScreen(BG);
  tft.fillRect(0,0,tft.width(),27,PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN,PANEL); tft.drawString("WI-FI SETUP",7,5,2);
  tft.setTextColor(TFT_WHITE,PANEL); tft.drawString(russianLayout?"RU":"EN",275,6,2);
  drawField(37,"NETWORK / SSID",wifiName,!editingPassword,false);
  drawField(68,"PASSWORD",wifiPassword,editingPassword,true);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(setupMessage.length()?TFT_YELLOW:MUTED,BG);
  tft.drawString(setupMessage.length()?setupMessage.c_str():"Select field, then enter characters",7,98,1);
  if(!symbolsPage) {
    drawLetterRow("qwertyuiop",108,5,21,1);
    drawLetterRow("asdfghjkl",133,16,22,2);
    drawLetterRow("zxcvbnm",158,40,22,3);
  } else {
    drawLetterRow("!@#$%^&*()",108,5,21,1);
    drawLetterRow("-_=+[]{};:",133,16,22,2);
    drawLetterRow(".,?/'\\|~",158,40,22,3);
  }
  const char* nums[3]={"123","456","789"};
  for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
    char k[2]={nums[r][c],0}; drawKey(246+c*24,108+r*25,22,22,k,0x041F);
  }
  drawKey(270,183,22,19,"0",0x041F);
  drawKey(5,184,40,19,russianLayout?"RU":"EN",0x03EF);
  drawKey(48,184,43,19,"SHIFT",0x001F);
  drawKey(94,184,62,19,"SPACE",0x001F);
  drawKey(159,184,39,19,symbolsPage?"ABC":"SYM",0x001F);
  drawKey(201,184,42,19,"DEL",0x7800);
  drawButton(5,209,310,27,"CONNECT",BLUE);
  tft.setTextDatum(MR_DATUM); tft.setTextColor(TFT_CYAN,BG);
  tft.drawString(passwordVisible?"HIDE":"SHOW",315,91,1);
}
static void appendKeyUtf8(const char* value) {
  String& target=editingPassword?wifiPassword:wifiName;
  const size_t maxLen=editingPassword?63:32;
  if(target.length()+strlen(value)<=maxLen) target += value;
}
static void appendMappedKey(char c) {
  if(!russianLayout || symbolsPage) {
    if(shiftKey && c>='a' && c<='z') c=toupper(c);
    char value[2]={c,0}; appendKeyUtf8(value); return;
  }
  const char* en="qwertyuiopasdfghjklzxcvbnm";
  const char* ru[]={"й","ц","у","к","е","н","г","ш","щ","з","ф","ы","в","а","п","р","о","л","д","я","ч","с","м","и","т","ь"};
  const char* upper[]={"Й","Ц","У","К","Е","Н","Г","Ш","Щ","З","Ф","Ы","В","А","П","Р","О","Л","Д","Я","Ч","С","М","И","Т","Ь"};
  for(size_t i=0;i<strlen(en);++i) if(en[i]==c) {
    appendKeyUtf8(shiftKey?upper[i]:ru[i]); return;
  }
  char value[2]={c,0}; appendKeyUtf8(value);
}
static void handleKeyboardTap(uint16_t x,uint16_t y) {
  if(y>=209 && x>=5 && x<=315) { setupMessage=""; beginWiFi(); return; }
  if(y>=37 && y<=63) { editingPassword=false; showWifiSetup(); return; }
  if(y>=68 && y<=94) {
    if(x>270) passwordVisible=!passwordVisible; else editingPassword=true;
    showWifiSetup(); return;
  }
  if(y>=108 && y<=130) {
    if(x>=246) { int c=(x-246)/24; if(c>=0&&c<3) appendMappedKey("123"[c]); }
    else { int i=(x-5)/22; if(i>=0&&i<10) appendMappedKey((symbolsPage?"!@#$%^&*()":"qwertyuiop")[i]); }
    showWifiSetup(); return;
  }
  if(y>=133 && y<=155) {
    if(x>=246) { int c=(x-246)/24; if(c>=0&&c<3) appendMappedKey("456"[c]); }
    else { int i=(x-16)/24; if(i>=0&&i<9) appendMappedKey((symbolsPage?"-_=+[]{};:":"asdfghjkl")[i]); }
    showWifiSetup(); return;
  }
  if(y>=158 && y<=180) {
    if(x>=246) { int c=(x-246)/24; if(c>=0&&c<3) appendMappedKey("789"[c]); }
    else if(x>=207) backspace();
    else { int i=(x-40)/25; if(i>=0&&i<7) appendMappedKey((symbolsPage?".,?/'\\|~":"zxcvbnm")[i]); }
    showWifiSetup(); return;
  }
  if(y>=183 && y<=205) {
    if(x<46) russianLayout=!russianLayout;
    else if(x<93) shiftKey=!shiftKey;
    else if(x<158) appendKeyUtf8(" ");
    else if(x<200) symbolsPage=!symbolsPage;
    else if(x<244) backspace();
    else if(x>=270&&x<=294) appendMappedKey('0');
    showWifiSetup();
  }
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

static void calibrateTouchIfNeeded();

static void showTouchDiagnostic() {
  screen=Screen::TouchTest;
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0,0,tft.width(),42,0x3186);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_YELLOW,0x3186); tft.drawString("TOUCH TEST / XPT2046",8,7,2);
  tft.setTextColor(TFT_WHITE,0x3186); tft.drawString("Touch the screen",8,26,1);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString("Raw touch controller check",12,68,2);
  tft.drawString("X:",18,112,2); tft.drawString("Y:",18,145,2);
  tft.drawRoundRect(10,190,220,54,6,TFT_CYAN);
  tft.setTextColor(TFT_YELLOW,TFT_BLACK); tft.drawString("Touch anywhere to start",16,207,2);
  tft.setTextColor(TFT_WHITE,TFT_BLACK); tft.drawString("calibration",16,229,2);
  rawTouchDetected=false;
}

static void updateTouchDiagnostic() {
  uint16_t x=0,y=0;
  if(tft.getTouchRaw(&x,&y)) {
    rawTouchX=x; rawTouchY=y; rawTouchDetected=true;
    Serial.printf("XPT2046 raw touch: X=%u Y=%u\\n",x,y);
    tft.fillRect(55,108,165,48,TFT_BLACK);
    tft.setTextColor(TFT_GREEN,TFT_BLACK);
    tft.drawNumber(rawTouchX,55,108,2); tft.drawNumber(rawTouchY,55,141,2);
    delay(250);
    calibrateTouchIfNeeded();
    screen=Screen::Init;
    screenStartedAt=millis();
  }
}
static void calibrateTouchIfNeeded() {
  if(prefs.getUChar("touchv",0)==3 && prefs.getBytesLength("touchcal")==sizeof(touchCal)) {
    prefs.getBytes("touchcal",touchCal,sizeof(touchCal));
    tft.setTouch(touchCal);
    touchCalibrated=true;
    return;
  }
  header("TOUCH SETUP","Touch the calibration targets");
  tft.calibrateTouch(touchCal,TFT_MAGENTA,TFT_BLACK,15);
  prefs.putBytes("touchcal",touchCal,sizeof(touchCal));
  prefs.putUChar("touchv",3);
  tft.setTouch(touchCal);
  touchCalibrated=true;
  showInitialization();
}

static void backspace() {
  String& target=editingPassword?wifiPassword:wifiName;
  if(target.length()) target.remove(target.length()-1);
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
    showTouchDiagnostic();
    screenStartedAt=now;
  } else if(screen==Screen::TouchTest) {
    updateTouchDiagnostic();
  } else if(screen==Screen::Init && now-screenStartedAt>=INIT_MS) {
    if(wifiName.length()) beginWiFi();
    else { setupMessage="Enter your router details"; showWifiSetup(); }
  } else if(screen==Screen::Connecting) {
    if(WiFi.status()==WL_CONNECTED) {
      screen=Screen::Connected;
      screenStartedAt=now;
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
