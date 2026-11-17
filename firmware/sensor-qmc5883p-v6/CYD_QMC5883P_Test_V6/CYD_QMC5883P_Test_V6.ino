#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_QMC5883P.h>

// CYD Satellite Tracker - QMC5883P continuous rotation test V6
// No Serial input is read: sending ENTER/typing cannot trigger a test-stage reset.
// Confirmed user startup sequence is preserved exactly:
//   0x0B <- 0x01, then 0x0A <- 0x01, then 0x09 <- 0x1D.
// No motor pins are configured or controlled.

constexpr uint8_t SDA_PIN = 27;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t MAG_ADDR = 0x2C;
constexpr uint8_t ADXL_ADDR = 0x53;
constexpr uint32_t I2C_HZ = 100000;
constexpr uint32_t SAMPLE_INTERVAL_MS = 250; // 4 samples/second

Adafruit_QMC5883P qmc;
bool qmcReady = false;
uint32_t lastSample = 0;
uint32_t sampleNumber = 0;

bool writeMag(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MAG_ADDR);
  Wire.write(reg);
  Wire.write(value);
  const uint8_t err = Wire.endTransmission();
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

void scanBus() {
  Serial.println("I2C scan:");
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) Serial.printf("  ACK 0x%02X\n", address);
    delay(2);
  }
}

void initializeSensor() {
  Serial.println("\nApplying the user's confirmed startup sequence, unchanged:");
  const bool a = writeMag(0x0B, 0x01);
  delay(10);
  const bool b = writeMag(0x0A, 0x01);
  delay(10);
  const bool c = writeMag(0x09, 0x1D);
  delay(20);

  if (!(a && b && c)) {
    Serial.println("FATAL: startup sequence write failed. Check wiring/power.");
    return;
  }

  Serial.println("All three startup writes acknowledged.");
  Serial.println("Starting Adafruit_QMC5883P driver...");
  qmcReady = qmc.begin(MAG_ADDR, &Wire);
  Serial.printf("qmc.begin(0x2C): %s\n", qmcReady ? "SUCCESS" : "FAILED");
  if (!qmcReady) return;

  uint8_t chip = 0, status = 0, ctrl1 = 0, ctrl2 = 0;
  const bool chipOk = readReg(MAG_ADDR, 0x00, chip);
  const bool statusOk = readReg(MAG_ADDR, 0x09, status);
  const bool ctrl1Ok = readReg(MAG_ADDR, 0x0A, ctrl1);
  const bool ctrl2Ok = readReg(MAG_ADDR, 0x0B, ctrl2);
  Serial.printf("REGISTERS CHIP_ID=%s0x%02X STATUS=%s0x%02X CTRL1=%s0x%02X CTRL2=%s0x%02X\n",
    chipOk ? "" : "READ_FAIL/", chip,
    statusOk ? "" : "READ_FAIL/", status,
    ctrl1Ok ? "" : "READ_FAIL/", ctrl1,
    ctrl2Ok ? "" : "READ_FAIL/", ctrl2);
  Serial.println("Continuous stream starts below. Do not send commands or press RESET.");
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_HZ);

  Serial.println();
  Serial.println("================================================");
  Serial.println("CYD SATELLITE TRACKER - QMC5883P TEST V6");
  Serial.println("CONTINUOUS ROTATION TEST - NO ENTER / NO STAGES");
  Serial.println("Adafruit_QMC5883P | I2C address 0x2C");
  Serial.println("Startup writes: 0B=01 -> 0A=01 -> 09=1D");
  Serial.println("SDA=GPIO27 SCL=GPIO22 I2C=100000 Hz");
  Serial.println("No motors are controlled.");
  Serial.println("================================================");

  scanBus();

  uint8_t adxlId = 0;
  if (readReg(ADXL_ADDR, 0x00, adxlId))
    Serial.printf("ADXL345 DEVID=0x%02X (expected 0xE5)\n", adxlId);
  else
    Serial.println("ADXL345 DEVID read failed (not required for QMC test).");

  initializeSensor();

  if (qmcReady) {
    Serial.println();
    Serial.println("INSTRUCTIONS:");
    Serial.println("1) Keep the magnetometer connected and still for 5 seconds.");
    Serial.println("2) Rotate ONLY the magnetometer approximately 90 degrees; hold 5 seconds.");
    Serial.println("3) Repeat at approximately 180 and 270 degrees from the start.");
    Serial.println("4) Copy the complete DATA lines from Serial Terminal.");
    Serial.println("CSV: DATA,sample,millis,ready,overflow,read,x,y,z");
    Serial.println("------------------------------------------------");
    Serial.println("DATA,sample,millis,ready,overflow,read,x,y,z");
  }
}

void loop() {
  if (!qmcReady) {
    delay(1000);
    return;
  }

  const uint32_t now = millis();
  if (now - lastSample < SAMPLE_INTERVAL_MS) return;
  lastSample = now;

  const bool ready = qmc.isDataReady();
  const bool overflow = qmc.isOverflow();
  int16_t x = 0, y = 0, z = 0;
  const bool ok = qmc.getRawMagnetic(&x, &y, &z);

  ++sampleNumber;
  Serial.printf("DATA,%lu,%lu,%u,%u,%u,%d,%d,%d\n",
    (unsigned long)sampleNumber,
    (unsigned long)now,
    ready ? 1 : 0,
    overflow ? 1 : 0,
    ok ? 1 : 0,
    ok ? x : 0,
    ok ? y : 0,
    ok ? z : 0);
}
