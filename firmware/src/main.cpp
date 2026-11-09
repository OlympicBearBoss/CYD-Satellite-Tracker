#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <mbedtls/base64.h>
#include "BearBossLogo.h"
#include <TinyGPSPlus.h>
#include "BoardConfig.h"
#include "SiteLink.h"

TFT_eSPI tft;
bool skipInitialization = false;
bool siteStageStarted = false;
TinyGPSPlus gps;
HardwareSerial GPS(2);

enum class CheckState : uint8_t { WAITING, RUNNING, PASS, WARNING, FAIL, SKIPPED };
struct CheckItem { const char* name; CheckState state; const char* detail; };

CheckItem checks[] = {
  {"ESP32 / запуск", CheckState::WAITING, "Ожидание"},
  {"TFT ILI9341", CheckState::WAITING, "Ожидание"},
  {"I2C", CheckState::WAITING, "Ожидание"},
  {"AS5600 азимут", CheckState::WAITING, "Ожидание"},
  {"PCF8574 / концевики", CheckState::WAITING, "Ожидание"},
  {"Аварийная цепь", CheckState::WAITING, "Ожидание"},
  {"AS5600 угол места", CheckState::WAITING, "Ожидание"},
  {"GPS NEO-6M", CheckState::WAITING, "Ожидание"},
  {"STEP / DIR", CheckState::WAITING, "Ожидание"},
  {"Проверка движения", CheckState::SKIPPED, "Не выполнялась при запуске"}
};
constexpr size_t CHECK_COUNT = sizeof(checks) / sizeof(checks[0]);

bool i2cReady = false;
bool azSensorReady = false;
bool pcfReady = false;
bool estopReleased = false;
bool limitsValid = false;
bool elAnalogPlausible = false;
bool gpsSentenceSeen = false;
uint8_t pcfInputs = 0xFF;
uint16_t elevationAdc = 0;
uint16_t azRaw = 0;

const char* stateGlyph(CheckState s) {
  switch (s) {
    case CheckState::RUNNING: return ">";
    case CheckState::PASS: return "OK";
    case CheckState::WARNING: return "!";
    case CheckState::FAIL: return "X";
    case CheckState::SKIPPED: return "--";
    default: return "...";
  }
}
uint16_t stateColor(CheckState s) {
  switch (s) {
    case CheckState::RUNNING: return TFT_CYAN;
    case CheckState::PASS: return TFT_GREEN;
    case CheckState::WARNING: return TFT_ORANGE;
    case CheckState::FAIL: return TFT_RED;
    case CheckState::SKIPPED: return TFT_DARKGREY;
    default: return TFT_LIGHTGREY;
  }
}

bool touchSkipPressed() {
  uint16_t sx = 0, sy = 0;
  return tft.getTouch(&sx, &sy, 600) &&
         sx >= 8 && sx <= 312 && sy >= 205 && sy <= 238;
}
void drawSkipButton() {
  tft.fillRoundRect(8, 207, 304, 27, 5, TFT_MAROON);
  tft.drawRoundRect(8, 207, 304, 27, 5, TFT_ORANGE);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.drawCentreString("ПРЕРВАТЬ ЗАПУСК → ПОДКЛЮЧЕНИЕ К САЙТУ", 160, 213, 1);
}
// Source JPEG is 160x90 (16:9): enlarge 2x and center on 320x240.
bool splashJpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  static uint16_t scaled[32 * 32];
  if (x < 0 || y < 0 || x + w > 160 || y + h > 90 || w > 16 || h > 16) return false;
  for (uint16_t row = 0; row < h; ++row) {
    for (uint16_t col = 0; col < w; ++col) {
      const uint16_t pixel = bitmap[row * w + col];
      const uint16_t dx = col * 2, dy = row * 2;
      scaled[dy * (w * 2) + dx] = pixel;
      scaled[dy * (w * 2) + dx + 1] = pixel;
      scaled[(dy + 1) * (w * 2) + dx] = pixel;
      scaled[(dy + 1) * (w * 2) + dx + 1] = pixel;
    }
  }
  tft.pushImage(x * 2, 30 + y * 2, w * 2, h * 2, scaled);
  return true;
}

void drawSplash() {
  tft.fillScreen(TFT_BLACK);
  static uint8_t jpg[16000];
  String encoded = FPSTR(BEAR_BOSS_LOGO_BASE64);
  size_t decodedLength = 0;
  int rc = mbedtls_base64_decode(jpg, sizeof(jpg), &decodedLength,
      reinterpret_cast<const unsigned char*>(encoded.c_str()), encoded.length());
  if (rc == 0 && decodedLength > 0) {
    const JRESULT result = TJpgDec.drawJpg(0, 0, jpg, decodedLength);
    if (result != JDR_OK) {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawCentreString("SPLASH JPEG ERROR", 160, 120, 2);
    }
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("SPLASH DATA ERROR", 160, 120, 2);
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("CYD SATELLITE TRACKER", 160, 216, 2);
}
void enterSiteStage() {
  if (siteStageStarted) return;
  siteStageStarted = true;
  SiteLink::begin();
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString("ПОДКЛЮЧЕНИЕ К САЙТУ", 160, 92, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Wi-Fi / привязка устройства", 160, 122, 2);
  tft.drawCentreString("Точка настройки: CYD-Setup", 160, 151, 1);
  tft.drawCentreString("192.168.4.1 | пароль: cydsetup", 160, 170, 1);
}
void pollSkip() {
  if (skipInitialization) return;
  if (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 's' || c == 'S') skipInitialization = true;
  }
  if (touchSkipPressed()) { skipInitialization = true; delay(250); }
  if (skipInitialization) enterSiteStage();
}

void drawScreen(const char* headline = "Проверка системы") {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("CYD SATELLITE TRACKER", 8, 5, 2);
  tft.drawFastHLine(8, 26, 304, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(headline, 8, 32, 2);
  for (size_t i = 0; i < CHECK_COUNT; ++i) {
    const int col = (i >= 5) ? 1 : 0;
    const int row = (int)(i % 5);
    const int x = col ? 164 : 8;
    const int y = 58 + row * 29;
    tft.setTextColor(stateColor(checks[i].state), TFT_BLACK);
    tft.drawString(stateGlyph(checks[i].state), x, y, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(checks[i].name, x + 31, y, 1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(checks[i].detail, x + 31, y + 13, 1);
  }
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("ИНИЦИАЛИЗАЦИЯ: БЕЗ ДВИЖЕНИЯ МОТОРОВ", 8, 196, 1);
  drawSkipButton();
}

void setCheck(size_t i, CheckState state, const char* detail) {
  checks[i].state = state;
  checks[i].detail = detail;
  drawScreen();
  const uint32_t until = millis() + 250;
  while (!skipInitialization && millis() < until) { pollSkip(); delay(10); }
}
void waitBrief() { pollSkip(); delay(80); }

bool readRegister16(uint8_t address, uint8_t reg, uint16_t &value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)address, 2) != 2) return false;
  value = ((uint16_t)Wire.read() << 8) | Wire.read();
  return true;
}
bool devicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}
void initSafeOutputs() {
  pinMode(Pins::AZ_STEP, OUTPUT);
  digitalWrite(Pins::AZ_STEP, LOW);
  pinMode(Pins::EL_STEP, OUTPUT);
  digitalWrite(Pins::EL_STEP, LOW);
  pinMode(Pins::AZ_DIR, OUTPUT);
  digitalWrite(Pins::AZ_DIR, LOW);
  pinMode(Pins::EL_DIR, OUTPUT);
  digitalWrite(Pins::EL_DIR, LOW);
}

void setup() {
  initSafeOutputs();
  Serial.begin(115200);
  delay(50);
  checks[0] = {"ESP32 / запуск", CheckState::PASS, "Запуск выполнен"};
  tft.init();
  tft.setRotation(1);
  pinMode(Pins::TFT_BACKLIGHT, OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT, HIGH);
  tft.setSwapBytes(true);
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(splashJpegOutput);
  drawSplash();
  delay(5000);
  setCheck(1, CheckState::PASS, "Экран 320x240 активен");
  if (skipInitialization) { enterSiteStage(); return; }
  setCheck(2, CheckState::RUNNING, "GPIO27 SDA / GPIO22 SCL");
  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  Wire.setClock(100000);
  i2cReady = devicePresent(Devices::AS5600_ADDRESS) ||
             devicePresent(Devices::PCF8574_ADDRESS);
  setCheck(2, i2cReady ? CheckState::PASS : CheckState::FAIL,
           i2cReady ? "Шина отвечает хотя бы одним устройством" : "Нет ответа I2C");
  if (skipInitialization) { enterSiteStage(); return; }
  setCheck(3, CheckState::RUNNING, "Адрес 0x36, регистр ANGLE");
  azSensorReady = i2cReady && readRegister16(Devices::AS5600_ADDRESS, 0x0E, azRaw);
  setCheck(3, azSensorReady ? CheckState::PASS : CheckState::FAIL,
           azSensorReady ? "AS5600 отвечает" : "Датчик не найден / нет данных");
  if (skipInitialization) { enterSiteStage(); return; }
  setCheck(4, CheckState::RUNNING, "PCF8574, P0..P3");
  pcfReady = devicePresent(Devices::PCF8574_ADDRESS);
  if (pcfReady) {
    Wire.beginTransmission(Devices::PCF8574_ADDRESS);
    Wire.write(0xFF);
    pcfReady = Wire.endTransmission() == 0;
    if (pcfReady && Wire.requestFrom((int)Devices::PCF8574_ADDRESS, 1) == 1)
      pcfInputs = Wire.read();
    else pcfReady = false;
  }
  if (pcfReady) {
    const bool azBoth = ((pcfInputs & 0x03) == 0x03);
    const bool elBoth = ((pcfInputs & 0x0C) == 0x0C);
    limitsValid = !(azBoth || elBoth);
    setCheck(4, limitsValid ? CheckState::PASS : CheckState::FAIL,
             limitsValid ? "Входы прочитаны; активные пределы учтены" :
                           "Оба концевика одной оси разом активны");
  } else {
    limitsValid = false;
    setCheck(4, CheckState::FAIL, "PCF8574 не отвечает");
  }
  if (skipInitialization) { enterSiteStage(); return; }
  setCheck(5, CheckState::RUNNING, "P5 — вспомогательный контакт E-STOP");
  estopReleased = pcfReady && ((pcfInputs & Devices::PCF_ESTOP_BIT) == 0);
  setCheck(5, estopReleased ? CheckState::PASS : CheckState::FAIL,
           estopReleased ? "Цепь замкнута (стоп отпущен)" :
                           "Разомкнута / не подтверждена; движение запрещено");
  if (skipInitialization) { enterSiteStage(); return; }
  setCheck(6, CheckState::RUNNING, "ADC GPIO35, аналоговый OUT");
  analogReadResolution(12);
  elevationAdc = analogRead(Pins::EL_AS5600_ANALOG);
  elAnalogPlausible = elevationAdc > 80 && elevationAdc < 4015;
  setCheck(6, elAnalogPlausible ? CheckState::WARNING : CheckState::FAIL,
           elAnalogPlausible ? "Сигнал есть; угловая калибровка не задана" :
                               "Нет достоверного сигнала ADC");
  if (skipInitialization) { enterSiteStage(); return; }
  setCheck(7, CheckState::RUNNING, "UART2 RX GPIO34, 9600 бод");
  GPS.begin(9600, SERIAL_8N1, Pins::GPS_RX, -1);
  const uint32_t start = millis();
  while (millis() - start < 1200 && !skipInitialization) {
    pollSkip();
    while (GPS.available()) if (gps.encode(GPS.read())) gpsSentenceSeen = true;
    delay(2);
  }
  setCheck(7, gpsSentenceSeen ? CheckState::PASS : CheckState::WARNING,
           gpsSentenceSeen ? "NMEA принимается; GPS fix проверяется отдельно" :
                             "Нет NMEA; проверьте питание и TX->GPIO34");
  if (skipInitialization) { enterSiteStage(); return; }
  setCheck(8, CheckState::RUNNING, "STEP/DIR настроены, ENABLE не включён");
  setCheck(8, CheckState::PASS, "Линии управления подготовлены; силовой тест не выполнен");
  checks[9].state = CheckState::SKIPPED;
  checks[9].detail = "Не запускался автоматически — это безопасно";
  bool anyFail = false, anyWarning = false;
  for (size_t i = 0; i < CHECK_COUNT; ++i) {
    anyFail |= checks[i].state == CheckState::FAIL;
    anyWarning |= checks[i].state == CheckState::WARNING;
  }
  drawScreen(anyFail ? "СИСТЕМА ЗАБЛОКИРОВАНА" :
             anyWarning ? "ГОТОВО С ПРЕДУПРЕЖДЕНИЕМ" : "ДИАГНОСТИКА ЗАВЕРШЕНА");
  enterSiteStage();
  Serial.println("Boot diagnostics complete. Motors remain disabled.");
}
void loop() {
  SiteLink::loop();
  delay(10);
}
