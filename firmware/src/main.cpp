#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <ctype.h>
#include <TJpg_Decoder.h>
#include "splash_jpg.h"
#include "ui_assets.h"
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

enum class Screen : uint8_t { Splash, TouchTest, Init, WifiSetup, Connecting, Connected, Dashboard, Satellite, Antenna, SystemInfo, Failed };
static Screen screen = Screen::Splash;
static uint32_t screenStartedAt = 0;
static uint32_t wifiAttemptAt = 0;
static uint32_t lastTouchAt = 0;
static uint32_t lastDashboardRefreshAt = 0;
static bool wifiScreenDrawn = false;
static bool keyboardNeedsRedraw = true;
static bool gpsSeen = false;
static bool touchCalibrated = false;
static bool calibrationFromSystem = false;
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

// Space Dark palette: deep navy background, layered blue panels and cyan telemetry.
static constexpr uint16_t BG = 0x0841;
static constexpr uint16_t PANEL = 0x10A3;
static constexpr uint16_t BLUE = 0x03B9;
static constexpr uint16_t MUTED = 0x9CF3;
static constexpr uint16_t KEY_BG = 0x10A3;
static constexpr uint16_t KEY_NUM = 0x035F;
static constexpr uint16_t KEY_EDGE = 0x07FF;
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
  uint16_t xs[5],ys[5];touchSPI.beginTransaction(SPISettings(1000000,MSBFIRST,SPI_MODE0));
  digitalWrite(TOUCH_CS,LOW);
  for(int i=0;i<5;i++){xs[i]=readXptChannel(0xD0);ys[i]=readXptChannel(0x90);}
  digitalWrite(TOUCH_CS,HIGH);touchSPI.endTransaction();
  // Median-of-five rejects occasional noisy samples without delaying response.
  for(int i=0;i<5;i++)for(int j=i+1;j<5;j++){if(xs[j]<xs[i]){uint16_t t=xs[i];xs[i]=xs[j];xs[j]=t;}if(ys[j]<ys[i]){uint16_t t=ys[i];ys[i]=ys[j];ys[j]=t;}}
  x=xs[2];y=ys[2];return true;
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
static bool loadTouchCalibration() {
  if (prefs.getUChar("touchv",0) != 5) return false;
  if (prefs.getBytesLength("touchraw") != sizeof(touchCorners)) return false;
  if (prefs.getBytes("touchraw",touchCorners,sizeof(touchCorners)) != sizeof(touchCorners)) return false;
  for (uint8_t i=0;i<8;i++) if (touchCorners[i] < 100 || touchCorners[i] > 4000) return false;
  const float ax=(float)touchCorners[2]-touchCorners[0];
  const float ay=(float)touchCorners[3]-touchCorners[1];
  const float bx=(float)touchCorners[4]-touchCorners[0];
  const float by=(float)touchCorners[5]-touchCorners[1];
  if (fabsf(ax*by-ay*bx) < 1000.0f) return false;
  touchCalibrated=true;
  Serial.println("Touch calibration restored from NVS");
  return true;
}

static void drawCalibrationTarget(uint8_t n){
  // Calibration keeps the exact four raw sampling coordinates, but now uses
  // the same Space Dark visual language as the rest of the interface.
  tft.fillScreen(BG);
  for(int i=0;i<18;i++){int sx=(i*47+19)%320, sy=(i*31+13)%240;tft.drawPixel(sx,sy,0x19A5);}
  tft.fillRect(0,0,320,35,0x0862); tft.fillRect(0,35,320,2,TFT_CYAN);
  tft.fillCircle(15,17,6,TFT_CYAN); tft.fillCircle(15,17,2,TFT_WHITE);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_WHITE,0x0862);
  tft.drawString("TOUCH SETUP",28,8,2);
  tft.setTextDatum(TR_DATUM);tft.setTextColor(MUTED,0x0862);
  tft.drawString("XPT2046",309,11,1);
  tft.drawRoundRect(54,78,212,88,12,0x2A69);
  tft.fillRoundRect(59,83,202,78,9,0x1028);
  char msg[32];snprintf(msg,sizeof(msg),"POINT %u / 4",n+1);
  tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_CYAN,0x1028);
  tft.drawString(msg,160,103,2);
  tft.setTextColor(TFT_WHITE,0x1028);tft.drawString("Touch the target",160,128,2);
  tft.setTextColor(MUTED,0x1028);tft.drawString("Keep finger steady",160,148,1);
  tft.drawRoundRect(65,187,190,9,4,0x2A69);
  tft.fillRoundRect(67,189,(uint16_t)(186*(n+1)/4),5,2,TFT_CYAN);
  tft.setTextDatum(BC_DATUM);tft.setTextColor(MUTED,BG);
  tft.drawString("4-CORNER PRECISION CALIBRATION",160,222,1);
  int x=calX[n],y=calY[n];
  tft.drawCircle(x,y,14,TFT_WHITE);tft.drawCircle(x,y,9,TFT_CYAN);
  tft.fillCircle(x,y,3,TFT_YELLOW);
  tft.drawFastHLine(x-20,y,40,TFT_CYAN);tft.drawFastVLine(x,y-20,40,TFT_CYAN);
  tft.drawCircle(x,y,19,0x2A69);
}

static void printGeometry(const char* stage) {
  Serial.printf("CYD V55 | %s | rotation=%u | logical=%dx%d\n",
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
  const uint32_t pixels=(uint32_t)w*(uint32_t)h;
  for(uint32_t i=0;i<pixels;++i) bitmap[i]=(uint16_t)~bitmap[i];
  tft.pushImage(x,y,w,h,bitmap);
  return true;
}
static bool uiImageOutput(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t* bitmap){
  if(x>=tft.width() || y>=tft.height()) return false;
  tft.pushImage(x,y,w,h,bitmap);
  return true;
}
static void drawUiImage(int16_t x,int16_t y,const uint8_t* data,uint32_t length){
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(uiImageOutput);
  TJpgDec.drawJpg(x,y,data,length);
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
  // Restrained starfield and layered top chrome; no full-screen animation.
  for(int i=0;i<22;i++){int sx=(i*53+17)%320,sy=(i*29+9)%238;tft.drawPixel(sx,sy,(i%3)?0x19A5:0x2A69);}
  tft.fillRect(0,0,320,30,0x0862);
  tft.fillRect(0,29,320,1,0x2A69);tft.fillRect(0,30,320,2,TFT_CYAN);
  tft.fillCircle(16,15,8,0x03B9);tft.fillCircle(16,15,4,TFT_WHITE);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_WHITE,0x0862);tft.drawString("CYD",30,5,2);
  tft.setTextColor(MUTED,0x0862);tft.drawString("SATELLITE TRACKER",64,9,1);
  tft.fillRoundRect(275,7,35,16,7,WiFi.status()==WL_CONNECTED?0x0320:0x4000);
  tft.fillCircle(283,15,3,WiFi.status()==WL_CONNECTED?TFT_GREEN:TFT_ORANGE);
  tft.setTextDatum(MR_DATUM);tft.setTextColor(TFT_WHITE,WiFi.status()==WL_CONNECTED?0x0320:0x4000);
  tft.drawString(WiFi.status()==WL_CONNECTED?"LINK":"OFF",306,15,1);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_WHITE,BG);tft.drawString(title,11,38,4);
  tft.setTextColor(MUTED,BG);tft.drawString(subtitle,13,73,1);
  tft.drawFastHLine(10,83,300,0x2A69);
}
static void drawSectionLabel(const char* label,int x,int y,int w) {
  tft.setTextDatum(TL_DATUM); tft.setTextColor(TFT_CYAN,PANEL); tft.drawString(label,x,y,1);
  tft.drawFastHLine(x,y+13,w,0x2A69);
}
static void drawAzimuthDial(int cx,int cy,int r) {
  tft.drawCircle(cx,cy,r,0x2A69); tft.drawCircle(cx,cy,r-5,0x1A45);
  for(int a=0;a<360;a+=30) {
    const float rad=(a-90)*0.0174532925f;
    int x1=cx+(int)((r-3)*cosf(rad)), y1=cy+(int)((r-3)*sinf(rad));
    int x2=cx+(int)((r-(a%90==0?10:6))*cosf(rad)), y2=cy+(int)((r-(a%90==0?10:6))*sinf(rad));
    tft.drawLine(x1,y1,x2,y2,a%90==0?TFT_CYAN:0x2A69);
  }
  tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_WHITE,PANEL);
  tft.drawString("N",cx,cy-r+8,1); tft.drawString("E",cx+r-8,cy,1);
  tft.drawString("S",cx,cy+r-8,1); tft.drawString("W",cx-r+8,cy,1);
  tft.fillCircle(cx,cy,3,TFT_CYAN);
}
static void drawElevationArc(int x,int y,int w) {
  tft.drawRoundRect(x,y,w,50,7,0x2A69); tft.drawFastHLine(x+10,y+37,w-20,0x2A69);
  for(int i=0;i<=6;i++){int px=x+12+i*(w-24)/6;tft.drawFastVLine(px,y+32,8,i==3?TFT_CYAN:0x2A69);}
  tft.setTextDatum(TL_DATUM); tft.setTextColor(MUTED,PANEL);
  tft.drawString("0",x+10,y+39,1); tft.drawString("90",x+w-23,y+39,1);
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
  tft.setTextColor(selected?TFT_CYAN:MUTED,BG);tft.drawString(label,10,y-12,1);
  const uint16_t fill=selected?0x123B:0x0C63;
  tft.fillRoundRect(7,y,306,25,5,fill);
  tft.drawRoundRect(7,y,306,25,5,selected?TFT_CYAN:0x3151);
  tft.fillRoundRect(11,y+5,3,15,1,selected?TFT_CYAN:0x3151);
  String shown;
  if(mask && !passwordVisible) {for(size_t i=0;i<value.length();++i) shown += '*';}
  else shown=value;
  if(shown.length()>36) shown="..."+shown.substring(shown.length()-33);
  tft.setTextDatum(ML_DATUM);tft.setTextColor(TFT_WHITE,fill);tft.drawString(shown,19,y+13,2);
}

static bool readStableTouch(uint16_t &rawX,uint16_t &rawY,uint16_t &screenX,uint16_t &screenY) {
  // Sample three independent controller bursts while the finger is still down.
  // Each burst is already median-filtered; median the mapped coordinates again
  // to reject occasional XPT2046 spikes near adjacent keyboard hit zones.
  uint16_t xs[3],ys[3],rx[3],ry[3];
  for(uint8_t i=0;i<3;++i) {
    if(!readXpt2046(rx[i],ry[i])) return false;
    if(!mapRawTouch(rx[i],ry[i],xs[i],ys[i])) return false;
    if(i<2) delay(2);
  }
  rawX=median3(rx[0],rx[1],rx[2]);
  rawY=median3(ry[0],ry[1],ry[2]);
  screenX=median3(xs[0],xs[1],xs[2]);
  screenY=median3(ys[0],ys[1],ys[2]);
  return true;
}

static void beginWiFi();
static void backspace();

static void drawKey(int x,int y,int w,int h,const char* label, uint16_t fill=0x0000) {
  const uint16_t edge=(fill==KEY_NUM)?0x2F9F:0x3A77;
  tft.fillRoundRect(x,y+2,w,h,4,0x0000);
  tft.fillRoundRect(x,y,w,h,4,fill);
  tft.drawRoundRect(x,y,w,h,4,edge);
  tft.drawFastHLine(x+4,y+1,w-8,fill==KEY_NUM?0x5DFF:0x2E9F);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fill==0x07FF?TFT_BLACK:TFT_WHITE,fill);
  tft.drawString(label,x+w/2,y+h/2,2);
}
static void drawLetterRow(const char* keys,int y,int x,int keyW,int gap) {
  for(int i=0;keys[i];++i) {
    char label[2]={keys[i],0};
    if(shiftKey && !symbolsPage && label[0]>='a' && label[0]<='z') label[0]=toupper(label[0]);
    drawKey(x+i*(keyW+gap),y,keyW,22,label,KEY_BG);
  }
}
static void flashKeyboardKey(uint16_t x,uint16_t y) {
  int kx=-1,ky=0,kw=0,kh=22; char label[8]={0};
  if(y>=108&&y<=130) {
    ky=108;
    if(x>=246) { int c=(x-246)/24; if(c>2)return; kx=246+c*24;kw=22;label[0]="123"[c]; }
    else { int c=(x-5)/22;if(c<0||c>=10)return;kx=5+c*22;kw=21;label[0]=(symbolsPage?"!@#$%^&*()":"qwertyuiop")[c]; }
  } else if(y>=133&&y<=155) {
    ky=133;
    if(x>=246) { int c=(x-246)/24;if(c>2)return;kx=246+c*24;kw=22;label[0]="456"[c]; }
    else { int c=(x-16)/24;if(c<0||c>=9)return;kx=16+c*24;kw=22;label[0]=(symbolsPage?"-_=+[]{};:":"asdfghjkl")[c]; }
  } else if(y>=158&&y<=180) {
    ky=158;
    if(x>=246) { int c=(x-246)/24;if(c>2)return;kx=246+c*24;kw=22;label[0]="789"[c]; }
    else { int c=(x-40)/25;if(c<0||c>=7)return;kx=40+c*25;kw=22;label[0]=(symbolsPage?".,?/'\\\\|~":"zxcvbnm")[c]; }
  } else if(y>=183&&y<=205) {
    ky=184;kh=19;
    if(x>=5&&x<45){kx=5;kw=40;strcpy(label,russianLayout?"RU":"EN");}
    else if(x>=48&&x<91){kx=48;kw=43;strcpy(label,"SHIFT");}
    else if(x>=94&&x<156){kx=94;kw=62;strcpy(label,"SPACE");}
    else if(x>=159&&x<198){kx=159;kw=39;strcpy(label,symbolsPage?"ABC":"SYM");}
    else if(x>=201&&x<243){kx=201;kw=42;strcpy(label,"DEL");}
    else if(x>=270&&x<292){kx=270;kw=22;strcpy(label,"0");}
    else return;
  } else return;
  if(x<kx||x>=kx+kw)return;
  drawKey(kx,ky,kw,kh,label,KEY_NUM);
  delay(45);
  drawKey(kx,ky,kw,kh,label,KEY_BG);
}
static void showWifiSetup() {
  const bool firstDraw = (screen != Screen::WifiSetup) || !wifiScreenDrawn;
  screen=Screen::WifiSetup;
  if(firstDraw) {
    tft.fillScreen(BG);
    for(int i=0;i<12;i++){int sx=(i*59+23)%320,sy=(i*37+7)%100;tft.drawPixel(sx,sy,0x19A5);}
    tft.fillRect(0,0,320,27,0x0862);tft.fillRect(0,27,320,2,TFT_CYAN);
    tft.fillCircle(14,13,5,TFT_CYAN);tft.fillCircle(14,13,2,TFT_WHITE);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE,0x0862);tft.drawString("NETWORK CONFIG",26,5,2);
    tft.setTextDatum(TR_DATUM);tft.setTextColor(MUTED,0x0862);tft.drawString("SPACE DARK",263,8,1);
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE,PANEL); tft.drawString(russianLayout?"RU":"EN",275,6,2);
  drawField(37,"NETWORK / SSID",wifiName,!editingPassword,false);
  drawField(68,"PASSWORD",wifiPassword,editingPassword,true);
  tft.fillRoundRect(7,96,306,10,4,0x1028);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(setupMessage.length()?TFT_YELLOW:MUTED,0x1028);
  tft.drawString(setupMessage.length()?setupMessage.c_str():"SELECT A FIELD  /  TYPE YOUR WI-FI DETAILS",12,97,1);
  if(firstDraw || keyboardNeedsRedraw) {
    if(!symbolsPage) {
      drawLetterRow("qwertyuiop",108,5,21,1);
      drawLetterRow("asdfghjkl",133,16,22,2);
      drawLetterRow("zxcvbnm",158,40,22,3);
    } else {
      drawLetterRow("!@#$%^&*()",108,5,21,1);
      drawLetterRow("-_=+[]{};:",133,16,22,2);
      drawLetterRow(".,?/'\\\\|~",158,40,22,3);
    }
    const char* nums[3]={"123","456","789"};
    for(int r=0;r<3;++r) for(int col=0;col<3;++col) {
      char k[2]={nums[r][col],0}; drawKey(246+col*24,108+r*25,22,22,k,KEY_NUM);
    }
    drawKey(270,183,22,19,"0",KEY_NUM);
    drawKey(5,184,40,19,russianLayout?"RU":"EN",KEY_BG);
    drawKey(48,184,43,19,"SHIFT",KEY_BG);
    drawKey(94,184,62,19,"SPACE",KEY_BG);
    drawKey(159,184,39,19,symbolsPage?"ABC":"SYM",KEY_BG);
    drawKey(201,184,42,19,"DEL",KEY_BG);
    drawButton(5,209,190,27,"CONNECT",0x07FF);
  
    keyboardNeedsRedraw=false;
  }
  tft.setTextDatum(MR_DATUM); tft.setTextColor(TFT_WHITE,BG);
  tft.drawString(passwordVisible?"HIDE":"SHOW",315,91,1);
  wifiScreenDrawn=true;
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
  Serial.printf("KEYBOARD HIT x=%u y=%u page=%u field=%u\\n",x,y,(unsigned)symbolsPage,(unsigned)editingPassword);
  flashKeyboardKey(x,y);
  if(y>=209 && y<=239 && x>=5 && x<=195) { setupMessage=""; beginWiFi(); return; }
  if(y>=FIELD_HIT_TOP && y<65) {
    if(editingPassword) { editingPassword=false; showWifiSetup(); }
    return;
  }
  if(y>=65 && y<=FIELD_HIT_BOTTOM) {
    if(x>270 && y>=82) passwordVisible=!passwordVisible;
    else if(!editingPassword) editingPassword=true;
    showWifiSetup(); return;
  }
  // Hit zones mirror the actual drawn rectangles. Keep the numeric keypad
  // independent from the letter-row spacing (22/24/25 px differ by row).
  if(y>=108 && y<130) {
    if(x>=246 && x<318) {
      const int col=(x-246)/24;
      if(col<3 && x<(246+col*24+22)) { Serial.printf("KEYMAP digit=%c mapped=%u,%u raw=%u,%u\\n","123"[col],x,y,rawTouchX,rawTouchY); appendMappedKey("123"[col]); }
    } else if(x>=5 && x<225) {
      const int col=(x-5)/22;
      if(col<10 && x<5+col*22+21) appendMappedKey((symbolsPage?"!@#$%^&*()":"qwertyuiop")[col]);
    }
    showWifiSetup(); return;
  }
  if(y>=133 && y<155) {
    if(x>=246 && x<318) {
      const int col=(x-246)/24;
      if(col<3 && x<(246+col*24+22)) { Serial.printf("KEYMAP digit=%c mapped=%u,%u raw=%u,%u\\n","456"[col],x,y,rawTouchX,rawTouchY); appendMappedKey("456"[col]); }
    } else if(x>=16 && x<232) {
      const int col=(x-16)/26;
      if(col<9 && x<16+col*26+22) appendMappedKey((symbolsPage?"-_=+[]{};:":"asdfghjkl")[col]);
    }
    showWifiSetup(); return;
  }
  if(y>=158 && y<180) {
    if(x>=246 && x<318) {
      const int col=(x-246)/24;
      if(col<3 && x<(246+col*24+22)) { Serial.printf("KEYMAP digit=%c mapped=%u,%u raw=%u,%u\\n","789"[col],x,y,rawTouchX,rawTouchY); appendMappedKey("789"[col]); }
    } else if(x>=40 && x<215) {
      const int col=(x-40)/25;
      if(col<7 && x<40+col*25+22) appendMappedKey((symbolsPage?".,?/'\\\\|~":"zxcvbnm")[col]);
    }
    showWifiSetup(); return;
  }
  if(y>=183 && y<=205) {
    if(x>=5&&x<45) { russianLayout=!russianLayout; keyboardNeedsRedraw=true; }
    else if(x>=48&&x<91) { shiftKey=!shiftKey; keyboardNeedsRedraw=true; }
    else if(x>=94&&x<156) appendKeyUtf8(" ");
    else if(x>=159&&x<198) { symbolsPage=!symbolsPage; keyboardNeedsRedraw=true; }
    else if(x>=201&&x<243) backspace();
    else if(x>=270&&x<292) appendMappedKey('0');
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

static void drawNavigation(uint8_t active) {
  const uint16_t activeColor=0x07FF, idleColor=0x001F;
  drawButton(4,207,76,29,"HOME",active==0?activeColor:idleColor);
  drawButton(83,207,76,29,"SAT",active==1?activeColor:idleColor);
  drawButton(162,207,76,29,"ANT",active==2?activeColor:idleColor);
  drawButton(241,207,76,29,"SYSTEM",active==3?activeColor:idleColor);
}

static void updateGpsTelemetry();


static void drawEarthArtwork(int cx,int cy,int r) {
  // Layered shaded globe, limb glow and simplified continental silhouettes.
  tft.fillCircle(cx+3,cy+4,r+4,0x0000);
  tft.drawCircle(cx,cy,r+4,0x07FF);
  for(int k=r;k>0;--k) {
    uint16_t c = k>r*3/4 ? 0x0010 : (k>r/2 ? 0x0016 : (k>r/4 ? 0x021B : 0x043F));
    tft.drawCircle(cx,cy,k,c);
  }
  // Ocean highlight and curved latitude/longitude traces.
  tft.drawEllipse(cx,cy,r-7,r/3,0x04BF);
  tft.drawEllipse(cx,cy,r/2,r-8,0x039F);
  tft.drawFastHLine(cx-r+8,cy,r*2-16,0x035F);
  // Stylized continents, intentionally subdued beneath telemetry.
  tft.fillTriangle(cx-25,cy-24,cx-5,cy-31,cx-8,cy-8,0x36A6);
  tft.fillTriangle(cx-25,cy-24,cx-8,cy-8,cx-20,cy+1,0x36A6);
  tft.fillTriangle(cx-8,cy-8,cx+1,cy+4,cx-11,cy+25,0x2E85);
  tft.fillTriangle(cx+9,cy-24,cx+30,cy-16,cx+21,cy-2,0x36A6);
  tft.fillTriangle(cx+9,cy-24,cx+21,cy-2,cx+5,cy+4,0x36A6);
  tft.fillTriangle(cx+5,cy+4,cx+21,cy-2,cx+15,cy+18,0x2E85);
  // Atmospheric rim and orbit.
  for(int i=0;i<72;i++) {
    float a=i*0.0872665f;
    int px=cx+(int)((r+2)*cosf(a)), py=cy+(int)((r+2)*sinf(a));
    tft.drawPixel(px,py,(i%3)?0x05BF:TFT_CYAN);
  }
  int ox=cx, oy=cy;
  for(int i=0;i<=72;i++) {
    float a=i*0.0872665f;
    int px=cx+(int)((r+15)*cosf(a)), py=cy+(int)((r/2+9)*sinf(a));
    if(i) tft.drawLine(ox,oy,px,py,0x4D7F);
    ox=px;oy=py;
  }
  tft.fillCircle(cx+r+12,cy-5,3,TFT_YELLOW);
  tft.fillCircle(cx+r+12,cy-5,1,TFT_WHITE);
}
static void drawSatelliteArtwork(int cx,int cy,int scale) {
  tft.fillRect(cx-23*scale,cy-8*scale,14*scale,16*scale,0x035F);
  tft.drawRect(cx-23*scale,cy-8*scale,14*scale,16*scale,TFT_CYAN);
  for(int i=1;i<4;i++) tft.drawFastVLine(cx-23*scale+i*3*scale,cy-7*scale,14*scale,0x07FF);
  tft.fillRect(cx+9*scale,cy-8*scale,14*scale,16*scale,0x035F);
  tft.drawRect(cx+9*scale,cy-8*scale,14*scale,16*scale,TFT_CYAN);
  for(int i=1;i<4;i++) tft.drawFastVLine(cx+9*scale+i*3*scale,cy-7*scale,14*scale,0x07FF);
  tft.fillRoundRect(cx-8*scale,cy-6*scale,16*scale,12*scale,3*scale,0xBDF7);
  tft.drawRoundRect(cx-8*scale,cy-6*scale,16*scale,12*scale,3*scale,TFT_WHITE);
  tft.drawLine(cx,cy-6*scale,cx+4*scale,cy-14*scale,TFT_WHITE);
  tft.fillCircle(cx+4*scale,cy-15*scale,2*scale,TFT_YELLOW);
}
static void drawDialNeedle(int cx,int cy,int length,float degrees,uint16_t color) {
  const float a=(degrees-90.0f)*0.0174532925f;
  int ex=cx+(int)(length*cosf(a)), ey=cy+(int)(length*sinf(a));
  tft.drawLine(cx+1,cy+2,ex+1,ey+2,0x0000);
  tft.drawLine(cx,cy,ex,ey,color);
  tft.fillCircle(cx,cy,5,0xBDF7);
  tft.drawCircle(cx,cy,5,TFT_WHITE);
}

static void drawGpsPanel() {
  tft.fillRoundRect(8,88,304,109,9,0x0C63);
  tft.drawRoundRect(8,88,304,109,9,0x3151);
  tft.fillRoundRect(13,93,137,99,7,0x0010);
  drawEarthArtwork(78,143,37);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_CYAN,0x0010);
  tft.drawString("LIVE ORBIT",18,98,1);
  tft.fillRoundRect(156,93,150,99,7,0x0862);
  tft.drawRoundRect(156,93,150,99,7,0x3151);
  drawSectionLabel("GPS TELEMETRY",164,99,133);
  updateGpsTelemetry();
  tft.fillRect(8,199,304,1,0x2A69);
}
static void updateGpsTelemetry() {
  const bool fix=gps.location.isValid() && gps.location.age()<10000;
  char line[40];
  tft.fillRect(139,117,159,64,0x0862);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fix?TFT_GREEN:TFT_YELLOW,0x0862);
  tft.drawString(fix?"3D POSITION FIX":"SEARCHING FOR GPS",141,119,1);
  snprintf(line,sizeof(line),"SAT %lu",(unsigned long)(gps.satellites.isValid()?gps.satellites.value():0));
  tft.setTextColor(TFT_WHITE,0x0862);tft.drawString(line,141,135,1);
  if(gps.location.isValid()) {
    snprintf(line,sizeof(line),"LAT %.3f",gps.location.lat());
    tft.setTextColor(TFT_CYAN,0x0862);tft.drawString(line,141,150,1);
    snprintf(line,sizeof(line),"LON %.3f",gps.location.lng());
    tft.drawString(line,141,164,1);
  } else {
    tft.setTextColor(MUTED,0x0862);tft.drawString("LAT --.---",141,150,1);tft.drawString("LON --.---",141,164,1);
  }
  tft.fillRect(9,200,302,7,BG);tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WiFi.status()==WL_CONNECTED?TFT_GREEN:TFT_ORANGE,BG);
  tft.drawString(WiFi.status()==WL_CONNECTED?"WI-FI LINK OK":"WI-FI OFFLINE",10,199,1);
  tft.setTextDatum(TR_DATUM);tft.setTextColor(MUTED,BG);
  if(gps.time.isValid())snprintf(line,sizeof(line),"UTC %02u:%02u:%02u",gps.time.hour(),gps.time.minute(),gps.time.second());
  else snprintf(line,sizeof(line),"UTC --:--:--");
  tft.drawString(line,309,199,1);
}

static void showDashboard() {
  screen=Screen::Dashboard;
  header("DASHBOARD","GPS / SATELLITE TRACKER");
  drawGpsPanel();
  drawNavigation(0);
  lastDashboardRefreshAt=millis();
}

static void satelliteRow(int y,const char* name,const char* type,bool selected) {
  const uint16_t fill=selected?0x123B:PANEL;
  tft.fillRoundRect(9,y,302,30,5,fill);
  tft.drawRoundRect(9,y,302,30,5,selected?TFT_CYAN:0x2A69);
  tft.fillCircle(22,y+15,4,selected?TFT_GREEN:TFT_CYAN);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(selected?TFT_CYAN:TFT_WHITE,fill);
  tft.drawString(name,34,y+2,2);
  tft.setTextColor(MUTED,fill);
  tft.drawString(type,34,y+18,1);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(TFT_YELLOW,fill);
  tft.drawString(selected?"SELECTED":"ORBIT N/A",302,y+15,1);
}

static void showSatelliteScreen() {
  screen=Screen::Satellite;header("SATELLITES","OBJECT CATALOG  /  TRACKING TARGETS");
  tft.fillRoundRect(8,88,304,55,9,0x123B);tft.drawRoundRect(8,88,304,55,9,TFT_CYAN);
  tft.fillRoundRect(13,93,62,45,6,0x0010);
  drawSatelliteArtwork(44,116,1);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_WHITE,0x123B);tft.drawString("ISS  /  ZARYA",84,96,2);
  tft.setTextColor(TFT_CYAN,0x123B);tft.drawString("INTERNATIONAL SPACE STATION",84,119,1);
  tft.fillRoundRect(245,96,57,15,6,0x0320);tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN,0x0320);tft.drawString("PRIORITY",273,104,1);
  satelliteRow(148,"NOAA-18","WEATHER SATELLITE  /  TLE PENDING",false);
  satelliteRow(182,"NOAA-19","WEATHER SATELLITE  /  TLE PENDING",false);
  drawNavigation(1);
}
static void showAntennaScreen() {
  screen=Screen::Antenna;header("ANTENNA","VHF 145 MHz  /  CIRCULAR POLARIZED YAGI");
  tft.fillRoundRect(8,88,147,109,9,0x0C63);tft.drawRoundRect(8,88,147,109,9,0x3151);
  tft.fillRoundRect(163,88,149,109,9,0x0C63);tft.drawRoundRect(163,88,149,109,9,0x3151);
  drawSectionLabel("AZIMUTH",17,96,128);
  // Instrument-style compass face with major/minor ticks and cardinal labels.
  const int cx=81,cy=153,rr=37;
  tft.fillCircle(cx,cy,rr-2,0x0822);
  tft.drawCircle(cx,cy,rr,0x07FF);tft.drawCircle(cx,cy,rr-4,0x2A69);
  for(int a=0;a<360;a+=10) {
    float rad=(a-90)*0.0174532925f;
    int r1=rr-5, r2=rr-(a%30==0?13:9);
    int x1=cx+(int)(r1*cosf(rad)),y1=cy+(int)(r1*sinf(rad));
    int x2=cx+(int)(r2*cosf(rad)),y2=cy+(int)(r2*sinf(rad));
    tft.drawLine(x1,y1,x2,y2,a%30==0?TFT_CYAN:0x2A69);
  }
  tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_WHITE,0x0822);
  tft.drawString("N",cx,cy-25,1);tft.drawString("E",cx+25,cy,1);
  tft.drawString("S",cx,cy+25,1);tft.drawString("W",cx-25,cy,1);
  drawDialNeedle(cx,cy,17,0x0,TFT_ORANGE);
  tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_CYAN,0x0C63);tft.drawString("--.-°",81,181,2);
  drawSectionLabel("ELEVATION",172,96,130);
  // Large altitude arc with degree scale and a neutral, uncommanded pointer.
  tft.drawArc(237,157,43,35,180,360,TFT_CYAN,0x0C63,true);
  tft.drawArc(237,157,35,29,180,360,0x2A69,0x0C63,true);
  for(int a=180;a<=360;a+=15) {
    float rad=a*0.0174532925f;
    int x1=237+(int)(38*cosf(rad)),y1=157+(int)(38*sinf(rad));
    int x2=237+(int)(31*cosf(rad)),y2=157+(int)(31*sinf(rad));
    tft.drawLine(x1,y1,x2,y2,a%45==0?TFT_CYAN:0x2A69);
  }
  tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_WHITE,0x0C63);
  tft.drawString("--.-°",237,151,2);
  tft.setTextColor(MUTED,0x0C63);tft.drawString("0       45       90",237,184,1);
  tft.fillRoundRect(8,181,304,18,6,0x4000);tft.drawRoundRect(8,181,304,18,6,TFT_ORANGE);
  tft.setTextColor(TFT_YELLOW,0x4000);tft.drawString("ORBIT NOT LOADED  /  DRIVE LOCKED",160,190,1);
  drawNavigation(2);
}
static void showSystemScreen() {
  screen=Screen::SystemInfo;header("SYSTEM","DEVICE HEALTH  /  CONFIGURATION");
  const bool wifi=WiFi.status()==WL_CONNECTED;
  const bool fix=gps.location.isValid() && gps.location.age()<10000;
  tft.fillRoundRect(8,88,148,54,8,0x0C63);tft.drawRoundRect(8,88,148,54,8,0x3151);
  tft.fillRoundRect(164,88,148,54,8,0x0C63);tft.drawRoundRect(164,88,148,54,8,0x3151);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(MUTED,0x0C63);
  tft.drawString("WI-FI LINK",17,95,1);tft.drawString("GPS RECEIVER",173,95,1);
  tft.setTextColor(wifi?TFT_GREEN:TFT_ORANGE,0x0C63);tft.drawString(wifi?"CONNECTED":"OFFLINE",17,111,2);
  tft.setTextColor(fix?TFT_GREEN:TFT_YELLOW,0x0C63);tft.drawString(fix?"3D POSITION":"SEARCHING",173,111,2);
  tft.setTextColor(MUTED,0x0C63);tft.drawString(wifi?WiFi.localIP().toString().c_str():"IP  --",17,129,1);
  String net=wifiName.length()?wifiName:"NO SAVED NETWORK";if(net.length()>19)net=net.substring(0,19)+"...";
  tft.drawString(net.c_str(),173,129,1);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_CYAN,BG);tft.drawString("QUICK SETTINGS",10,146,1);
  drawButton(9,159,147,37,"TOUCH CALIBRATE",0x035F);
  drawButton(164,159,147,37,"CHANGE WI-FI",0x03B9);
  drawNavigation(3);
}

static void showConnected() {
  showDashboard();
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
 uint16_t rx=0,ry=0;
 if(!readXpt2046(rx,ry)){touchWasDown=false;return;}
 if(touchWasDown||millis()-lastTouchAt<45)return;
 touchWasDown=true;lastTouchAt=millis();
 // Take a burst at each calibration target. A single XPT2046 sample can be
 // displaced enough to move the whole keyboard by one 24 px numeric key.
 uint16_t sx[7],sy[7]; sx[0]=rx; sy[0]=ry;
 for(uint8_t i=1;i<7;i++){
   if(!readXpt2046(sx[i],sy[i])) { sx[i]=sx[i-1]; sy[i]=sy[i-1]; }
   delay(3);
 }
 for(uint8_t i=0;i<7;i++) for(uint8_t j=i+1;j<7;j++){
   if(sx[j]<sx[i]){uint16_t t=sx[i];sx[i]=sx[j];sx[j]=t;}
   if(sy[j]<sy[i]){uint16_t t=sy[i];sy[i]=sy[j];sy[j]=t;}
 }
 rx=sx[3]; ry=sy[3];
 touchCorners[(touchCalStep-1)*2]=rx;touchCorners[(touchCalStep-1)*2+1]=ry;
 Serial.printf("Touch calibration %u/4 median7=%u,%u\\n",touchCalStep,rx,ry);++touchCalStep;
 if(touchCalStep<=4){drawCalibrationTarget(touchCalStep-1);return;}
 prefs.putBytes("touchraw",touchCorners,sizeof(touchCorners));prefs.putUChar("touchv",5);
 Serial.printf("Touch corners TL=%u,%u TR=%u,%u BL=%u,%u BR=%u,%u\\n",touchCorners[0],touchCorners[1],touchCorners[2],touchCorners[3],touchCorners[4],touchCorners[5],touchCorners[6],touchCorners[7]);
 touchCalibrated=true;
 prefs.putBytes("touchraw",touchCorners,sizeof(touchCorners)); prefs.putUChar("touchv",5);
 if(calibrationFromSystem) { calibrationFromSystem=false; touchWasDown=false; showSystemScreen(); }
 else { screen=Screen::Init; showInitialization(); screenStartedAt=millis(); }
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
 if(digitalRead(TOUCH_IRQ_PIN)!=LOW){touchWasDown=false;return;}
 if(touchWasDown||millis()-lastTouchAt<35)return;
 if(!readStableTouch(rx,ry,x,y))return;
 touchWasDown=true;lastTouchAt=millis();
 rawTouchX=rx; rawTouchY=ry;
 Serial.printf("Touch stable raw=%u,%u mapped=%u,%u rot=%u screen=%u\\n",rx,ry,x,y,tft.getRotation(),(unsigned)screen);
 if(screen==Screen::WifiSetup)handleKeyboardTap(x,y);
 else if(screen==Screen::SystemInfo && y>=145 && y<=199) {
   if(x<160) {
     calibrationFromSystem=true; touchCalibrated=false; touchCalStep=1;
     screen=Screen::TouchTest; touchWasDown=false; drawCalibrationTarget(0);
   } else {
     WiFi.disconnect(false,false); editingPassword=false; setupMessage="Edit network, then press CONNECT";
     wifiScreenDrawn=false; showWifiSetup();
   }
 }
 else if((screen==Screen::Dashboard||screen==Screen::Satellite||screen==Screen::Antenna||screen==Screen::SystemInfo)&&y>=204) {
   if(x<81) showDashboard();
   else if(x<160) showSatelliteScreen();
   else if(x<239) showAntennaScreen();
   else showSystemScreen();
 }
 else if(screen==Screen::Connected&&y>=200){WiFi.disconnect();setupMessage="";editingPassword=false;showWifiSetup();}
 else if(screen==Screen::Failed&&y>=190){setupMessage="Check password, then connect again";showWifiSetup();}
}
void setup() {
  Serial.begin(115200);
  delay(150);
  pinMode(Pins::TFT_BACKLIGHT,OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT,HIGH); // Constant backlight; no PWM/fade in firmware.
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
    prefs.putString("ssid",wifiName); prefs.putString("password",wifiPassword);
  }
  loadTouchCalibration();
  showSplash();
  screen=Screen::Splash;
  screenStartedAt=millis();
}

void loop() {
  while(Serial2.available()) {
    if(gps.encode(Serial2.read()) && gps.location.isValid()) gpsSeen=true;
  }
  const uint32_t now=millis();
  if(screen==Screen::Dashboard && now-lastDashboardRefreshAt>=1000) {
    updateGpsTelemetry();
    lastDashboardRefreshAt=now;
  }
  if(screen==Screen::Splash && now-screenStartedAt>=SPLASH_MS) {
    if(touchCalibrated) { screen=Screen::Init; showInitialization(); screenStartedAt=now; }
    else showTouchDiagnostic();
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
  }
  handleTouch();
}
