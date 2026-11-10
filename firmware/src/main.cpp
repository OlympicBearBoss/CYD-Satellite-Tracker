#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BoardConfig.h"

TFT_eSPI tft;
static uint32_t lastPageChange = 0;
static uint8_t pageIndex = 0;

static const uint16_t pageColors[] = { TFT_BLACK, TFT_NAVY, TFT_DARKGREEN, TFT_MAROON };
static const char* pageNames[] = { "BLACK GRID", "BLUE GRID", "GREEN GRID", "RED GRID" };

static int16_t scaleCoord(int16_t value, int16_t extent) {
  return (int32_t)value * (extent - 1) / 200;
}

void drawGridTest() {
  const uint16_t bg = pageColors[pageIndex];
  tft.fillScreen(bg);
  const int16_t w = tft.width();
  const int16_t h = tft.height();
  const uint16_t grid = TFT_DARKGREY;

  tft.drawRect(0, 0, w, h, TFT_WHITE);
  // Both 0..200 axes are mapped over the complete active display area.
  // Keep the previously confirmed orientation (rotation 2); scale both axes to the full active bounds.
  for (int16_t value = 0; value <= 200; value += 50) {
    const int16_t x = scaleCoord(value, w);
    tft.drawFastVLine(x, 0, h, grid);
  }
  for (int16_t value = 0; value <= 200; value += 50) {
    const int16_t y = scaleCoord(value, h);
    tft.drawFastHLine(0, y, w, grid);
  }

  const int16_t m = 14;
  tft.fillRect(2, 2, m, m, TFT_RED);
  tft.fillRect(w - m - 2, 2, m, m, TFT_GREEN);
  tft.fillRect(2, h - m - 2, m, m, TFT_BLUE);
  tft.fillRect(w - m - 2, h - m - 2, m, m, TFT_YELLOW);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, bg);
  for (int16_t value = 0; value <= 200; value += 50) {
    const int16_t x = (int32_t)value * (w - 1) / 200;
    char label[12];
    snprintf(label, sizeof(label), "X:%d", value);
    const int16_t labelX = (x < 24) ? 2 : (x > w - 34 ? w - 38 : x - 16);
    tft.drawString(label, labelX, 19, 2);
  }
  for (int16_t value = 0; value <= 200; value += 50) {
    const int16_t y = scaleCoord(value, h);
    char label[12];
    snprintf(label, sizeof(label), "Y:%d", value);
    const int16_t labelY = (y > h - 22) ? h - 22 : y + 3;
    tft.drawString(label, 3, labelY, 2);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, bg);
  char title[48];
  snprintf(title, sizeof(title), "CYD V23  %s", pageNames[pageIndex]);
  tft.drawCentreString(title, w / 2, h / 2, 4);
  Serial.printf("V23 GRID TEST: rotation=2 geometry=%dx%d page=%u (%s)\\n",
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
