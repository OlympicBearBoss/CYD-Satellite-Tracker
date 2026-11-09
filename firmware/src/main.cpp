#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BoardConfig.h"

TFT_eSPI tft;
static uint32_t lastPageChange = 0;
static uint8_t pageIndex = 0;

static const uint16_t pageColors[] = { TFT_BLACK, TFT_NAVY, TFT_DARKGREEN, TFT_MAROON };
static const char* pageNames[] = { "BLACK GRID", "BLUE GRID", "GREEN GRID", "RED GRID" };

void drawGridTest() {
  const uint16_t bg = pageColors[pageIndex];
  tft.fillScreen(bg);
  const int16_t w = tft.width();
  const int16_t h = tft.height();
  const uint16_t grid = TFT_DARKGREY;

  tft.drawRect(0, 0, w, h, TFT_WHITE);
  for (int16_t x = 0; x < w; x += 40) tft.drawFastVLine(x, 0, h, grid);
  for (int16_t y = 0; y < h; y += 40) tft.drawFastHLine(0, y, w, grid);

  const int16_t m = 14;
  tft.fillRect(2, 2, m, m, TFT_RED);
  tft.fillRect(w - m - 2, 2, m, m, TFT_GREEN);
  tft.fillRect(2, h - m - 2, m, m, TFT_BLUE);
  tft.fillRect(w - m - 2, h - m - 2, m, m, TFT_YELLOW);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, bg);
  for (int16_t x = 0; x < w; x += 40) {
    char label[12];
    snprintf(label, sizeof(label), "X:%d", x);
    tft.drawString(label, x + 3, 19, 2);
  }
  for (int16_t y = 0; y < h; y += 40) {
    char label[12];
    snprintf(label, sizeof(label), "Y:%d", y);
    tft.drawString(label, 3, y + 3, 2);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, bg);
  char title[48];
  snprintf(title, sizeof(title), "CYD V20  %s", pageNames[pageIndex]);
  tft.drawCentreString(title, w / 2, h / 2, 4);
  Serial.printf("V20 GRID TEST: rotation=2 geometry=%dx%d page=%u (%s)\\n",
                w, h, pageIndex, pageNames[pageIndex]);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(Pins::TFT_BACKLIGHT, OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT, HIGH);
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  pageIndex = 0;
  drawGridTest();
  lastPageChange = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastPageChange >= 5000) {
    lastPageChange = now;
    pageIndex = (pageIndex + 1) % (sizeof(pageColors) / sizeof(pageColors[0]));
    drawGridTest();
  }
}
