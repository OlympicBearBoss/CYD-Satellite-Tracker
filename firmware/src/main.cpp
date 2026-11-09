#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <TinyGPSPlus.h>
#include "BoardConfig.h"
#include "SiteLink.h"

TFT_eSPI tft;
// Render the complete landscape UI off-screen, then rotate the whole frame.
TFT_eSprite ui(&tft);
constexpr int16_t UI_W = 320;
constexpr int16_t UI_H = 240;
void presentUI() {
  // Standard RGB565 sprite transfer in the controller native landscape mode.
  // No software rotation, raw address-window writes, or pushRotated path.
  if (!ui.created()) {
    Serial.println("DISPLAY ERROR: framebuffer is unavailable");
    return;
  }
  ui.pushSprite(0, 0);
}
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
  ui.fillRoundRect(8, 207, 304, 27, 5, TFT_MAROON);
  ui.drawRoundRect(8, 207, 304, 27, 5, TFT_ORANGE);
  ui.setTextColor(TFT_WHITE, TFT_MAROON);
  ui.drawCentreString("ПРЕРВАТЬ ЗАПУСК → ПОДКЛЮЧЕНИЕ К САЙТУ", 160, 213, 1);
}
// Native TFT splash: no JPEG decoder, compressed image, scaling, or byte swapping.
void drawSplash() {
  ui.fillScreen(TFT_BLACK);
  const int16_t W = UI_W;
  const int16_t H = UI_H;
  if (ui.width() != UI_W || ui.height() != UI_H) {
    Serial.printf("SPLASH TFT GEOMETRY ERROR: %dx%d\\n", W, H);
    ui.setTextDatum(MC_DATUM);
    ui.setTextColor(TFT_RED, TFT_BLACK);
    ui.drawCentreString("DISPLAY GEOMETRY ERROR", W / 2, H / 2 - 10, 2);
    ui.drawCentreString(String(W) + " x " + String(H), W / 2, H / 2 + 14, 2);
    presentUI();
    return;
  }
  for (int i = 0; i < 54; ++i) {
    const int16_t x = (i * 73 + 19) % W;
    const int16_t y = (i * 47 + 11) % H;
    ui.drawPixel(x, y, (i % 4 == 0) ? 0x7DFF : 0x39C7);
  }
  const int cx = 160, cy = 91;
  for (int a = 0; a < 360; a += 2) {
    const float rad = a * 0.0174532925f;
    ui.drawPixel(cx + (int)(112.0f * cosf(rad)),
                  cy + (int)(36.0f * sinf(rad)), 0x4D7F);
  }
  ui.fillCircle(cx, cy, 27, 0x021F);
  ui.drawCircle(cx, cy, 27, TFT_CYAN);
  ui.drawCircle(cx, cy, 22, 0x3D7F);
  ui.fillCircle(cx - 8, cy - 7, 5, 0x7FFF);
  ui.drawLine(cx - 48, cy - 17, cx - 31, cy - 8, TFT_CYAN);
  ui.drawLine(cx + 27, cy + 12, cx + 47, cy + 21, TFT_CYAN);
  ui.fillRect(cx - 55, cy - 23, 17, 12, 0x39E7);
  ui.drawRect(cx - 55, cy - 23, 17, 12, TFT_WHITE);
  ui.fillRect(cx + 38, cy + 15, 17, 12, 0x39E7);
  ui.drawRect(cx + 38, cy + 15, 17, 12, TFT_WHITE);
  ui.setTextDatum(MC_DATUM);
  ui.setTextColor(TFT_WHITE, TFT_BLACK);
  ui.drawCentreString("CYD SATELLITE TRACKER", 160, 151, 4);
  ui.setTextColor(TFT_CYAN, TFT_BLACK);
  ui.drawCentreString("BY OLYMPIC BEAR", 160, 180, 2);
  ui.setTextColor(0x9E7F, TFT_BLACK);
  ui.drawCentreString("VHF 145 MHz  |  ORBITAL TRACKING", 160, 211, 1);
  presentUI();
}
void enterSiteStage() {
  if (siteStageStarted) return;
  siteStageStarted = true;
  SiteLink::begin();
  ui.fillScreen(TFT_BLACK);
  ui.setTextDatum(MC_DATUM);
  ui.setTextColor(TFT_CYAN, TFT_BLACK);
  ui.drawCentreString("ПОДКЛЮЧЕНИЕ К САЙТУ", 160, 92, 2);
  ui.setTextColor(TFT_WHITE, TFT_BLACK);
  ui.drawCentreString("Wi-Fi / привязка устройства", 160, 122, 2);
  ui.drawCentreString("Точка настройки: CYD-Setup", 160, 151, 1);
  ui.drawCentreString("192.168.4.1 | пароль: cydsetup", 160, 170, 1);
  presentUI();
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
  ui.fillScreen(TFT_BLACK);
  ui.setTextDatum(TL_DATUM);
  ui.setTextColor(TFT_CYAN, TFT_BLACK);
  ui.drawString("CYD SATELLITE TRACKER", 8, 5, 2);
  ui.drawFastHLine(8, 26, 304, TFT_DARKGREY);
  ui.setTextColor(TFT_WHITE, TFT_BLACK);
  ui.drawString(headline, 8, 32, 2);
  for (size_t i = 0; i < CHECK_COUNT; ++i) {
    const int col = (i >= 5) ? 1 : 0;
    const int row = (int)(i % 5);
    const int x = col ? 164 : 8;
    const int y = 58 + row * 29;
    ui.setTextColor(stateColor(checks[i].state), TFT_BLACK);
    ui.drawString(stateGlyph(checks[i].state), x, y, 2);
    ui.setTextColor(TFT_WHITE, TFT_BLACK);
    ui.drawString(checks[i].name, x + 31, y, 1);
    ui.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    ui.drawString(checks[i].detail, x + 31, y + 13, 1);
  }
  ui.setTextColor(TFT_CYAN, TFT_BLACK);
  ui.drawString("ИНИЦИАЛИЗАЦИЯ: БЕЗ ДВИЖЕНИЯ МОТОРОВ", 8, 196, 1);
  drawSkipButton();
  presentUI();
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

void runDisplayDiagnostic() {
  // Use the controller native landscape mode and transfer the sprite directly.
  tft.setRotation(1);
  Serial.printf("TFT GEOMETRY: %dx%d; UI GEOMETRY: %dx%d; direct sprite output\\n",
                tft.width(), tft.height(), UI_W, UI_H);
}

void setup() {
  initSafeOutputs();
  Serial.begin(115200);
  delay(50);
  checks[0] = {"ESP32 / запуск", CheckState::PASS, "Запуск выполнен"};
  tft.init();
  pinMode(Pins::TFT_BACKLIGHT, OUTPUT);
  digitalWrite(Pins::TFT_BACKLIGHT, HIGH);
  runDisplayDiagnostic();
  // Keep the framebuffer in RGB565 for the standard pushSprite() transfer.
  Serial.printf("Free heap before framebuffer: %u bytes\\n", ESP.getFreeHeap());
  ui.setColorDepth(16);
  if (!ui.createSprite(UI_W, UI_H)) {
    Serial.println("FATAL: unable to allocate 320x240 UI framebuffer");
    while (true) delay(1000);
  }
  ui.setTextWrap(false);
  Serial.printf("RGB565 framebuffer allocated: %dx%d, free heap: %u bytes\\n",
                ui.width(), ui.height(), ESP.getFreeHeap());
  Serial.println("UI framebuffer allocated; RGB565 full-frame rotation enabled.");
  drawSplash();
  delay(5000);
  const bool landscapeReady = (ui.width() == UI_W && ui.height() == UI_H &&
                               tft.width() == 320 && tft.height() == 240);
  char geometryDetail[32];
  if (landscapeReady) {
    snprintf(geometryDetail, sizeof(geometryDetail), "Экран 320x240 активен");
  } else {
    snprintf(geometryDetail, sizeof(geometryDetail), "Ошибка UI/панели");
  }
  setCheck(1, landscapeReady ? CheckState::PASS : CheckState::FAIL, geometryDetail);
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