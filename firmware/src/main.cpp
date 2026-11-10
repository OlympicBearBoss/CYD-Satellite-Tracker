#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BoardConfig.h"

TFT_eSPI tft;

static constexpr uint8_t DISPLAY_ROTATION = 1; // V27 rotation 3 turned by another 180 degrees
static constexpr uint32_t SPLASH_PAGE_MS = 6000;
static constexpr uint32_t PATTERN_PAGE_MS = 10000;
static bool showingSplash = true;
static uint32_t pageStartedAt = 0;

static void printGeometry(const char* stage) {
  Serial.printf(
    "DISPLAY DIAG V28 | %s | rotation=%u | logical=%dx%d | SPI write=%u Hz | SPI read=%u Hz\n",
    stage, tft.getRotation(), tft.width(), tft.height(),
    (unsigned)SPI_FREQUENCY, (unsigned)SPI_READ_FREQUENCY
  );
}

static void drawSatellite(int16_t cx, int16_t cy) {
  // Main satellite body and its two solar panels.
  tft.fillRoundRect(cx - 8, cy - 6, 16, 12, 3, TFT_LIGHTGREY);
  tft.drawRoundRect(cx - 8, cy - 6, 16, 12, 3, TFT_WHITE);
  tft.fillRect(cx - 31, cy - 8, 19, 16, TFT_BLUE);
  tft.drawRect(cx - 31, cy - 8, 19, 16, TFT_CYAN);
  tft.fillRect(cx + 12, cy - 8, 19, 16, TFT_BLUE);
  tft.drawRect(cx + 12, cy - 8, 19, 16, TFT_CYAN);
  for (int16_t x = cx - 26; x <= cx - 16; x += 5) tft.drawFastVLine(x, cy - 7, 14, TFT_CYAN);
  for (int16_t x = cx + 17; x <= cx + 27; x += 5) tft.drawFastVLine(x, cy - 7, 14, TFT_CYAN);
  tft.drawLine(cx, cy - 6, cx + 5, cy - 15, TFT_WHITE);
  tft.fillCircle(cx + 5, cy - 16, 2, TFT_YELLOW);
}

static void showSplash() {
  const int16_t w = tft.width();
  const int16_t h = tft.height();
  tft.fillScreen(0x020A1B);

  // Star field.
  const int16_t sx[] = {12, 34, 61, 87, 111, 146, 174, 203, 229, 257, 291, 307, 43, 132, 238, 279};
  const int16_t sy[] = {18, 51, 29, 76, 14, 42, 17, 64, 31, 19, 54, 91, 113, 96, 120, 147};
  for (uint8_t i = 0; i < 16; ++i) {
    if (sx[i] < w && sy[i] < h) tft.fillCircle(sx[i], sy[i], (i % 4 == 0) ? 2 : 1, TFT_WHITE);
  }

  // Orbital path behind the Earth and satellite.
  tft.drawEllipse(w / 2, h / 2 + 15, 104, 43, 0x1C6B9A);
  tft.drawEllipse(w / 2, h / 2 + 15, 108, 47, 0x123B65);

  // Stylized Earth with blue limb and green continents.
  const int16_t ex = w / 2;
  const int16_t ey = h / 2 + 23;
  tft.fillCircle(ex, ey, 48, 0x0878B8);
  tft.drawCircle(ex, ey, 49, TFT_CYAN);
  tft.fillCircle(ex - 13, ey - 12, 19, 0x168B70);
  tft.fillCircle(ex + 17, ey + 9, 15, 0x168B70);
  tft.fillCircle(ex - 2, ey + 25, 9, 0x168B70);
  tft.fillCircle(ex + 3, ey - 2, 42, 0x09699F);

  drawSatellite(ex + 84, ey - 28);

  // Header and project name.
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, 0x020A1B);
  tft.drawString("CYD", w / 2, 24, 4);
  tft.setTextColor(TFT_WHITE, 0x020A1B);
  tft.drawString("SATELLITE TRACKER", w / 2, 49, 2);

  tft.setTextColor(0x9CCDE8, 0x020A1B);
  tft.drawString("AUTOMATIC ANTENNA POINTING", w / 2, h - 18, 1);
  tft.drawFastHLine(24, h - 34, w - 48, 0x1C6B9A);
  printGeometry("SPLASH");
}

static void showAddressPattern() {
  const int16_t w = tft.width();
  const int16_t h = tft.height();
  const uint16_t colors[] = {
    TFT_RED, TFT_GREEN, TFT_BLUE, TFT_CYAN,
    TFT_MAGENTA, TFT_YELLOW, TFT_ORANGE, TFT_WHITE
  };
  const uint8_t count = sizeof(colors) / sizeof(colors[0]);

  tft.fillScreen(TFT_BLACK);
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t x0 = (int32_t)i * w / count;
    const int16_t x1 = (int32_t)(i + 1) * w / count;
    tft.fillRect(x0, 0, x1 - x0, h, colors[i]);
    tft.drawFastVLine(x0, 0, h, TFT_BLACK);
  }

  tft.fillRect(0, 0, w, 5, TFT_WHITE);
  tft.fillRect(0, h - 5, w, 5, TFT_BLACK);
  tft.drawFastVLine(0, 0, h, TFT_WHITE);
  tft.drawFastVLine(1, 0, h, TFT_WHITE);
  tft.drawFastVLine(w - 2, 0, h, TFT_WHITE);
  tft.drawFastVLine(w - 1, 0, h, TFT_WHITE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("V28 ADDRESS TEST", 6, 8, 2);

  tft.setTextDatum(BL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  char geometry[32];
  snprintf(geometry, sizeof(geometry), "%d x %d  ROT %u", w, h, tft.getRotation());
  tft.drawString(geometry, 6, h - 8, 2);
  printGeometry("ADDRESS_PATTERN");
}

void setup() {
  Serial.begin(115200);
  delay(150);
  pinMode(Pins::TFT_BACKLIGHT, OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT, HIGH);

  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  tft.fillScreen(TFT_BLACK);
  printGeometry("BOOT");

  showSplash();
  showingSplash = true;
  pageStartedAt = millis();
}

void loop() {
  const uint32_t now = millis();
  if (showingSplash && now - pageStartedAt >= SPLASH_PAGE_MS) {
    showAddressPattern();
    showingSplash = false;
    pageStartedAt = now;
  } else if (!showingSplash && now - pageStartedAt >= PATTERN_PAGE_MS) {
    showSplash();
    showingSplash = true;
    pageStartedAt = now;
  }
}
