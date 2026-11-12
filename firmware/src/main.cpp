#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <ctype.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <math.h>
#include <Sgp4.h>
#include <TJpg_Decoder.h>
#include "splash_jpg.h"
#include "ui_assets.h"
#include "BoardConfig.h"
#include "WifiConfig.h"

// Trust anchors for CelesTrak HTTPS: USERTrust RSA Certification Authority
// and Sectigo Public Server Authentication Root R46, both from Mozilla's CA store.
// setCACert() validates the server certificate chain and hostname; no bypass.
static const char CELESTRAK_CA_BUNDLE[] = R"PEM(-----BEGIN CERTIFICATE-----
MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw
MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV
BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU
aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy
dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK
AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B
3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY
tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/
Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2
VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT
79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6
c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT
Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l
c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee
UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE
Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd
BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G
A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF
Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO
VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3
ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs
8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR
iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze
Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ
XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/
qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB
VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB
L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG
jjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIFijCCA3KgAwIBAgIQdY39i658BwD6qSWn4cetFDANBgkqhkiG9w0BAQwFADBf
MQswCQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQD
Ey1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYw
HhcNMjEwMzIyMDAwMDAwWhcNNDYwMzIxMjM1OTU5WjBfMQswCQYDVQQGEwJHQjEY
MBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1Ymxp
YyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYwggIiMA0GCSqGSIb3DQEB
AQUAA4ICDwAwggIKAoICAQCTvtU2UnXYASOgHEdCSe5jtrch/cSV1UgrJnwUUxDa
ef0rty2k1Cz66jLdScK5vQ9IPXtamFSvnl0xdE8H/FAh3aTPaE8bEmNtJZlMKpnz
SDBh+oF8HqcIStw+KxwfGExxqjWMrfhu6DtK2eWUAtaJhBOqbchPM8xQljeSM9xf
iOefVNlI8JhD1mb9nxc4Q8UBUQvX4yMPFF1bFOdLvt30yNoDN9HWOaEhUTCDsG3X
ME6WW5HwcCSrv0WBZEMNvSE6Lzzpng3LILVCJ8zab5vuZDCQOc2TZYEhMbUjUDM3
IuM47fgxMMxF/mL50V0yeUKH32rMVhlATc6qu/m1dkmU8Sf4kaWD5QazYw6A3OAS
VYCmO2a0OYctyPDQ0RTp5A1NDvZdV3LFOxxHVp3i1fuBYYzMTYCQNFu31xR13NgE
SJ/AwSiItOkcyqex8Va3e0lMWeUgFaiEAin6OJRpmkkGj80feRQXEgyDet4fsZfu
+Zd4KKTIRJLpfSYFplhym3kT2BFfrsU4YjRosoYwjviQYZ4ybPUHNs2iTG7sijbt
8uaZFURww3y8nDnAtOFr94MlI1fZEoDlSfB1D++N6xybVCi0ITz8fAr/73trdf+L
HaAZBav6+CuBQug4urv7qv094PPK306Xlynt8xhW6aWWrL3DkJiy4Pmi1KZHQ3xt
zwIDAQABo0IwQDAdBgNVHQ4EFgQUVnNYZJX5khqwEioEYnmhQBWIIUkwDgYDVR0P
AQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAC9c
mTz8Bl6MlC5w6tIyMY208FHVvArzZJ8HXtXBc2hkeqK5Duj5XYUtqDdFqij0lgVQ
YKlJfp/imTYpE0RHap1VIDzYm/EDMrraQKFz6oOht0SmDpkBm+S8f74TlH7Kph52
gDY9hAaLMyZlbcp+nv4fjFg4exqDsQ+8FxG75gbMY/qB8oFM2gsQa6H61SilzwZA
Fv97fRheORKkU55+MkIQpiGRqRxOF3yEvJ+M0ejf5lG5Nkc/kLnHvALcWxxPDkjB
JYOcCj+esQMzEhonrPcibCTRAUH4WAP+JWgiH5paPHxsnnVI84HxZmduTILA7rpX
DhjvLpr3Etiga+kFpaHpaPi8TD8SHkXoUsCjvxInebnMMTzD9joiFgOgyY9mpFui
TdaBJQbpdqQACj7LzTWb4OE4y2BThihCQRxEV+ioratF4yUQvNs+ZUH7G6aXD+u5
dHn5HrwdVw1Hr8Mvn4dGp+smWg9WY7ViYG4A++MnESLn/pmPNPW56MORcr3Ywx65
LvKRRFHQV80MNNVIIb/bE/FmJUNS0nAiNs2fxBx1IK1jcmMGDw4nztJqDby1ORrp
0XZ60Vzk50lJLVU3aPAaOpg+VBeHVOmmJ1CJeyAvP/+/oYtKR5j/K3tJPsMpRmAY
QqszKbrAKbkTidOIijlBO8n9pu0f9GBj39ItVQGL
-----END CERTIFICATE-----)PEM";

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
static bool displayDiagnosticActive = false;
static bool displayInvertTestState = true;
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
static String deviceHardwareId;
static WebServer localServer(80);
static bool localApiStarted=false;
// Local browser control state. Targets are stored only; motors remain disabled.
static float targetAzimuthDeg=0.0f;
static float targetElevationDeg=0.0f;
static bool trackingRequested=false;
static uint32_t localCommandSequence=0;
static constexpr double DEMO_OBSERVER_LAT=56.8389;
static constexpr double DEMO_OBSERVER_LON=60.6057;
static constexpr double DEMO_OBSERVER_ALT_M=250.0;
static Sgp4 issSgp4;
static bool ntpStarted=false,issTleValid=false,issPositionValid=false,observerUsesGps=false;
static uint32_t lastTleAttemptMs=0,lastTleSuccessMs=0,lastOrbitCalcMs=0;
static time_t orbitUtcEpoch=0;
static double observerLatitude=DEMO_OBSERVER_LAT,observerLongitude=DEMO_OBSERVER_LON,observerAltitudeM=DEMO_OBSERVER_ALT_M;
static double issAzimuthDeg=0,issElevationDeg=0,issRangeKm=0;
static uint32_t selectedNoradId=25544;
static String selectedSatelliteName="ISS (ZARYA)";
static String selectedSatelliteImageUrl="https://db-satnogs.freetls.fastly.net/media/satellites/ISS.jpg";
static uint8_t* selectedSatelliteImageData=nullptr;
static uint32_t selectedSatelliteImageSize=0;
static bool selectedSatelliteImagePending=true;
static String orbitDataMessage="Ожидание подключения Wi-Fi";

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
  Serial.printf("CYD V74 | %s | rotation=%u | logical=%dx%d\n",
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
  // LCD inversion is configured globally in setup(). Do not invert JPEG
  // pixels a second time here; doing so makes the splash artwork negative.
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
      const int col=(x-16)/24;
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


static void showSystemScreen();

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
static void drawSelectedSatellitePhoto() {
  // Replace the fixed ISS artwork with the selected object's public photo.
  tft.fillRoundRect(8,34,175,80,6,0x0822);
  if(selectedSatelliteImageData && selectedSatelliteImageSize>4) {
    tft.setViewport(8,34,175,80,true);
    TJpgDec.setJpgScale(2);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(uiImageOutput);
    TJpgDec.drawJpg(0,0,selectedSatelliteImageData,selectedSatelliteImageSize);
    tft.resetViewport();
  } else {
    drawSatelliteArtwork(96,74,2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW,0x0822);
    tft.drawString("PHOTO NOT FOUND",96,105,1);
  }
  tft.fillRect(8,114,175,27,0x0010);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE,0x0010);
  String label=selectedSatelliteName;
  if(label.length()>25) label=label.substring(0,25);
  tft.drawString(label,14,116,1);
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
  // V81: compact four-zone instrument dashboard, designed for native 320x240.
  // Photo and static chrome are drawn once; telemetry values are refreshed separately.
  const uint16_t edge=0x03BF, panel=0x0822, panelHi=0x1045;
  // Selected spacecraft photo card; never label a different satellite as ISS.
  tft.fillRoundRect(5,31,181,113,9,0x0000);
  tft.drawRoundRect(5,31,181,113,9,edge);
  drawSelectedSatellitePhoto();
  // Radar: luminous rings, graduated ticks, cardinal marks and target marker.
  const int cx=250,cy=87,r=49;
  tft.fillCircle(cx,cy,r+8,0x0000);
  tft.drawCircle(cx,cy,r+7,0x03BF);
  tft.drawCircle(cx,cy,r,0x2A69);
  tft.drawCircle(cx,cy,32,0x1A45);
  tft.drawCircle(cx,cy,15,0x1A45);
  tft.drawFastHLine(cx-r,cy,2*r,0x2A69);
  tft.drawFastVLine(cx,cy-r,2*r,0x2A69);
  for(int a=0;a<360;a+=5) {
    const float q=(a-90)*0.0174532925f;
    const int r1=r-2, r2=r-(a%30==0?10:(a%10==0?7:4));
    const uint16_t c=a%30==0?TFT_CYAN:0x2A69;
    tft.drawLine(cx+(int)(r1*cosf(q)),cy+(int)(r1*sinf(q)),
                 cx+(int)(r2*cosf(q)),cy+(int)(r2*sinf(q)),c);
  }
  tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_WHITE,BG);
  tft.drawString("N",cx,cy-r-2,2); tft.drawString("E",cx+r+2,cy,2);
  tft.drawString("S",cx,cy+r+2,2); tft.drawString("W",cx-r-2,cy,2);
  tft.fillTriangle(cx,cy-13,cx-6,cy+7,cx+6,cy+7,TFT_CYAN);
  tft.fillCircle(cx,cy,3,TFT_WHITE);
  tft.fillCircle(cx-25,cy+23,4,TFT_RED);
  tft.drawCircle(cx-25,cy+23,6,0xF986);
  // Three aligned data cards with consistent inset, edge and typography.
  const int xs[3]={5,111,217}, ws[3]={101,101,98};
  for(int i=0;i<3;i++) {
    tft.fillRoundRect(xs[i],149,ws[i],50,7,0x0000);
    tft.fillRoundRect(xs[i]+2,151,ws[i]-4,46,5,panelHi);
    tft.drawRoundRect(xs[i],149,ws[i],50,7,i==0?edge:0x2A69);
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN,panelHi); tft.drawString("GPS",11,155,1);
  tft.setTextColor(TFT_CYAN,panelHi); tft.drawString("SATELLITES",117,155,1);
  tft.setTextColor(TFT_CYAN,panelHi); tft.drawString("LINK",223,155,1);
  updateGpsTelemetry();
}
static void updateGpsTelemetry() {
  const bool fix=gps.location.isValid() && gps.location.age()<10000;
  const bool wifi=WiFi.status()==WL_CONNECTED;
  char line[32];
  // GPS fix and altitude.
  tft.fillRect(10,169,91,25,0x1045);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fix?TFT_GREEN:TFT_YELLOW,0x1045);
  if(fix) snprintf(line,sizeof(line),"%.2f %.2f",gps.location.lat(),gps.location.lng());
  else snprintf(line,sizeof(line),"SEARCHING");
  tft.drawString(line,11,169,1);
  if(fix) snprintf(line,sizeof(line),"ALT %.0fm",gps.altitude.meters());
  else snprintf(line,sizeof(line),"ALT ---");
  tft.setTextColor(TFT_WHITE,0x1045); tft.drawString(line,11,183,1);
  // This is the GNSS receiver's satellite count, not the orbital catalog.
  tft.fillRect(116,169,91,25,0x1045);
  tft.setTextDatum(MC_DATUM);
  snprintf(line,sizeof(line),"%02lu",(unsigned long)(gps.satellites.isValid()?gps.satellites.value():0));
  tft.setTextColor(TFT_WHITE,0x1045); tft.drawString(line,161,182,4);
  // Wi-Fi state and UTC clock.
  tft.fillRect(222,169,90,25,0x1045);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(wifi?TFT_GREEN:TFT_ORANGE,0x1045);
  tft.drawString(wifi?"CONNECTED":"OFFLINE",266,174,1);
  tft.setTextColor(TFT_WHITE,0x1045);
  if(gps.time.isValid()) {
    snprintf(line,sizeof(line),"%02u:%02u:%02u",gps.time.hour(),gps.time.minute(),gps.time.second());
    tft.drawString(line,266,188,1);
  } else tft.drawString("UTC --:--",266,188,1);
}
static void updateTargetTelemetry() {
  char line[40];
  if(screen==Screen::Dashboard) {
    // Refresh the target readout without repainting the selected spacecraft photo.
    tft.fillRect(8,128,175,13,0x0010);
    snprintf(line,sizeof(line),"AZ %.1f%c  EL %.1f%c",targetAzimuthDeg,(char)247,targetElevationDeg,(char)247);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN,0x0010);
    tft.drawString(line,14,129,1);
  } else if(screen==Screen::Antenna) {
    // Live azimuth: update the numeric heading and rotate the compass needle
    // without repainting the compass rose, labels, panels, or whole screen.
    tft.fillRect(15,93,132,16,0x0822);
    snprintf(line,sizeof(line),"AZ %.1f%c",targetAzimuthDeg,(char)247);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN,0x0822);
    tft.drawString(line,17,95,2);
    const int cx=81, cy=150;
    tft.fillCircle(cx,cy,22,0x0822);
    drawDialNeedle(cx,cy,19,targetAzimuthDeg,TFT_ORANGE);

    // Keep the motor-lock warning visible alongside the latest target values.
    tft.fillRect(10,186,300,9,0x4000);
    snprintf(line,sizeof(line),"LOCKED  AZ %.1f%c  EL %.1f%c",targetAzimuthDeg,(char)247,targetElevationDeg,(char)247);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW,0x4000);
    tft.drawString(line,160,190,1);
    tft.fillRect(184,139,106,25,0x0822);
    snprintf(line,sizeof(line),"%.1f%c",targetElevationDeg,(char)247);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,0x0822);
    tft.drawString(line,237,151,4);
  }
}
static void showDashboard() {
  screen=Screen::Dashboard;
  tft.fillScreen(BG);
  // Premium Space Dark home: real ISS photo, luminous radar and live receiver telemetry.
  const bool wifi=WiFi.status()==WL_CONNECTED;
  const uint16_t top=0x0010;
  tft.fillRect(0,0,320,27,top);
  tft.fillRect(0,26,320,2,0x03BF);
  tft.drawCircle(15,13,7,TFT_CYAN); tft.fillCircle(15,13,3,TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE,top); tft.drawString("CYD",29,4,2);
  tft.setTextColor(0x8E7F,top); tft.drawString("SATELLITE TRACKER",65,8,1);
  // Compact Wi-Fi glyph replaces the text badge; color reflects link state.
  const uint16_t wifiColor=wifi?TFT_GREEN:TFT_ORANGE;
  tft.drawLine(250,9,254,6,wifiColor); tft.drawLine(254,6,260,4,wifiColor);
  tft.drawLine(260,4,266,6,wifiColor); tft.drawLine(266,6,270,9,wifiColor);
  tft.drawLine(254,13,257,10,wifiColor); tft.drawLine(257,10,260,9,wifiColor);
  tft.drawLine(260,9,263,10,wifiColor); tft.drawLine(263,10,266,13,wifiColor);
  tft.fillCircle(260,17,2,wifiColor);
  drawGpsPanel();
  drawNavigation(0);
  updateTargetTelemetry();
  lastDashboardRefreshAt=millis();
}

static void satelliteRow(int y,const char* name,const char* type,bool selected) {
  const uint16_t fill=selected?0x123B:0x0822;
  tft.fillRoundRect(9,y,302,19,5,fill);
  tft.drawRoundRect(9,y,302,19,5,selected?TFT_CYAN:0x2A69);
  tft.fillCircle(19,y+9,3,selected?TFT_GREEN:TFT_CYAN);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_WHITE,fill);tft.drawString(name,29,y+2,1);
  tft.setTextColor(MUTED,fill);tft.drawString(type,100,y+3,1);
  tft.setTextDatum(MR_DATUM);tft.setTextColor(TFT_YELLOW,fill);
  tft.drawString(selected?"TRACKING":"TLE PENDING",302,y+9,1);
}

static void showSatelliteScreen() {
  screen=Screen::Satellite;
  header("SATELLITES","OBJECT CATALOG  /  TRACKING TARGETS");
  // Featured ISS card with the embedded spacecraft photograph.
  tft.fillRoundRect(8,88,304,65,9,0x0822);
  tft.drawRoundRect(8,88,304,65,9,0x07FF);
  tft.fillRoundRect(13,93,76,55,6,0x0010);
  drawUiImage(19,96,ui_iss_jpg,sizeof(ui_iss_jpg));
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_WHITE,0x0822);
  tft.drawString("INTERNATIONAL",99,96,1);
  tft.drawString("SPACE STATION",99,108,2);
  tft.setTextColor(TFT_CYAN,0x0822);tft.drawString("ISS  /  ZARYA",99,132,1);
  tft.fillRoundRect(242,96,60,17,7,0x0320);
  tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_GREEN,0x0320);
  tft.drawString("PRIMARY",272,104,1);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(MUTED,BG);
  tft.drawString("OTHER OBJECTS",11,157,1);
  satelliteRow(169,"NOAA-18","WEATHER  /  APT",false);
  satelliteRow(190,"NOAA-19","WEATHER  /  APT",false);
  drawNavigation(1);
}
static void showAntennaScreen() {
  screen=Screen::Antenna;
  header("ROTATOR","VHF 145 MHz  /  CIRCULAR YAGI");
  // Instrument console with a compass rose and elevation arc.
  tft.fillRoundRect(8,88,147,109,9,0x0822);tft.drawRoundRect(8,88,147,109,9,0x2A69);
  tft.fillRoundRect(163,88,149,109,9,0x0822);tft.drawRoundRect(163,88,149,109,9,0x2A69);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_CYAN,0x0822);tft.drawString("AZIMUTH",17,95,1);
  const int cx=81,cy=150,rr=38;
  tft.drawCircle(cx,cy,rr,0x07FF);tft.drawCircle(cx,cy,rr-3,0x2A69);
  for(int a=0;a<360;a+=5) {
    float rad=(a-90)*0.0174532925f;
    int r1=rr-3,r2=rr-(a%30==0?12:(a%10==0?8:5));
    tft.drawLine(cx+(int)(r1*cosf(rad)),cy+(int)(r1*sinf(rad)),
                 cx+(int)(r2*cosf(rad)),cy+(int)(r2*sinf(rad)),
                 a%30==0?TFT_CYAN:0x2A69);
  }
  tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_WHITE,0x0822);
  tft.drawString("N",cx,cy-26,1);tft.drawString("E",cx+26,cy,1);
  tft.drawString("S",cx,cy+26,1);tft.drawString("W",cx-26,cy,1);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_CYAN,0x0822);tft.drawString("ELEVATION",173,95,1);
  tft.setTextDatum(MC_DATUM);tft.setTextColor(MUTED,0x0822);tft.drawString("0°",180,181,1);
  tft.drawString("45°",237,181,1);tft.drawString("90°",294,181,1);
  tft.drawArc(237,151,48,39,180,360,0x2A69,0x0822,true);
  tft.drawArc(237,151,43,35,180,360,TFT_CYAN,0x0822,true);
  for(int a=180;a<=360;a+=15) {
    float rad=a*0.0174532925f;
    tft.drawLine(237+(int)(44*cosf(rad)),151+(int)(44*sinf(rad)),
                 237+(int)(37*cosf(rad)),151+(int)(37*sinf(rad)),
                 a%45==0?TFT_WHITE:0x2A69);
  }
  tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_WHITE,0x0822);
  tft.drawString("--.-°",237,151,4);
  tft.fillRoundRect(8,184,304,13,5,0x4000);tft.drawRoundRect(8,184,304,13,5,TFT_ORANGE);
  tft.setTextColor(TFT_YELLOW,0x4000);tft.drawString("DRIVE LOCKED  /  TARGET PENDING",160,190,1);
  drawNavigation(2);
  updateTargetTelemetry();
}
static void showSystemScreen() {
  screen=Screen::SystemInfo;
  header("SYSTEM","DEVICE HEALTH  /  CONFIGURATION");
  const bool wifi=WiFi.status()==WL_CONNECTED;
  const bool fix=gps.location.isValid() && gps.location.age()<10000;
  // Two status panels with clear state hierarchy.
  tft.fillRoundRect(8,89,148,65,8,0x0822);tft.drawRoundRect(8,89,148,65,8,0x2A69);
  tft.fillRoundRect(164,89,148,65,8,0x0822);tft.drawRoundRect(164,89,148,65,8,0x2A69);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(MUTED,0x0822);
  tft.drawString("WI-FI CONNECTION",17,97,1);tft.drawString("GPS RECEIVER",173,97,1);
  tft.setTextColor(wifi?TFT_GREEN:TFT_ORANGE,0x0822);tft.drawString(wifi?"ONLINE":"OFFLINE",17,114,4);
  tft.setTextColor(fix?TFT_GREEN:TFT_YELLOW,0x0822);tft.drawString(fix?"FIX":"SEARCH",173,114,4);
  tft.setTextColor(MUTED,0x0822);tft.drawString(wifi?WiFi.localIP().toString().c_str():"NO NETWORK",17,140,1);
  tft.drawString(gps.satellites.isValid()?"SATELLITES RECEIVED":"WAITING FOR DATA",173,140,1);
  tft.setTextColor(TFT_CYAN,0x0822);
  tft.drawString("LOCAL API READY",173,151,1);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_CYAN,BG);tft.drawString("QUICK ACTIONS",10,160,1);
  drawButton(7,174,98,26,"TOUCH",0x035F);
  drawButton(111,174,98,26,"WI-FI",0x03B9);
  drawButton(215,174,98,26,"API",0x03B9);
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
 if(displayDiagnosticActive) return;
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
 else if(screen==Screen::SystemInfo && y>=170 && y<=203) {
   if(x<108) {
     calibrationFromSystem=true; touchCalibrated=false; touchCalStep=1;
     screen=Screen::TouchTest; touchWasDown=false; drawCalibrationTarget(0);
   } else if(x<212) {
     WiFi.disconnect(false,false); editingPassword=false; setupMessage="Edit network, then press CONNECT";
     wifiScreenDrawn=false; showWifiSetup();
   } else { Serial.printf("[LOCAL] API address: http://%s/\\n",WiFi.localIP().toString().c_str()); showSystemScreen(); }
 }
 else if(screen==Screen::Dashboard && y>=160 && y<200) {
   showSatelliteScreen();
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
// Serial-only display diagnostic. Send 'c' in the serial monitor to show
// exact RGB565 primary/secondary colors; send 'b' to return to the current UI.
// This does not alter normal boot or disturb touch calibration.
static void drawDisplayColorTest() {
  displayDiagnosticActive = true;
  tft.fillScreen(TFT_BLACK);
  const uint16_t colors[6] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x07FF, 0xF81F};
  const char* labels[6] = {"RED  F800", "GREEN 07E0", "BLUE 001F",
                           "WHITE FFFF", "CYAN 07FF", "MAGENTA F81F"};
  for (int i=0; i<6; ++i) {
    const int y=8+i*37;
    tft.fillRect(8,y,304,30,colors[i]);
    tft.drawRect(8,y,304,30,TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor((i==3||i==4)?TFT_BLACK:TFT_WHITE,colors[i]);
    tft.drawString(labels[i],160,y+15,2);
  }
  Serial.printf("DISPLAY COLOR TEST | rotation=%u | size=%dx%d | RGB565: F800/07E0/001F/FFFF/07FF/F81F\n",
                tft.getRotation(),tft.width(),tft.height());
}
static void restoreCurrentScreen() {
  displayDiagnosticActive = false;
  switch(screen) {
    case Screen::Dashboard: showDashboard(); break;
    case Screen::Satellite: showSatelliteScreen(); break;
    case Screen::Antenna: showAntennaScreen(); break;
    case Screen::SystemInfo: showSystemScreen(); break;
    default: showDashboard(); break;
  }
}
static void handleSerialDisplayDiagnostics() {
  while(Serial.available()) {
    const char command=(char)Serial.read();
    if(command=='c' || command=='C') drawDisplayColorTest();
    else if(command=='i' || command=='I') {
      displayInvertTestState = !displayInvertTestState;
      tft.invertDisplay(displayInvertTestState);
      drawDisplayColorTest();
      Serial.printf("DISPLAY INVERSION TEST: %s\\n",displayInvertTestState?"ON":"OFF");
    }
    else if(command=='b' || command=='B') restoreCurrentScreen();
  }
}

static void updateOrbitTelemetry() {
  if(screen!=Screen::Satellite) return;
  tft.fillRect(10,88,300,105,BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN,BG);
  String label=selectedSatelliteName;
  if(label.length()>25) label=label.substring(0,25);
  tft.drawString(label,14,91,2);
  char norad[32]; snprintf(norad,sizeof(norad),"NORAD %lu",(unsigned long)selectedNoradId);
  tft.setTextColor(MUTED,BG); tft.drawString(norad,14,111,1);
  tft.setTextColor(issTleValid?TFT_GREEN:TFT_YELLOW,BG);
  tft.drawString(issTleValid?"TLE OK":"TLE WAIT",220,111,1);
  char line[40];
  tft.setTextColor(TFT_WHITE,BG);
  snprintf(line,sizeof(line),"AZ  %6.1f deg",issAzimuthDeg); tft.drawString(line,14,132,2);
  snprintf(line,sizeof(line),"EL  %6.1f deg",issElevationDeg); tft.drawString(line,14,153,2);
  snprintf(line,sizeof(line),"RANGE %7.1f km",issRangeKm); tft.drawString(line,164,153,1);
  tft.setTextColor(MUTED,BG);
  tft.drawString(observerUsesGps?"GPS FIX":"DEMO: YEKATERINBURG",14,178,1);
}

static void startOrbitTimeSync() {
  if(ntpStarted || WiFi.status()!=WL_CONNECTED) return;
  configTime(0,0,"pool.ntp.org","time.google.com","time.cloudflare.com");
  ntpStarted=true;
  orbitDataMessage="Синхронизация времени UTC через NTP";
  Serial.println("[ORBIT] NTP synchronization started");
}
static bool fetchSelectedTle() {
  if(WiFi.status()!=WL_CONNECTED) return false;
  WiFiClientSecure client;
  // Verify the CelesTrak certificate chain against trusted Sectigo RSA roots.
  // The NTP gate in updateSelectedOrbit() ensures the clock is valid first.
  client.setCACert(CELESTRAK_CA_BUNDLE);
  HTTPClient http;
  http.setTimeout(12000);
  const String url="https://celestrak.org/NORAD/elements/gp.php?CATNR="+String(selectedNoradId)+"&FORMAT=TLE";
  if(!http.begin(client,url.c_str())) { orbitDataMessage="Не удалось открыть HTTPS CelesTrak"; return false; }
  const int code=http.GET();
  if(code!=HTTP_CODE_OK) {
    orbitDataMessage="CelesTrak HTTP "+String(code);
    Serial.printf("[ORBIT] TLE HTTP error: %d\\n",code);
    http.end();
    return false;
  }
  String payload=http.getString();
  http.end();
  String rows[3];
  uint8_t count=0;
  int start=0;
  while(start<(int)payload.length() && count<3) {
    int end=payload.indexOf('\n',start);
    if(end<0) end=payload.length();
    String row=payload.substring(start,end);
    row.trim();
    if(row.length()) rows[count++]=row;
    start=end+1;
  }
  String name=selectedSatelliteName,line1,line2;
  if(count>=3 && rows[1].startsWith("1 ") && rows[2].startsWith("2 ")) {
    name=rows[0]; line1=rows[1]; line2=rows[2];
  } else if(count>=2 && rows[0].startsWith("1 ") && rows[1].startsWith("2 ")) {
    line1=rows[0]; line2=rows[1];
  } else {
    orbitDataMessage="CelesTrak вернул некорректный TLE";
    Serial.println("[ORBIT] Invalid TLE response");
    return false;
  }
  char tleName[24]={0},tle1[130]={0},tle2[130]={0};
  name.toCharArray(tleName,sizeof(tleName));
  line1.toCharArray(tle1,sizeof(tle1));
  line2.toCharArray(tle2,sizeof(tle2));
  const bool initialized=issSgp4.init(tleName,tle1,tle2);
  if(!initialized && !issTleValid) {
    orbitDataMessage="SGP4 отклонил TLE выбранного спутника";
    Serial.println("[ORBIT] SGP4 rejected selected satellite TLE");
    return false;
  }
  issTleValid=true;
  selectedSatelliteName=name;
  lastTleSuccessMs=millis();
  orbitDataMessage="TLE загружены: "+selectedSatelliteName;
  Serial.printf("[ORBIT] TLE accepted NORAD %lu: %s\\n",(unsigned long)selectedNoradId,tleName);
  return true;
}
static bool isAllowedSatelliteImageUrl(const String& url) {
  return url.startsWith("https://db.satnogs.org/media/") ||
         url.startsWith("https://db-satnogs.freetls.fastly.net/media/");
}
static bool fetchSelectedSatelliteImage() {
  if(!isAllowedSatelliteImageUrl(selectedSatelliteImageUrl) || WiFi.status()!=WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setCACert(CELESTRAK_CA_BUNDLE);
  HTTPClient http;
  http.setTimeout(9000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if(!http.begin(client,selectedSatelliteImageUrl)) return false;
  const int code=http.GET();
  const int length=http.getSize();
  if(code!=HTTP_CODE_OK || length<4 || length>120000) {
    http.end();
    Serial.printf("[PHOTO] download rejected code=%d size=%d\\n",code,length);
    return false;
  }
  uint8_t* buffer=(uint8_t*)malloc((size_t)length);
  if(!buffer) { http.end(); return false; }
  WiFiClient* stream=http.getStreamPtr();
  const size_t received=stream->readBytes((char*)buffer,(size_t)length);
  http.end();
  if(received!=(size_t)length || buffer[0]!=0xFF || buffer[1]!=0xD8) {
    free(buffer);
    Serial.printf("[PHOTO] invalid JPEG bytes=%u/%d\\n",(unsigned)received,length);
    return false;
  }
  if(selectedSatelliteImageData) free(selectedSatelliteImageData);
  selectedSatelliteImageData=buffer;
  selectedSatelliteImageSize=(uint32_t)length;
  Serial.printf("[PHOTO] loaded %u bytes for NORAD %lu\\n",(unsigned)length,(unsigned long)selectedNoradId);
  return true;
}
static void updateSelectedOrbit() {
  if(WiFi.status()!=WL_CONNECTED) return;
  startOrbitTimeSync();
  const time_t now=time(nullptr);
  if(now<1700000000) {
    orbitDataMessage="Ожидание точного времени NTP";
    return;
  }
  if(!issTleValid && (lastTleAttemptMs==0 || millis()-lastTleAttemptMs>=60000)) {
    lastTleAttemptMs=millis();
    fetchSelectedTle();
  } else if(issTleValid && millis()-lastTleSuccessMs>=6UL*60UL*60UL*1000UL) {
    lastTleAttemptMs=millis();
    fetchSelectedTle();
  }
  if(!issTleValid || millis()-lastOrbitCalcMs<1000) return;
  lastOrbitCalcMs=millis();
  observerUsesGps=gps.location.isValid() && gps.location.age()<10000;
  if(observerUsesGps) {
    observerLatitude=gps.location.lat();
    observerLongitude=gps.location.lng();
    observerAltitudeM=gps.altitude.isValid()?gps.altitude.meters():0.0;
  } else {
    observerLatitude=DEMO_OBSERVER_LAT;
    observerLongitude=DEMO_OBSERVER_LON;
    observerAltitudeM=DEMO_OBSERVER_ALT_M;
  }
  issSgp4.site(observerLatitude,observerLongitude,observerAltitudeM);
  issSgp4.findsat((unsigned long)now);
  orbitUtcEpoch=now;
  if(isfinite(issSgp4.satAz) && isfinite(issSgp4.satEl) && isfinite(issSgp4.satDist) && issSgp4.satDist>0) {
    issAzimuthDeg=issSgp4.satAz;
    issElevationDeg=issSgp4.satEl;
    issRangeKm=issSgp4.satDist;
    issPositionValid=true;
    // Feed the live SGP4 solution into the same target values rendered by
    // the dashboard and rotator screens. These are display/setpoint values;
    // motor outputs remain physically and logically disabled.
    targetAzimuthDeg=(float)issAzimuthDeg;
    targetElevationDeg=(float)issElevationDeg;
    orbitDataMessage="SGP4: рассчитано положение "+selectedSatelliteName;
    if(screen==Screen::Dashboard || screen==Screen::Antenna) updateTargetTelemetry();
  } else {
    issPositionValid=false;
    orbitDataMessage="SGP4 не вернул координаты выбранного спутника";
  }
  if(screen==Screen::Satellite) updateOrbitTelemetry();
}

static void sendLocalCorsHeaders() {
  localServer.sendHeader("Access-Control-Allow-Origin","*");
  localServer.sendHeader("Access-Control-Allow-Methods","GET, POST, OPTIONS");
  localServer.sendHeader("Access-Control-Allow-Headers","Content-Type");
  localServer.sendHeader("Access-Control-Allow-Private-Network","true");
  localServer.sendHeader("Vary","Origin");
}

static void sendLocalStatus() {
  JsonDocument doc;
  doc["ok"]=true;
  doc["device"]="CYD Satellite Tracker";
  doc["firmware"]="V87";
  doc["hardware_id"]=deviceHardwareId;
  JsonObject hardware=doc["hardware"].to<JsonObject>();
  hardware["gps_connected"]=gps.charsProcessed()>0;
  hardware["angle_sensor_connected"]=false;
  hardware["compass_connected"]=false;
  hardware["motor_drivers_connected"]=false;
  hardware["limit_switches_connected"]=false;
  doc["connection"]="local-wifi";
  doc["discovery"]="mdns";
  doc["ip"]=WiFi.localIP().toString();
  doc["hostname"]="cyd-tracker.local";
  doc["wifi_rssi"]=WiFi.RSSI();
  doc["gps_valid"]=gps.location.isValid() && gps.location.age()<10000;
  if(doc["gps_valid"].as<bool>()) {
    doc["latitude"]=gps.location.lat();
    doc["longitude"]=gps.location.lng();
  }
  if(gps.altitude.isValid()) doc["altitude_m"]=gps.altitude.meters();
  if(gps.satellites.isValid()) doc["gps_satellites"]=gps.satellites.value();
  doc["time_valid"]=time(nullptr)>1700000000;
  doc["utc_epoch"]=(long long)time(nullptr);
  JsonObject orbit=doc["orbit"].to<JsonObject>();
  orbit["satellite"]=selectedSatelliteName;
  orbit["norad_id"]=selectedNoradId;
  orbit["tle_valid"]=issTleValid;
  orbit["position_valid"]=issPositionValid;
  orbit["observer_source"]=observerUsesGps?"gps":"demo_yekaterinburg";
  orbit["observer_latitude"]=observerLatitude;
  orbit["observer_longitude"]=observerLongitude;
  orbit["utc_epoch"]=(long long)orbitUtcEpoch;
  if(issPositionValid) {
    orbit["azimuth_deg"]=issAzimuthDeg;
    orbit["elevation_deg"]=issElevationDeg;
    orbit["range_km"]=issRangeKm;
  }
  orbit["message"]=orbitDataMessage;
  doc["control"]["target_azimuth_deg"]=targetAzimuthDeg;
  doc["control"]["target_elevation_deg"]=targetElevationDeg;
  doc["control"]["tracking_requested"]=trackingRequested;
  doc["control"]["target_source"]=issPositionValid?"sgp4":"pending";
  doc["control"]["motors_enabled"]=false;
  doc["last_command_sequence"]=localCommandSequence;
  String body; serializeJson(doc,body);
  sendLocalCorsHeaders();
  localServer.sendHeader("Cache-Control","no-store");
  localServer.send(200,"application/json; charset=utf-8",body);
}
static void startLocalApi() {
  if(localApiStarted || WiFi.status()!=WL_CONNECTED) return;
  localServer.on("/",HTTP_GET,[](){
    static const char page[] PROGMEM=R"HTML(<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#071321"><title>CYD Satellite Tracker · Local</title>
<style>
:root{color-scheme:dark;--bg:#050d18;--side:#081727;--panel:#0c1d30;--panel2:#102740;--line:#1d3d58;--text:#eef7ff;--muted:#8ba7c0;--blue:#4abfff;--cyan:#8de5ff;--green:#59dfad;--amber:#ffc36b;--red:#ff647e}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at 70% -15%,#123a5c 0,transparent 38%),var(--bg);color:var(--text);font:14px -apple-system,BlinkMacSystemFont,"SF Pro Text","Segoe UI",sans-serif;min-height:100vh}
button{font:inherit;cursor:pointer}.layout{min-height:100vh}.sidebar{position:fixed;inset:0 auto 0 0;width:226px;padding:25px 15px;background:linear-gradient(180deg,#091a2b,#06111e);border-right:1px solid #1a354d;display:flex;flex-direction:column;z-index:5}
.brand{display:flex;align-items:center;gap:11px;padding:0 8px 30px}.brand-mark{width:36px;height:36px;border-radius:12px;display:grid;place-items:center;color:#031321;font-weight:900;font-size:19px;background:linear-gradient(145deg,#8ce5ff,#2997d6);box-shadow:0 0 25px #32a9ed55}.brand strong{display:block;font-size:12px;letter-spacing:.09em}.brand small{display:block;color:#6991af;font-size:9px;letter-spacing:.16em;margin-top:4px}
.nav-label{font:9px monospace;letter-spacing:.16em;color:#547895;padding:0 12px 10px}.nav{display:grid;gap:6px}.nav button{border:1px solid transparent;background:transparent;color:#9db5c9;text-align:left;border-radius:10px;padding:12px;display:flex;align-items:center;gap:12px}.nav button span{font-size:17px;color:#6dbbe8;width:20px;text-align:center}.nav button.active,.nav button:hover{background:linear-gradient(100deg,#123957,#10283e);border-color:#285a7b;color:white;box-shadow:inset 3px 0 var(--blue)}.side-foot{margin-top:auto;border-top:1px solid #1b354c;padding:16px 10px 3px;color:#7e9ab1;font-size:10px;line-height:1.7}.side-foot b{color:#c9eaff;font-weight:600}
.main{margin-left:226px;min-height:100vh}.top{height:65px;position:sticky;top:0;z-index:4;background:#071523eF;backdrop-filter:blur(15px);border-bottom:1px solid #1a354d;display:flex;align-items:center;justify-content:space-between;padding:0 30px}.crumb{font:10px monospace;letter-spacing:.1em;color:#7797b2}.crumb b{color:#eaf6ff;font-weight:500}.top-right{display:flex;align-items:center;gap:16px}.live{font:10px monospace;color:var(--green);display:flex;align-items:center;gap:8px}.dot{width:7px;height:7px;border-radius:50%;background:var(--green);box-shadow:0 0 12px var(--green)}.refresh{border:1px solid #28516f;border-radius:9px;background:#102b43;color:#c9eaff;padding:9px 12px;font-size:11px;font-weight:700}.content{max-width:1450px;margin:auto;padding:28px 30px 45px}.heading{display:flex;align-items:flex-end;justify-content:space-between;gap:18px;margin-bottom:22px}.eyebrow{font:9px monospace;letter-spacing:.18em;color:#60a8d2}.heading h1{font-size:clamp(26px,3vw,37px);letter-spacing:-.055em;margin:8px 0 7px}.heading p{color:var(--muted);font-size:12px;margin:0}.badge{display:inline-flex;align-items:center;gap:7px;border:1px solid #24624f;background:#10372f;color:#7ce4b7;padding:7px 10px;border-radius:99px;font:9px monospace;white-space:nowrap}.badge.warn{border-color:#70532b;background:#332719;color:#ffd18b}.grid{display:grid;gap:14px}.hero-grid{grid-template-columns:minmax(0,1.55fr) minmax(260px,.8fr)}.panel{background:linear-gradient(145deg,#10263cee,#091a2bf5);border:1px solid #203f59;border-radius:16px;padding:18px;box-shadow:0 14px 34px #0002;min-width:0}.panel-head{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-bottom:16px}.panel-head h2{font-size:13px;margin:0;font-weight:650}.micro{font:9px monospace;letter-spacing:.1em;color:#6e94b2}.scene{height:250px;position:relative;overflow:hidden;border-radius:12px;background:radial-gradient(ellipse at 50% 100%,#0b5279 0,transparent 58%),linear-gradient(180deg,#071a2c,#061321);display:grid;place-items:center}.planet{width:170px;height:170px;border-radius:50%;position:relative;background:radial-gradient(circle at 32% 27%,#70dfff 0,#1b76a4 22%,#0a3a63 58%,#03172d 78%);box-shadow:inset -22px -15px 30px #020914,0 0 40px #159de277,0 0 100px #168fd633}.planet:before{content:"";position:absolute;inset:0;border-radius:50%;background:radial-gradient(ellipse at 28% 42%,#8dc78a 0 8%,transparent 9%),radial-gradient(ellipse at 60% 35%,#75b77d 0 13%,transparent 14%),radial-gradient(ellipse at 45% 68%,#4b9b79 0 10%,transparent 11%),radial-gradient(ellipse at 73% 63%,#78b982 0 8%,transparent 9%);filter:blur(4px);opacity:.7}.orbit{position:absolute;width:285px;height:90px;border:1px solid #65d5ff99;border-radius:50%;transform:rotate(-24deg)}.orbit.two{width:255px;height:135px;transform:rotate(42deg);border-color:#5de3bb55}.scene-label{position:absolute;left:15px;bottom:13px;font:9px monospace;color:#a8d9f3;letter-spacing:.08em}.scene-note{position:absolute;right:14px;top:13px;font:8px monospace;color:#779bb6;background:#071827bb;border:1px solid #294a63;padding:6px 8px;border-radius:7px}.scene-foot{display:flex;justify-content:space-between;gap:12px;color:#8ca9c0;font-size:10px;padding:13px 3px 0}.scene-foot b{color:#d9efff;font-weight:600}
.metrics{grid-template-columns:repeat(2,minmax(0,1fr))}.metric{padding:14px;border:1px solid #1b3a54;background:#091b2d;border-radius:12px;min-width:0}.metric small{display:block;color:#7899b5;font:9px monospace;letter-spacing:.1em;text-transform:uppercase}.metric strong{display:block;font:20px monospace;color:#e3f5ff;margin-top:9px;overflow-wrap:anywhere}.metric em{font:10px -apple-system,sans-serif;color:#7797b2;font-style:normal}.statusline{display:flex;justify-content:space-between;gap:10px;padding:12px 0;border-bottom:1px solid #1b3850;font-size:11px;color:#a7bfd2}.statusline:last-child{border:0}.statusline b{font:10px monospace;color:#d8edff;text-align:right}.bar{height:5px;border-radius:9px;background:#1a344b;overflow:hidden;margin-top:10px}.bar i{display:block;height:100%;width:0;background:linear-gradient(90deg,#218dce,#72dcff);border-radius:9px;transition:width .3s}
.lower{grid-template-columns:1fr 1fr;margin-top:14px}.device-address{font:12px monospace;color:#9bdcff;overflow-wrap:anywhere}.actions{display:flex;gap:9px;flex-wrap:wrap;margin-top:14px}.btn{border:1px solid #2a5778;border-radius:9px;background:#12314b;color:#d8efff;padding:10px 13px;font-size:10px;font-weight:750}.btn:hover{border-color:#64c9ff}.btn.stop{background:#452331;border-color:#844253;color:#ffc1cd}.hint{margin:13px 0 0;color:#809db5;font-size:10px;line-height:1.65}.view{display:none}.view.active{display:block}.system-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.mobile-nav{display:none}
@media(max-width:900px){.sidebar{width:190px}.main{margin-left:190px}.content{padding:22px 18px}.top{padding:0 18px}.hero-grid{grid-template-columns:1fr}.scene{height:230px}}
@media(max-width:620px){.sidebar{display:none}.main{margin-left:0}.top{height:58px;padding:0 14px}.content{padding:20px 13px 32px}.heading{align-items:flex-start;flex-direction:column}.top-right{gap:9px}.live{font-size:9px}.hero-grid,.lower,.system-grid{grid-template-columns:1fr}.metrics{grid-template-columns:repeat(2,minmax(0,1fr))}.metric strong{font-size:17px}.mobile-nav{display:flex;position:sticky;bottom:0;z-index:8;background:#071523f2;border-top:1px solid #24445e;justify-content:space-around;padding:8px 4px calc(8px + env(safe-area-inset-bottom));backdrop-filter:blur(12px)}.mobile-nav button{background:transparent;border:0;color:#87a6bf;font-size:9px;display:grid;gap:3px;justify-items:center}.mobile-nav button span{font-size:17px}.mobile-nav button.active{color:#8cddff}.scene{height:215px}}
</style></head>
<body><div class="layout"><aside class="sidebar"><div class="brand"><div class="brand-mark">C</div><div><strong>CYD SATELLITE</strong><small>TRACKER · LOCAL</small></div></div><div class="nav-label">СТАНЦИЯ</div><nav class="nav"><button class="active" data-view="overview"><span>⌂</span>Обзор</button><button data-view="system"><span>◈</span>Система</button><button data-view="connection"><span>⌁</span>Подключение</button></nav><div class="side-foot">Локальный режим<br><b id="sideIp">Ожидание IP…</b><br>Управление в пределах Wi‑Fi сети</div></aside>
<div class="main"><header class="top"><div class="crumb">CYD / <b id="crumbName">ОБЗОР СТАНЦИИ</b></div><div class="top-right"><div class="live"><i class="dot"></i><span id="topState">ПОДКЛЮЧЕНО</span></div><button class="refresh" onclick="refreshStatus()">↻ ОБНОВИТЬ</button></div></header>
<main class="content">
<section class="view active" id="overview"><div class="heading"><div><div class="eyebrow">LOCAL CONTROL CENTER / V81</div><h1>Центр управления</h1><p>Состояние контроллера и локального соединения в реальном времени</p></div><div class="badge" id="connectionBadge"><i class="dot"></i> ЛОКАЛЬНАЯ СЕТЬ</div></div>
<div class="grid hero-grid"><article class="panel"><div class="panel-head"><h2>Орбитальная обстановка</h2><span class="micro">ВИЗУАЛЬНЫЙ МОДУЛЬ</span></div><div class="scene"><div class="orbit"></div><div class="orbit two"></div><div class="planet"></div><div class="scene-note">ОРБИТЫ: НЕТ ДАННЫХ</div><div class="scene-label">CYD SATELLITE TRACKER</div></div><div class="scene-foot"><span>Режим отображения</span><b>Локальная диагностика</b></div><p class="hint">Орбитальные позиции и каталог спутников появятся после подключения источника TLE. Сейчас здесь показана только визуальная оболочка интерфейса — реальные координаты спутников не имитируются.</p></article>
<article class="panel"><div class="panel-head"><h2>Телеметрия устройства</h2><span class="micro">АВТООБНОВЛЕНИЕ · 1 СЕК</span></div><div class="grid metrics"><div class="metric"><small>Wi‑Fi сигнал</small><strong id="rssi">—</strong><em id="rssiLabel">Ожидание</em><div class="bar"><i id="rssiBar"></i></div></div><div class="metric"><small>GPS модуль</small><strong id="gps">—</strong><em id="gpsLabel">Статус фиксации</em></div><div class="metric"><small>GPS спутники</small><strong id="sats">—</strong><em>В зоне приёма</em></div><div class="metric"><small>Режим работы</small><strong id="cloud">—</strong><em id="cloudLabel">Работа без облака</em></div></div><div class="statusline"><span>Прошивка</span><b id="firmware">V83</b></div><div class="statusline"><span>Состояние API</span><b id="apiState">Проверка…</b></div></article></div>
<div class="grid lower"><article class="panel"><div class="panel-head"><h2>Положение станции</h2><span class="micro">GPS / GNSS</span></div><div class="statusline"><span>Широта</span><b id="lat">Нет фиксации</b></div><div class="statusline"><span>Долгота</span><b id="lon">Нет фиксации</b></div><div class="statusline"><span>Высота</span><b id="alt">—</b></div><p class="hint" id="gpsHint">GPS-модуль пока не выдал действительные координаты. Проверьте антенну GPS и подключение NEO-6M.</p></article><article class="panel"><div class="panel-head"><h2>Локальное устройство</h2><span class="micro">ESP32 / WI‑FI</span></div><div class="statusline"><span>Имя устройства</span><b>CYD Satellite Tracker</b></div><div class="statusline"><span>IP-адрес</span><b id="ip">—</b></div><div class="statusline"><span>mDNS</span><b id="hostname">cyd-tracker.local</b></div><div class="statusline"><span>Связь</span><b id="connectionType">Wi‑Fi · HTTP</b></div><div class="actions"><button class="btn" onclick="copyAddress()">КОПИРОВАТЬ IP</button><button class="btn" onclick="openSystem()">СИСТЕМА</button></div></article></div>
 <article class="panel" style="margin-top:14px"><div class="panel-head"><h2>Управление антенной · локальные команды</h2><span class="badge warn">МОТОРЫ ЗАБЛОКИРОВАНЫ</span></div>
 <div class="grid metrics control-grid"><label class="metric"><small>Азимут · 0–359.9°</small><input id="azInput" type="number" min="0" max="359.9" step="0.1" value="0" style="width:100%;margin-top:10px;padding:11px;border-radius:8px;border:1px solid #28516f;background:#071827;color:#eaf6ff;font:18px monospace"></label><label class="metric"><small>Угол места · 0–90°</small><input id="elInput" type="number" min="0" max="90" step="0.1" value="0" style="width:100%;margin-top:10px;padding:11px;border-radius:8px;border:1px solid #28516f;background:#071827;color:#eaf6ff;font:18px monospace"></label></div>
 <div class="actions"><button class="btn" id="sendTargetBtn" onclick="sendTarget()">ОТПРАВИТЬ ЦЕЛЬ</button><button class="btn" id="trackingBtn" onclick="toggleTracking()">СЛЕЖЕНИЕ: ВЫКЛ</button><button class="btn stop" onclick="requestStop()">STOP</button></div>
 <div class="statusline"><span>Принятая цель</span><b id="targetState">Азимут 0° · место 0°</b></div><div class="statusline"><span>Режим слежения</span><b id="trackingState">Выключен</b></div><p class="hint" id="commandMessage">Команды принимаются по локальному Wi‑Fi. Сейчас прошивка только сохраняет целевые углы и флаг слежения — физическое движение двигателей не запускается.</p>
 </article></section>
<section class="view" id="system"><div class="heading"><div><div class="eyebrow">DEVICE HEALTH / ESP32</div><h1>Система</h1><p>Диагностика платы, GPS и сетевого соединения</p></div></div><div class="grid system-grid"><article class="panel"><div class="panel-head"><h2>Контроллер</h2><span class="micro">ESP32</span></div><div class="statusline"><span>Устройство</span><b>CYD Satellite Tracker</b></div><div class="statusline"><span>Версия ПО</span><b id="sysFirmware">—</b></div><div class="statusline"><span>IP</span><b id="sysIp">—</b></div><div class="statusline"><span>Wi‑Fi RSSI</span><b id="sysRssi">—</b></div></article><article class="panel"><div class="panel-head"><h2>GPS / GNSS</h2><span class="micro">NEO-6M</span></div><div class="statusline"><span>Фиксация</span><b id="sysGps">—</b></div><div class="statusline"><span>Спутники</span><b id="sysSats">—</b></div><div class="statusline"><span>Широта</span><b id="sysLat">—</b></div><div class="statusline"><span>Долгота</span><b id="sysLon">—</b></div></article><article class="panel"><div class="panel-head"><h2>Безопасность управления</h2><span class="micro">ТЕКУЩИЕ ОГРАНИЧЕНИЯ</span></div><div class="badge warn">ДВИЖЕНИЕ ЗАБЛОКИРОВАНО</div><p class="hint">Команды поворота антенны ещё не реализованы. Драйверы и двигатели нельзя включать до проверки концевых выключателей, аппаратного STOP и механических ограничителей.</p><div class="actions"><button class="btn stop" onclick="requestStop()">ТЕСТОВЫЙ ЗАПРОС STOP</button></div><p class="hint">Эта кнопка отправляет диагностический запрос. Она не является аварийной остановкой и не отключает питание двигателей.</p></article><article class="panel"><div class="panel-head"><h2>Локальная связь</h2><span class="micro">LOCAL LINK</span></div><div class="statusline"><span>Привязка</span><b id="sysCloud">—</b></div><p class="hint">Прошивка не обращается к облачным серверам. Телеметрия и команды передаются напрямую между браузером и CYD по локальному Wi‑Fi.</p></article></div></section>
<section class="view" id="connection"><div class="heading"><div><div class="eyebrow">NETWORK / LOCAL ACCESS</div><h1>Подключение</h1><p>Адреса для браузера и мобильного приложения в этой Wi‑Fi сети</p></div></div><article class="panel"><div class="panel-head"><h2>Доступ к контроллеру</h2><span class="badge"><i class="dot"></i> API ДОСТУПЕН</span></div><p class="micro">ЛОКАЛЬНЫЙ HTTP АДРЕС</p><p class="device-address" id="fullAddress">http://192.168.0.48/</p><div class="actions"><button class="btn" onclick="copyAddress()">КОПИРОВАТЬ АДРЕС</button><button class="btn" onclick="window.location.href='/api/v1/status'">ОТКРЫТЬ JSON API</button></div><div class="statusline"><span>mDNS адрес</span><b>http://cyd-tracker.local/</b></div><div class="statusline"><span>Статус API</span><b id="connApi">Работает</b></div><div class="statusline"><span>Обновление данных</span><b>Каждую секунду</b></div><p class="hint">Телефон, планшет или компьютер должны быть подключены к той же Wi‑Fi сети, что и CYD. Для локального управления используйте этот адрес, а не HTTPS-сайт Cloudflare: браузер ограничивает обращения с HTTPS-страницы к локальному HTTP-устройству.</p></article></section>
</main><nav class="mobile-nav"><button class="active" data-view="overview"><span>⌂</span>Обзор</button><button data-view="system"><span>◈</span>Система</button><button data-view="connection"><span>⌁</span>Сеть</button></nav></div></div>
<script>
let lastStatus=null;let targetInputsDirty=false;let targetSendBusy=false;const $=id=>document.getElementById(id);function showView(name){document.querySelectorAll('.view').forEach(v=>v.classList.toggle('active',v.id===name));document.querySelectorAll('[data-view]').forEach(b=>b.classList.toggle('active',b.dataset.view===name));$('crumbName').textContent=({overview:'ОБЗОР СТАНЦИИ',system:'СИСТЕМА',connection:'ПОДКЛЮЧЕНИЕ'})[name]||'ОБЗОР';window.scrollTo({top:0,behavior:'smooth'});}document.querySelectorAll('[data-view]').forEach(b=>b.addEventListener('click',()=>showView(b.dataset.view)));function openSystem(){showView('system')}
function set(id,value){const e=$(id);if(e)e.textContent=value}function fmt(v,n=6){return typeof v==='number'?v.toFixed(n):'—'}
async function refreshStatus(){try{const response=await fetch('/api/v1/status',{cache:'no-store'});if(!response.ok)throw new Error('HTTP '+response.status);const d=await response.json();lastStatus=d;set('topState','ПОДКЛЮЧЕНО');set('apiState','ОТВЕЧАЕТ · HTTP 200');set('connApi','Работает · HTTP 200');set('firmware',d.firmware||'—');set('sysFirmware',d.firmware||'—');set('ip',d.ip||'—');set('sysIp',d.ip||'—');set('sideIp',d.ip||'—');set('hostname',d.hostname||'—');set('connectionType',d.connection||'local-wifi');set('fullAddress','http://'+(d.ip||location.hostname)+'/');set('rssi',(d.wifi_rssi??'—')+' dBm');set('sysRssi',(d.wifi_rssi??'—')+' dBm');const pct=Math.max(0,Math.min(100,((Number(d.wifi_rssi)+100)/60)*100));$('rssiBar').style.width=pct+'%';set('rssiLabel',d.wifi_rssi>-60?'Отличный сигнал':d.wifi_rssi>-75?'Хороший сигнал':'Слабый сигнал');const gps=!!d.gps_valid;set('gps',gps?'FIX':'ПОИСК');set('gpsLabel',gps?'Координаты получены':'Ожидание фиксации');set('sysGps',gps?'Есть фиксация':'Нет фиксации');set('sats',d.gps_satellites??'—');set('sysSats',d.gps_satellites??'—');set('lat',gps?fmt(d.latitude):'Нет фиксации');set('lon',gps?fmt(d.longitude):'Нет фиксации');set('sysLat',gps?fmt(d.latitude):'—');set('sysLon',gps?fmt(d.longitude):'—');set('alt',d.altitude_m!=null?d.altitude_m.toFixed(1)+' м':'—');set('gpsHint',gps?'GPS-координаты получены от модуля.':'GPS-модуль пока не выдал действительные координаты. Проверьте антенну GPS и подключение NEO-6M.');set('cloud','LOCAL');set('cloudLabel','Работа только в локальной сети');set('sysCloud','Облако отключено');updateControlState(d);$('connectionBadge').classList.toggle('warn',false);$('topState').previousElementSibling.style.background='var(--green)';}catch(e){set('topState','НЕТ СВЯЗИ');set('apiState','ОШИБКА СОЕДИНЕНИЯ');set('connApi','Нет ответа');$('topState').previousElementSibling.style.background='var(--red)';}}
function updateControlState(d){const c=d.control||{};const az=Number(c.target_azimuth_deg??0),el=Number(c.target_elevation_deg??0);if(!targetInputsDirty){$('azInput').value=az.toFixed(1);$('elInput').value=el.toFixed(1);}set('targetState','Азимут '+az.toFixed(1)+'° · место '+el.toFixed(1)+'°');const on=c.tracking_requested===true;set('trackingState',on?'Запрошено':'Выключено');set('trackingBtn',on?'СЛЕЖЕНИЕ: ВКЛ':'СЛЕЖЕНИЕ: ВЫКЛ');}
function copyAddress(){const a='http://'+(lastStatus?.ip||location.hostname)+'/';if(navigator.clipboard?.writeText)navigator.clipboard.writeText(a).then(()=>alert('Адрес скопирован: '+a)).catch(()=>prompt('Скопируйте адрес',a));else prompt('Скопируйте адрес',a)}
async function sendCommand(payload){try{const r=await fetch('/api/v1/command',{method:'POST',headers:{'Content-Type':'application/json'},cache:'no-store',body:JSON.stringify(payload)});const d=await r.json();if(!r.ok||!d.ok)throw new Error(d.message||d.error||('HTTP '+r.status));set('commandMessage',d.message||'Команда принята');return d;}catch(e){set('commandMessage','Ошибка команды: '+e.message);throw e;}}
$('azInput').addEventListener('input',()=>{targetInputsDirty=true;});
$('elInput').addEventListener('input',()=>{targetInputsDirty=true;});
async function sendTarget(){if(targetSendBusy)return;const azText=$('azInput').value.trim(),elText=$('elInput').value.trim();if(azText===''||elText===''){set('commandMessage','Введите оба значения: азимут и угол места.');return;}const az=Number(azText),el=Number(elText);if(!Number.isFinite(az)||!Number.isFinite(el)||az<0||az>=360||el<0||el>90){set('commandMessage','Проверьте углы: азимут 0–менее 360°, угол места 0–90°.');return;}const btn=$('sendTargetBtn');targetSendBusy=true;btn.disabled=true;btn.textContent='ОТПРАВКА…';set('commandMessage','Передаю азимут и угол места одним запросом…');try{const d=await sendCommand({command:'set_target',azimuth_deg:az,elevation_deg:el});targetInputsDirty=false;set('commandMessage',d.message||'Оба угла приняты контроллером.');await refreshStatus();}catch(e){}finally{targetSendBusy=false;btn.disabled=false;btn.textContent='ОТПРАВИТЬ ЦЕЛЬ';}}
 async function toggleTracking(){const enabled=!(lastStatus?.control?.tracking_requested===true);try{await sendCommand({command:'tracking',enabled});}catch(e){}}
 async function requestStop(){if(!confirm('Отправить STOP? Это программная команда, не аппаратная аварийная остановка.'))return;try{await sendCommand({command:'stop'});}catch(e){}}
refreshStatus();setInterval(refreshStatus,1000);
</script></body></html>)HTML";
    // Keep the page in static flash storage; never allocate this large HTML
    // buffer on the request-handler stack or copy it into the heap.
    Serial.printf("[LOCAL] GET / begin | free_heap=%u min_heap=%u\\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    localServer.send_P(200,"text/html; charset=utf-8",page);
    Serial.printf("[LOCAL] GET / sent | free_heap=%u min_heap=%u\\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  });
  localServer.on("/api/v1/status",HTTP_GET,sendLocalStatus);
  localServer.on("/api/v1/status",HTTP_OPTIONS,[](){sendLocalCorsHeaders();localServer.send(204,"text/plain","");});
  localServer.on("/api/v1/command",HTTP_OPTIONS,[](){sendLocalCorsHeaders();localServer.send(204,"text/plain","");});
  localServer.on("/api/v1/command",HTTP_POST,[](){
    sendLocalCorsHeaders();
    JsonDocument req; JsonDocument out;
    if(deserializeJson(req,localServer.arg("plain"))!=DeserializationError::Ok) {
      out["ok"]=false; out["error"]="invalid_json";
      String body; serializeJson(out,body); localServer.send(400,"application/json",body); return;
    }
    const String command=req["command"]| "";
    String message;
    if(command=="stop") {
      trackingRequested=false;
      message="STOP принят. Двигатели заблокированы; аппаратное движение не включено.";
    } else if(command=="set_target") {
      if(!(req["azimuth_deg"].is<float>() || req["azimuth_deg"].is<int>() || req["azimuth_deg"].is<long>()) || !(req["elevation_deg"].is<float>() || req["elevation_deg"].is<int>() || req["elevation_deg"].is<long>())) {
        out["ok"]=false; out["error"]="invalid_target";
        out["message"]="Нужны числовые azimuth_deg и elevation_deg.";
        String body; serializeJson(out,body); localServer.send(400,"application/json",body); return;
      }
      const float az=req["azimuth_deg"].as<float>();
      const float el=req["elevation_deg"].as<float>();
      if(!isfinite(az) || !isfinite(el) || az<0.0f || az>=360.0f || el<0.0f || el>90.0f) {
        out["ok"]=false; out["error"]="target_out_of_range";
        out["message"]="Азимут: 0–менее 360°, угол места: 0–90°.";
        String body; serializeJson(out,body); localServer.send(400,"application/json",body); return;
      }
      targetAzimuthDeg=az; targetElevationDeg=el;
      message="Целевые углы сохранены. Моторы пока не задействованы.";
    } else if(command=="follow_satellite") {
      const uint32_t norad=req["norad_id"]|0U;
      if(norad<1 || norad>999999) {
        out["ok"]=false; out["error"]="invalid_norad_id";
        out["message"]="Нужен действительный NORAD ID от 1 до 999999.";
        String body; serializeJson(out,body); localServer.send(400,"application/json",body); return;
      }
      selectedNoradId=norad;
      const String requestedName=req["name"]| "";
      selectedSatelliteName=requestedName.length()?requestedName:("NORAD "+String(norad));
      selectedSatelliteImageUrl=req["image_url"]|"";
      if(!isAllowedSatelliteImageUrl(selectedSatelliteImageUrl)) selectedSatelliteImageUrl="";
      selectedSatelliteImagePending=selectedSatelliteImageUrl.length()>0;
      if(selectedSatelliteImageData) { free(selectedSatelliteImageData); selectedSatelliteImageData=null; }
      selectedSatelliteImageSize=0;
      if(screen==Screen::Dashboard) drawSelectedSatellitePhoto();
      issTleValid=false; issPositionValid=false;
      lastTleAttemptMs=0; lastTleSuccessMs=0; lastOrbitCalcMs=0;
      targetAzimuthDeg=0.0f;
      targetElevationDeg=0.0f;
      trackingRequested=true;
      orbitDataMessage="Загрузка орбиты выбранного спутника";
      message="Выбран "+selectedSatelliteName+" · NORAD "+String(selectedNoradId)+". Загружаю TLE и рассчитываю SGP4.";
    } else if(command=="tracking") {
      if(!req["enabled"].is<bool>()) {
        out["ok"]=false; out["error"]="invalid_enabled";
        String body; serializeJson(out,body); localServer.send(400,"application/json",body); return;
      }
      trackingRequested=req["enabled"].as<bool>();
      message=trackingRequested ? "Запрос слежения принят; движение моторов заблокировано." : "Слежение выключено.";
    } else {
      out["ok"]=false; out["error"]="unsupported_command";
      out["message"]="Допустимые команды: stop, set_target, follow_satellite, tracking.";
      String body; serializeJson(out,body); localServer.send(400,"application/json",body); return;
    }
    ++localCommandSequence;
    out["ok"]=true; out["command"]=command; out["sequence"]=localCommandSequence;
    out["message"]=message; out["motors_enabled"]=false;
    String body; serializeJson(out,body);
    localServer.sendHeader("Cache-Control","no-store");
    localServer.send(200,"application/json; charset=utf-8",body);
    Serial.printf("[LOCAL] command=%s seq=%lu motors=LOCKED\\n",command.c_str(),(unsigned long)localCommandSequence);
  });
  localServer.onNotFound([](){
    localServer.send(404,"application/json","{\"ok\":false,\"error\":\"not_found\"}");
  });
  localServer.begin();
  if(MDNS.begin("cyd-tracker")) MDNS.addService("http","tcp",80);
  localApiStarted=true;
  Serial.printf("[LOCAL] API ready: http://%s/ and http://cyd-tracker.local/\\n",WiFi.localIP().toString().c_str());
}
static void serviceLocalApi() {
  if(WiFi.status()!=WL_CONNECTED) { localApiStarted=false; return; }
  if(!localApiStarted) startLocalApi();
  if(localApiStarted) localServer.handleClient();
}

void setup() {
  Serial.begin(115200);
  delay(150);
  pinMode(Pins::TFT_BACKLIGHT,OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT,HIGH); // Constant backlight; no PWM/fade in firmware.
  tft.init();
  tft.invertDisplay(true);
  pinMode(TOUCH_CS,OUTPUT);digitalWrite(TOUCH_CS,HIGH);
  pinMode(TOUCH_IRQ_PIN,INPUT);
  touchSPI.begin(TOUCH_CLK_PIN,TOUCH_MISO_PIN,TOUCH_MOSI_PIN,TOUCH_CS);
  tft.setRotation(DISPLAY_ROTATION);
  tft.fillScreen(TFT_BLACK);
  Serial2.begin(9600,SERIAL_8N1,Pins::GPS_RX,-1);
  prefs.begin("cyd-tracker",false);
  char hardwareIdBuffer[20];
  snprintf(hardwareIdBuffer,sizeof(hardwareIdBuffer),"CYD-%012llX",(unsigned long long)ESP.getEfuseMac());
  deviceHardwareId=hardwareIdBuffer;
  Serial.printf("[DEVICE] hardware_id=%s\\n",deviceHardwareId.c_str());
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
  handleSerialDisplayDiagnostics();
  while(Serial2.available()) {
    if(gps.encode(Serial2.read()) && gps.location.isValid()) gpsSeen=true;
  }
  const uint32_t now=millis();
  if(!displayDiagnosticActive && screen==Screen::Splash && now-screenStartedAt>=SPLASH_MS) {
    if(touchCalibrated) { screen=Screen::Init; showInitialization(); screenStartedAt=now; }
    else {
      // Skip the obsolete TOUCH TEST screen and go straight to calibration.
      touchCalStep=1; touchWasDown=false; screen=Screen::TouchTest;
      drawCalibrationTarget(0);
    }
    screenStartedAt=now;
  } else if(!displayDiagnosticActive && screen==Screen::TouchTest) {
    if(touchCalStep>=1&&!touchCalibrated)updateTouchCalibration();else updateTouchDiagnostic();
  } else if(!displayDiagnosticActive && screen==Screen::Init && now-screenStartedAt>=INIT_MS) {
    if(wifiName.length()) beginWiFi();
    else { setupMessage="Enter your router details"; showWifiSetup(); }
  } else if(!displayDiagnosticActive && screen==Screen::Connecting) {
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
  if(!displayDiagnosticActive) handleTouch();
  if(!displayDiagnosticActive) { serviceLocalApi(); } // Browser and CYD communicate only over the local network.
  if(!displayDiagnosticActive) updateSelectedOrbit();
  if(!displayDiagnosticActive && selectedSatelliteImagePending) {
    selectedSatelliteImagePending=false;
    if(fetchSelectedSatelliteImage() && screen==Screen::Dashboard) drawSelectedSatellitePhoto();
  }
  // Refresh only dynamic telemetry; keep artwork and navigation static.
  if(!displayDiagnosticActive && now-lastDashboardRefreshAt>=1000) {
    if(screen==Screen::Dashboard) { updateGpsTelemetry(); updateTargetTelemetry(); }
    if(screen==Screen::Antenna) updateTargetTelemetry();
    if(screen==Screen::Satellite) updateOrbitTelemetry();
    lastDashboardRefreshAt=now;
  }
}
