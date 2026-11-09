#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BoardConfig.h"

TFT_eSPI tft;

static uint8_t rotationIndex = 0;
static uint8_t colorIndex = 0;
static uint32_t lastColorChange = 0;
static uint32_t lastRotationChange = 0;

static const uint16_t testColors[] = {
  TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK
};
static const char* colorNames[] = {
  "RED", "GREEN", "BLUE", "WHITE", "BLACK"
};

void drawRotationTest() {
  const uint16_t bg = testColors[colorIndex];
  tft.fillScreen(bg);

  const int16_t w = tft.width();
  const int16_t h = tft.height();
  const int16_t mark = 18;

  // Four differently colored corner markers expose clipping and address-window errors.
  tft.fillRect(0, 0, mark, mark, TFT_RED);
  tft.fillRect(w - mark, 0, mark, mark, TFT_GREEN);
  tft.fillRect(0, h - mark, mark, mark, TFT_BLUE);
  tft.fillRect(w - mark, h - mark, mark, mark, TFT_YELLOW);

  tft.drawRect(1, 1, w - 2, h - 2, TFT_CYAN);

  const uint16_t textColor = (bg == TFT_WHITE || bg == TFT_GREEN || bg == TFT_YELLOW)
                               ? TFT_BLACK : TFT_WHITE;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(textColor, bg);
  char label[48];
  snprintf(label, sizeof(label), "ROT %u  %s", rotationIndex, colorNames[colorIndex]);
  tft.drawCentreString(label, w / 2, h / 2, 4);

  Serial.printf("DISPLAY TEST: rotation=%u geometry=%dx%d color=%s\\n",
                rotationIndex, w, h, colorNames[colorIndex]);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(Pins::TFT_BACKLIGHT, OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT, HIGH);
  tft.init();

  rotationIndex = 0;
  colorIndex = 0;
  tft.setRotation(rotationIndex);
  drawRotationTest();
  lastColorChange = millis();
  lastRotationChange = millis();
}

void loop() {
  const uint32_t now = millis();

  // Change solid fill every 2 seconds to check every pixel and RGB565 color path.
  if (now - lastColorChange >= 2000) {
    lastColorChange = now;
    colorIndex = (colorIndex + 1) % (sizeof(testColors) / sizeof(testColors[0]));
    drawRotationTest();
  }

  // Cycle all four ILI9341 orientations every 10 seconds.
  if (now - lastRotationChange >= 10000) {
    lastRotationChange = now;
    rotationIndex = (rotationIndex + 1) % 4;
    tft.setRotation(rotationIndex);
    drawRotationTest();
  }
}
