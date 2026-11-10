#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BoardConfig.h"

TFT_eSPI tft;

static constexpr uint8_t DISPLAY_ROTATION = 2;
static constexpr uint32_t SOLID_PAGE_MS = 2500;
static constexpr uint32_t PATTERN_PAGE_MS = 8000;
static const uint16_t solidColors[] = {
  TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_CYAN, TFT_MAGENTA, TFT_YELLOW
};
static const char* solidNames[] = {
  "BLACK", "RED", "GREEN", "BLUE", "WHITE", "CYAN", "MAGENTA", "YELLOW"
};
static uint8_t solidIndex = 0;
static bool patternShown = false;
static uint32_t pageStartedAt = 0;

static void printGeometry(const char* stage) {
  Serial.printf(
    "DISPLAY DIAG V25 | %s | rotation=%u | logical=%dx%d | SPI write=%u Hz | SPI read=%u Hz\n",
    stage,
    tft.getRotation(),
    tft.width(),
    tft.height(),
    (unsigned)SPI_FREQUENCY,
    (unsigned)SPI_READ_FREQUENCY
  );
}

static void showSolid(uint8_t index) {
  const uint16_t color = solidColors[index];
  tft.fillScreen(color);
  // A tiny black marker at the logical bottom-right verifies the final addressable pixel.
  tft.drawPixel(tft.width() - 1, tft.height() - 1, color == TFT_BLACK ? TFT_WHITE : TFT_BLACK);
  Serial.printf("V25 SOLID %s (%u/%u)\n", solidNames[index],
                index + 1, (unsigned)(sizeof(solidColors) / sizeof(solidColors[0])));
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

  // Eight full-height, uniquely colored columns test the entire logical X range.
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t x0 = (int32_t)i * w / count;
    const int16_t x1 = (int32_t)(i + 1) * w / count;
    tft.fillRect(x0, 0, x1 - x0, h, colors[i]);
    tft.drawFastVLine(x0, 0, h, TFT_BLACK);
  }

  // Distinct top and bottom bands test the full logical Y range.
  tft.fillRect(0, 0, w, 5, TFT_WHITE);
  tft.fillRect(0, h - 5, w, 5, TFT_BLACK);

  // Double edge marks make clipped/shifted address windows obvious.
  tft.drawFastVLine(0, 0, h, TFT_WHITE);
  tft.drawFastVLine(1, 0, h, TFT_WHITE);
  tft.drawFastVLine(w - 2, 0, h, TFT_WHITE);
  tft.drawFastVLine(w - 1, 0, h, TFT_WHITE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("V25 ADDRESS TEST", 6, 8, 2);

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
  solidIndex = 0;
  patternShown = false;
  showSolid(solidIndex);
  pageStartedAt = millis();
}

void loop() {
  const uint32_t now = millis();

  if (!patternShown && now - pageStartedAt >= SOLID_PAGE_MS) {
    ++solidIndex;
    if (solidIndex >= sizeof(solidColors) / sizeof(solidColors[0])) {
      patternShown = true;
      showAddressPattern();
      pageStartedAt = now;
    } else {
      showSolid(solidIndex);
      pageStartedAt = now;
    }
  } else if (patternShown && now - pageStartedAt >= PATTERN_PAGE_MS) {
    patternShown = false;
    solidIndex = 0;
    showSolid(solidIndex);
    pageStartedAt = now;
  }
}
