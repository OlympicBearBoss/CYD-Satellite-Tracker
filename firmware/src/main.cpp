#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <ctype.h>
#include <TJpg_Decoder.h>
#include "splash_jpg.h"
#include "BoardConfig.h"
#include "WifiConfig.h"

TFT_eSPI tft;
SPIClass touchSPI(VSPI);
TinyGPSPlus gps;
Preferences prefs;

static constexpr uint8_t DISPLAY_ROTATION = 1;
static constexpr uint32_t SPLASH_MS = 6000;
static constexpr uint32_t INIT_MS = 3500;
static constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
static constexpr uint16_t TOUCH_THRESHOLD = 250;
static constexpr uint16_t FIELD_HIT_TOP = 24;
static constexpr uint16_t FIELD_HIT_BOTTOM = 99;

enum class Screen : uint8_t { Splash, TouchTest, Init, WifiSetup, Connecting, Connected, Failed };
static Screen screen = Screen::Splash;
static uint32_t screenStartedAt = 0;
static uint32_t wifiAttemptAt = 0;
static uint32_t lastTouchAt = 0;
static bool wifiScreenDrawn = false;
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
static constexpr uint16_t PANEL = 0x2104;
static constexpr uint16_t BLUE = 0x001F;
static constexpr uint16_t MUTED = 0xFFFF;
static uint16_t touchCal[5];
static constexpr int TOUCH_CLK_PIN=25, TOUCH_MISO_PIN=39, TOUCH_MOSI_PIN=32, TOUCH_IRQ_PIN=36;
static bool touchWasDown=false; static uint8_t touchCalStep=0; static uint16_t touchCorners[8]={0};
static constexpr int16_t calX[4]={18,302,18,302}, calY[4]={18,18,222,222};
static uint16_t readXptChannel(uint8_t command){touchSPI.transfer(command);return (touchSPI.transfer16(0)>>3)&0x0FFF;}
static uint16_t median3(uint16_t a,uint16_t b,uint16_t c){
  if(a>b){uint16_t t=a;a=b;b=t;} if(b>c){uint16_t t=b;b=c;c=t;} if(a>b){uint16_t t=a;a=b;b=t;} return b;
}
static bool readXpt2046(uint16_t &x,uint16_t &y){
  if(digitalRead(TOUCH_IRQ_PIN)!=LOW)return false;
  uint16_t xs[3],ys[3];touchSPI.beginTransaction(SPISettings(6000000,MSBFIRST,SPI_MODE0));
  digitalWrite(TOUCH_CS,LOW);
  for(int i=0;i<3;i++){xs[i]=readXptChannel(0xD0);ys[i]=readXptChannel(0x90);}
  digitalWrite(TOUCH_CS,HIGH);touchSPI.endTransaction();
  x=median3(xs[0],xs[1],xs[2]);y=median3(ys[0],ys[1],ys[2]);return true;
}
static bool mapRawTouch(uint16_t rx,uint16_t ry,uint16_t &x,uint16_t &y){
  // Four-corner bilinear calibration compensates for panel skew and axis orientation.
  const float x00=touchCorners[0], y00=touchCorners[1];
  const float x10=touchCorners[2], y10=touchCorners[3];
  const float x01=touchCorners[4], y01=touchCorners[5];
  const float x11=touchCorners[6], y11=touchCorners[7];
  const float ax=x10-x00, ay=y10-y00, bx=x01-x00, by=y01-y00;
  const float cx=x11-x10-x01+x00, cy=y11-y10-y01+y00;
  const float det=ax*by-ay*bx;
  if(fabsf(det)<1000.0f) return false;
  const float dx=rx-x00, dy=ry-y00;
  float u=(dx*by-dy*bx)/det, v=(ax*dy-ay*dx)/det;
  for(uint8_t i=0;i<8;i++){
    const float fx=x00+ax*u+bx*v+cx*u*v-rx;
    const float fy=y00+ay*u+by*v+cy*u*v-ry;
    const float j00=ax+cx*v, j01=bx+cx*u, j10=ay+cy*v, j11=by+cy*u;
    const float jd=j00*j11-j01*j10;
    if(fabsf(jd)<1.0f) break;
    const float du=(fx*j11-fy*j01)/jd, dv=(j00*fy-j10*fx)/jd;
    u-=du; v-=dv;
    if(fabsf(du)+fabsf(dv)<0.0001f) break;
  }
  x=constrain((int)(18.0f+u*284.0f+0.5f),0,319);
  y=constrain((int)(18.0f+v*204.0f+0.5f),0,239);
  return true;
}
static void drawCalibrationTarget(uint8_t n){
  tft.fillScreen(TFT_BLACK);tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_CYAN,TFT_BLACK);
  tft.drawString("TOUCH CALIBRATION",8,8,2);char msg[32];snprintf(msg,sizeof(msg),"Tap target %u of 4",n+1);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);tft.drawString(msg,8,31,2);int x=calX[n],y=calY[n];
  tft.drawCircle(x,y,14,TFT_WHITE);tft.drawCircle(x,y,5,TFT_YELLOW);
  tft.drawFastHLine(x-20,y,40,TFT_CYAN);tft.drawFastVLine(x,y-20,40,TFT_CYAN);
}

static void printGeometry(const char* stage) {
  Serial.printf("CYD V39 | %s | rotation=%u | logical=%dx%d\n",
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

static bool splashOutput(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t* bitmap){
  if(y>=tft.height()) return false;
  for(uint32_t i=0;i<(uint32_t)w*h;i++) bitmap[i]=~bitmap[i]; // RGB565 negative correction for the splash image\n  tft.pushImage(x,y,w,h,bitmap);
  return true;
}
static void showSplash(){
  tft.fillScreen(TFT_BLACK);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(splashOutput);
  TJpgDec.drawJpg(0,0,splash_jpg,sizeof(splash_jpg));
  printGeometry("JPEG SPLASH VARIANT 4");
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
  tft.setTextColor((color==0x07FF||color==0xFFE0)?TFT_BLACK:TFT_WHITE,color);
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

static void beginWiFi();
static void backspace();

static void drawKey(int x,int y,int w,int h,const char* label, uint16_t fill=0x0000) {
  (void)fill;
  tft.fillRoundRect(x,y,w,h,3,TFT_BLACK);
  tft.drawRoundRect(x,y,w,h,3,TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString(label,x+w/2,y+h/2,2);
}
static void drawLetterRow(const char* keys,int y,int x,int keyW,int gap) {
  for(int i=0;keys[i];++i) {
    char label[2]={keys[i],0};
    if(shiftKey && !symbolsPage && label[0]>='a' && label[0]<='z') label[0]=toupper(label[0]);
    drawKey(x+i*(keyW+gap),y,keyW,22,label,0xFFFF);
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
    char k[2]={nums[r][c],0}; drawKey(246+c*24,108+r*25,22,22,k,0x0000);
  }
  drawKey(270,183,22,19,"0",0x0000);
  drawKey(5,184,40,19,russianLayout?"RU":"EN",0x0000);
  drawKey(48,184,43,19,"SHIFT",0x0000);
  drawKey(94,184,62,19,"SPACE",0x0000);
  drawKey(159,184,39,19,symbolsPage?"ABC":"SYM",0x0000);
  drawKey(201,184,42,19,"DEL",0x0000);
  drawButton(5,209,190,27,"CONNECT",0x07FF);
  tft.setTextDatum(MR_DATUM); tft.setTextColor(TFT_WHITE,BG);
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
  flashKeyboardKey(x,y);
  // CONNECT is deliberately limited to the left side. The DEL key is on
  // the right and must never start a connection, even with small Y drift.
  if(y>=209 && y<=239 && x>=5 && x<=195) { setupMessage=""; beginWiFi(); return; }
  // Generous, non-overlapping hit zones include the field captions as well as
  // the value boxes. This makes SSID/password selection easier on the small panel.
  if(y>=FIELD_HIT_TOP && y<65) {
    if(editingPassword) { editingPassword=false; showWifiSetup(); }
    return;
  }
  if(y>=65 && y<=FIELD_HIT_BOTTOM) {
    if(x>270 && y>=82) passwordVisible=!passwordVisible;
    else if(!editingPassword) { editingPassword=true; }
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
  statusRow(108,"WI-FI","CONNECTED",TFT_GREEN);
  char signalText[20], ipText[20];
  snprintf(signalText,sizeof(signalText),"%d dBm",WiFi.RSSI());
  WiFi.localIP().toString().toCharArray(ipText,sizeof(ipText));
  statusRow(140,"SIGNAL",signalText,TFT_CYAN);
  statusRow(172,"IP ADDRESS",ipText,TFT_GREEN);
  drawButton(58,204,204,30,"CHANGE WI-FI",BLUE);
}
static void showFailed() {
  header("CONNECTION FAILED","Check the network name and password");
  statusRow(120,"NETWORK",wifiName.c_str(),TFT_YELLOW);
  statusRow(155,"STATUS","NOT CONNECTED",TFT_ORANGE);
  drawButton(52,195,216,32,"EDIT WI-FI",BLUE);
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


static void showTouchDiagnostic() {
  screen=Screen::TouchTest;
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0,0,tft.width(),42,0x3186);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_YELLOW,0x3186); tft.drawString("TOUCH TEST / XPT2046",8,7,2);
  tft.setTextColor(TFT_WHITE,0x3186); tft.drawString("Tap once to start calibration",8,26,1);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString("Raw touch controller check",12,68,2);
  tft.drawString("X:",18,112,2); tft.drawString("Y:",18,145,2);
  tft.drawRoundRect(10,190,220,54,6,TFT_CYAN);
  tft.setTextColor(TFT_YELLOW,TFT_BLACK); tft.drawString("Touch anywhere to start",16,207,2);
  tft.setTextColor(TFT_WHITE,TFT_BLACK); tft.drawString("calibration",16,229,2);
  rawTouchDetected=false;
}

static void updateTouchDiagnostic(){
 uint16_t rx=0,ry=0;if(!readXpt2046(rx,ry)){touchWasDown=false;return;}
 if(touchWasDown||millis()-lastTouchAt<300)return;touchWasDown=true;lastTouchAt=millis();
 rawTouchX=rx;rawTouchY=ry;rawTouchDetected=true;Serial.printf("XPT2046 VSPI raw %u,%u\\n",rx,ry);
 touchCalStep=1;drawCalibrationTarget(0);
}
static void updateTouchCalibration(){
 uint16_t rx=0,ry=0;if(!readXpt2046(rx,ry)){touchWasDown=false;return;}
 if(touchWasDown||millis()-lastTouchAt<45)return;touchWasDown=true;lastTouchAt=millis();
 touchCorners[(touchCalStep-1)*2]=rx;touchCorners[(touchCalStep-1)*2+1]=ry;
 Serial.printf("Touch calibration %u/4 raw=%u,%u\\n",touchCalStep,rx,ry);++touchCalStep;
 if(touchCalStep<=4){drawCalibrationTarget(touchCalStep-1);return;}
 prefs.putBytes("touchraw",touchCorners,sizeof(touchCorners));prefs.putUChar("touchv",5);
 Serial.printf("Touch corners TL=%u,%u TR=%u,%u BL=%u,%u BR=%u,%u\\n",touchCorners[0],touchCorners[1],touchCorners[2],touchCorners[3],touchCorners[4],touchCorners[5],touchCorners[6],touchCorners[7]);
 touchCalibrated=true;screen=Screen::Init;showInitialization();screenStartedAt=millis();
}
static void backspace() {
  String& target=editingPassword?wifiPassword:wifiName;
  if(target.length()) target.remove(target.length()-1);
}

static void handleTouch(){
 if(!touchCalibrated||screen==Screen::TouchTest)return;
 // Always poll release first. This prevents a stale pressed-state from
 // blocking every subsequent key after a screen redraw.
 uint16_t rx=0,ry=0,x=0,y=0;
 if(!readXpt2046(rx,ry)){touchWasDown=false;return;}
 if(touchWasDown||millis()-lastTouchAt<6)return;
 touchWasDown=true;lastTouchAt=millis();
 if(!mapRawTouch(rx,ry,x,y)){
   Serial.printf("Touch map failed raw=%u,%u\\n",rx,ry);
   return;
 }
 Serial.printf("Touch raw=%u,%u mapped=%u,%u screen=%u\\n",rx,ry,x,y,(unsigned)screen);
 if(screen==Screen::WifiSetup)handleKeyboardTap(x,y);
 else if(screen==Screen::Connected&&y>=200){WiFi.disconnect();setupMessage="";editingPassword=false;showWifiSetup();}
 else if(screen==Screen::Failed&&y>=190){setupMessage="Check password, then connect again";showWifiSetup();}
}
void setup() {
  Serial.begin(115200);
  delay(150);
  pinMode(Pins::TFT_BACKLIGHT,OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT,HIGH);
  tft.init();
  tft.invertDisplay(false);
  pinMode(TOUCH_CS,OUTPUT);digitalWrite(TOUCH_CS,HIGH);
  pinMode(TOUCH_IRQ_PIN,INPUT);
  touchSPI.begin(TOUCH_CLK_PIN,TOUCH_MISO_PIN,TOUCH_MOSI_PIN,TOUCH_CS);
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
    if(touchCalStep>=1&&!touchCalibrated)updateTouchCalibration();else updateTouchDiagnostic();
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
