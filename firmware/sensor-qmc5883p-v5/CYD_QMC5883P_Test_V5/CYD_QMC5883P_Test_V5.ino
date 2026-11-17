#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_QMC5883P.h>

// CYD Satellite Tracker - QMC5883P validation V5
// Uses the official Adafruit_QMC5883P driver for CHIP ID, data and DRDY.
// Preserves the user's exact startup write order:
//   0x0B <- 0x01, then 0x0A <- 0x01, then 0x09 <- 0x1D.
// No motor outputs are configured or controlled.
// Note: 0x09 is the QMC5883P STATUS register. The final user-requested
// write is retained for compatibility/diagnostics; its readback is logged.

constexpr uint8_t SDA_PIN = 27;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t MAG_ADDR = 0x2C;
constexpr uint8_t ADXL_ADDR = 0x53;
constexpr uint32_t I2C_HZ = 100000;
constexpr uint8_t SAMPLE_COUNT = 8;
constexpr uint16_t SAMPLE_DELAY_MS = 150;

Adafruit_QMC5883P qmc;
bool libraryReady = false;

bool writeMag(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MAG_ADDR);
  Wire.write(reg);
  Wire.write(value);
  uint8_t err = Wire.endTransmission();
  Serial.printf("WRITE 0x%02X <- 0x%02X : %s (I2C error=%u)\n",
                reg, value, err == 0 ? "ACK" : "FAIL", err);
  return err == 0;
}

bool readReg(uint8_t address, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)address, 1) != 1 || !Wire.available()) return false;
  value = (uint8_t)Wire.read();
  return true;
}

bool readMagBlock(uint8_t startReg, uint8_t *data, uint8_t len) {
  Wire.beginTransmission(MAG_ADDR);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MAG_ADDR, (int)len) != len) {
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
  Serial.println("\nI2C scan:");
  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf("  ACK 0x%02X\n", a);
    delay(2);
  }
}

void checkAdxl() {
  uint8_t id = 0;
  Serial.print("ADXL345 DEVID: ");
  if (readReg(ADXL_ADDR, 0x00, id)) Serial.printf("0x%02X (expected 0xE5)\n", id);
  else Serial.println("read failed");
}

void dumpQmcRegisters() {
  uint8_t chip = 0, status = 0, ctrl1 = 0, ctrl2 = 0;
  bool a = readReg(MAG_ADDR, 0x00, chip);
  bool b = readReg(MAG_ADDR, 0x09, status);
  bool c = readReg(MAG_ADDR, 0x0A, ctrl1);
  bool d = readReg(MAG_ADDR, 0x0B, ctrl2);
  Serial.printf("QMC registers: CHIP_ID=%s0x%02X STATUS=%s0x%02X CTRL1=%s0x%02X CTRL2=%s0x%02X\n",
    a ? "" : "READ_FAIL/", chip, b ? "" : "READ_FAIL/", status,
    c ? "" : "READ_FAIL/", ctrl1, d ? "" : "READ_FAIL/", ctrl2);
  Serial.printf("STATUS bits: DRDY(bit0)=%u OVFL(bit1)=%u\n",
    (status & 0x01) ? 1 : 0, (status & 0x02) ? 1 : 0);
}

bool initializeWithUserSequence() {
  Serial.println("\nApplying the user's confirmed startup sequence, unchanged:");
  bool a = writeMag(0x0B, 0x01);
  delay(10);
  bool b = writeMag(0x0A, 0x01);
  delay(10);
  bool c = writeMag(0x09, 0x1D);
  delay(20);
  if (!(a && b && c)) {
    Serial.println("ERROR: one or more startup writes failed.");
    return false;
  }
  Serial.println("All three writes acknowledged.");
  return true;
}

void capturePosition(const char *label) {
  Serial.printf("\n========== %s ==========\n", label);
  Serial.println("Data is read using Adafruit_QMC5883P::getRawMagnetic().");
  Serial.println("XYZ byte order per driver: X=01/02, Y=03/04, Z=05/06 (LSB first).");

  uint8_t success = 0, readyCount = 0, overflowCount = 0;
  for (uint8_t i = 0; i < SAMPLE_COUNT; ++i) {
    bool ready = qmc.isDataReady();
    bool overflow = qmc.isOverflow();
    int16_t x = 0, y = 0, z = 0;
    bool ok = qmc.getRawMagnetic(&x, &y, &z);
    uint8_t status = 0;
    bool statusOk = readReg(MAG_ADDR, 0x09, status);
    uint8_t raw[6] = {0};
    bool rawOk = readMagBlock(0x01, raw, 6);

    if (ready) ++readyCount;
    if (overflow) ++overflowCount;
    if (ok) ++success;

    Serial.printf("#%u READY=%u OVERFLOW=%u READ=%s STATUS=%s0x%02X ",
      i + 1, ready ? 1 : 0, overflow ? 1 : 0,
      ok ? "OK" : "FAIL", statusOk ? "" : "READ_FAIL/", status);
    if (ok) Serial.printf("XYZ=%d,%d,%d ", x, y, z);
    if (rawOk) Serial.printf("RAW(01..06)=%02X %02X %02X %02X %02X %02X",
      raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
    else Serial.print("RAW=READ_FAIL");
    Serial.println();
    delay(SAMPLE_DELAY_MS);
  }
  Serial.printf("Summary: driver reads %u/%u; DRDY=%u/%u; overflow=%u/%u\n",
    success, SAMPLE_COUNT, readyCount, SAMPLE_COUNT, overflowCount, SAMPLE_COUNT);
  dumpQmcRegisters();
  checkAdxl();
}

void waitForEnter(const char *instruction) {
  while (Serial.available()) Serial.read();
  Serial.println();
  Serial.println(instruction);
  Serial.println("Press ENTER once when ready (do not press RESET).");
  while (!Serial.available()) delay(10);
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') break;
  }
  delay(80);
  while (Serial.available()) Serial.read();
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_HZ);

  Serial.println("\n================================================");
  Serial.println("CYD SATELLITE TRACKER - QMC5883P TEST V5");
  Serial.println("Adafruit_QMC5883P driver | I2C address 0x2C");
  Serial.println("User startup writes: 0B=01 -> 0A=01 -> 09=1D");
  Serial.println("No motors are controlled.");
  Serial.printf("SDA=GPIO%u SCL=GPIO%u I2C=%lu Hz\n",
    SDA_PIN, SCL_PIN, (unsigned long)I2C_HZ);
  Serial.println("================================================");

  scanBus();
  checkAdxl();

  if (!initializeWithUserSequence()) return;

  Serial.println("\nStarting official Adafruit_QMC5883P driver...");
  libraryReady = qmc.begin(MAG_ADDR, &Wire);
  Serial.printf("qmc.begin(0x2C): %s\n", libraryReady ? "SUCCESS" : "FAILED");
  if (!libraryReady) {
    Serial.println("Driver did not detect CHIP_ID 0x80. No further tests.");
    return;
  }

  uint8_t chip = 0;
  if (readReg(MAG_ADDR, 0x00, chip))
    Serial.printf("CHIP_ID register 0x00 = 0x%02X (QMC5883P expected 0x80)\n", chip);
  dumpQmcRegisters();

  capturePosition("BASELINE - initial orientation");
  waitForEnter("Rotate ONLY the magnetometer about 90 degrees. Keep wires connected; avoid magnets and steel.");
  capturePosition("ROTATED ABOUT 90 DEGREES");
  waitForEnter("Rotate another 90 degrees (about 180 degrees from start).");
  capturePosition("ROTATED ABOUT 180 DEGREES");
  waitForEnter("Rotate another 90 degrees (about 270 degrees from start).");
  capturePosition("ROTATED ABOUT 270 DEGREES");

  Serial.println("\nV5 sequence complete. Send the full Serial output for analysis.");
}

void loop() {
  delay(1000);
}
