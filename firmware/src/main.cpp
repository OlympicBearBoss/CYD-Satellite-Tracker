#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BoardConfig.h"

// v16 hardware isolation test: no sprites, no framebuffer, no rotation helpers.
TFT_eSPI tft;

void drawPattern() {
  tft.fillScreen(TFT_BLACK);
  const int16_t w = tft.width();
  const int16_t h = tft.height();
  Serial.printf("TFT native geometry: %d x %d\\n", w, h);

  // Four large solid RGB565 fields make pixel corruption easy to identify.
  const int16_t band = h / 4;
  tft.fillRect(0, 0, w, band, TFT_RED);
  tft.fillRect(0, band, w, band, TFT_GREEN);
  tft.fillRect(0, band * 2, w, band, TFT_BLUE);
  tft.fillRect(0, band * 3, w, h - band * 3, TFT_WHITE);

  tft.drawRect(2, 2, w - 4, h - 4, TFT_YELLOW);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawCentreString("DIRECT TFT TEST v16", w / 2, band * 3 + band / 2, 2);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawCentreString("RED", w / 2, band / 2, 2);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.drawCentreString("GREEN", w / 2, band + band / 2, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawCentreString("BLUE", w / 2, band * 2 + band / 2, 2);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(Pins::TFT_BACKLIGHT, OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT, HIGH);
  tft.init();
  tft.setRotation(1); // native 320x240 landscape
  drawPattern();
}

void loop() {
  delay(1000);
}
