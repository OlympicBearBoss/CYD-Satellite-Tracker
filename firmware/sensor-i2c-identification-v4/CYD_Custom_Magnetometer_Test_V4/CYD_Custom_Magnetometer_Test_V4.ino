#include <Arduino.h>
#include <Wire.h>

// CYD Satellite Tracker - custom QMC5883-compatible module at 0x2C
// V4 is based on the user's confirmed initialization sequence.
// Writes ONLY these three registers, in this exact order:
//   0x0B = 0x01
//   0x0A = 0x01
//   0x09 = 0x1D
// No motors are controlled. ADXL345 at 0x53 is read-only cross-check.

constexpr uint8_t SDA_PIN = 27;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t MAG_ADDR = 0x2C;
constexpr uint8_t ADXL_ADDR = 0x53;
constexpr uint32_t I2C_HZ = 100000;
constexpr uint8_t SAMPLE_COUNT = 8;
constexpr uint16_t SAMPLE_DELAY_MS = 150;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MAG_ADDR);
  Wire.write(reg);
  Wire.write(value);
  uint8_t err = Wire.endTransmission();
  Serial.printf("WRITE 0x%02X <- 0x%02X : %s (I2C error=%u)\n",
                reg, value, err == 0 ? "ACK" : "FAIL", err);
  return err == 0;
}

bool readRegister(uint8_t address, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) return false;
  uint8_t n = Wire.requestFrom((int)address, 1);
  if (n != 1 || !Wire.available()) return false;
  value = (uint8_t)Wire.read();
  return true;
}

bool readMagBlock(uint8_t startReg, uint8_t *data, uint8_t len) {
  Wire.beginTransmission(MAG_ADDR);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t n = Wire.requestFrom((int)MAG_ADDR, (int)len);
  if (n != len) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t i = 0; i < len; ++i) {
    if (!Wire.available()) return false;
    data[i] = (uint8_t)Wire.read();
  }
  return true;
}

void scanBus() {
  Serial.println(F("\nI2C scan:"));
  uint8_t count = 0;
  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  ACK 0x%02X\n", a);
      ++count;
    }
    delay(2);
  }
  Serial.printf("Devices found: %u\n", count);
}

void checkAdxl() {
  uint8_t id = 0;
  Serial.print(F("ADXL345 DEVID: "));
  if (readRegister(ADXL_ADDR, 0x00, id))
    Serial.printf("0x%02X (expected 0xE5)\n", id);
  else
    Serial.println(F("read failed"));
}

bool initializeMagnetometer() {
  Serial.println(F("\nApplying confirmed custom-module startup sequence:"));
  bool a = writeRegister(0x0B, 0x01);
  delay(10);
  bool b = writeRegister(0x0A, 0x01);
  delay(10);
  bool c = writeRegister(0x09, 0x1D);
  delay(20);
  if (!(a && b && c)) {
    Serial.println(F("Initialization sequence reported an I2C write error."));
    return false;
  }
  Serial.println(F("All three initialization writes acknowledged."));
  return true;
}

void dumpRegisters00to0F() {
  Serial.println(F("Registers 0x00..0x0F:"));
  for (uint8_t row = 0; row < 2; ++row) {
    Serial.printf("%02X: ", row * 8);
    for (uint8_t col = 0; col < 8; ++col) {
      uint8_t v = 0;
      uint8_t reg = row * 8 + col;
      if (readRegister(MAG_ADDR, reg, v)) Serial.printf("%02X ", v);
      else Serial.print(F("?? "));
    }
    Serial.println();
  }
}

void capturePosition(const char *label) {
  Serial.printf("\n========== %s ==========\n", label);
  uint8_t good = 0;
  uint8_t ready = 0;
  uint8_t last[6] = {0};

  Serial.println(F("Raw data bytes from 0x00..0x05; status from 0x06."));
  Serial.println(F("X/Y/Z decoding shown as signed little-endian 16-bit for inspection; raw bytes are authoritative."));
  for (uint8_t i = 0; i < SAMPLE_COUNT; ++i) {
    uint8_t status = 0;
    uint8_t d[6] = {0};
    bool okStatus = readRegister(MAG_ADDR, 0x06, status);
    bool okData = readMagBlock(0x00, d, 6);
    if (okStatus && okData) {
      ++good;
      if (status & 0x01) ++ready;
      memcpy(last, d, sizeof(last));
      int16_t x = (int16_t)((uint16_t)d[1] << 8 | d[0]);
      int16_t y = (int16_t)((uint16_t)d[3] << 8 | d[2]);
      int16_t z = (int16_t)((uint16_t)d[5] << 8 | d[4]);
      Serial.printf("#%u ST=0x%02X RAW=%02X %02X %02X %02X %02X %02X  XYZ=%d,%d,%d\n",
                    i + 1, status, d[0], d[1], d[2], d[3], d[4], d[5], x, y, z);
    } else {
      Serial.printf("#%u read failed (status=%s data=%s)\n",
                    i + 1, okStatus ? "OK" : "FAIL", okData ? "OK" : "FAIL");
    }
    delay(SAMPLE_DELAY_MS);
  }
  Serial.printf("Successful samples: %u/%u; status DRDY bit set: %u/%u\n",
                good, SAMPLE_COUNT, ready, SAMPLE_COUNT);
  dumpRegisters00to0F();
  checkAdxl();
}

void waitForEnter(const char *instruction) {
  while (Serial.available()) Serial.read();
  Serial.println();
  Serial.println(instruction);
  Serial.println(F("Press ENTER when ready."));
  while (!Serial.available()) delay(10);
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') break;
  }
  delay(100);
  while (Serial.available()) Serial.read();
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_HZ);

  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F("CYD SATELLITE TRACKER - CUSTOM MAGNETOMETER TEST V4"));
  Serial.println(F("Address 0x2C | user-confirmed startup sequence"));
  Serial.println(F("Writes ONLY 0B=01, then 0A=01, then 09=1D"));
  Serial.println(F("Motors are not controlled."));
  Serial.printf("SDA=GPIO%u SCL=GPIO%u I2C=%lu Hz\n",
                SDA_PIN, SCL_PIN, (unsigned long)I2C_HZ);
  Serial.println(F("================================================"));

  scanBus();
  checkAdxl();

  Wire.beginTransmission(MAG_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println(F("Magnetometer 0x2C absent. Check wiring and power."));
    return;
  }

  if (!initializeMagnetometer()) {
    Serial.println(F("Stopped because one or more startup writes failed."));
    return;
  }

  capturePosition("BASELINE - initial orientation");
  waitForEnter("Rotate ONLY the magnetometer by about 90 degrees. Keep wires connected; avoid magnets and steel.");
  capturePosition("ROTATED ABOUT 90 DEGREES");
  waitForEnter("Rotate another 90 degrees (about 180 degrees from start).");
  capturePosition("ROTATED ABOUT 180 DEGREES");
  waitForEnter("Rotate another 90 degrees (about 270 degrees from start).");
  capturePosition("ROTATED ABOUT 270 DEGREES");

  Serial.println(F("\nV4 sequence complete. Send the full output for analysis."));
}

void loop() {
  delay(1000);
}
