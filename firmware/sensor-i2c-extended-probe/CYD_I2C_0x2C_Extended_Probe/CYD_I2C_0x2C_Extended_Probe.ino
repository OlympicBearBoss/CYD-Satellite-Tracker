#include <Arduino.h>
#include <Wire.h>

// Extended READ-ONLY probe for the unidentified I2C device at 0x2C.
// No sensor configuration registers are written; only register pointers are selected.
constexpr uint8_t SDA_PIN = 27;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t TARGET = 0x2C;
constexpr uint8_t ADXL = 0x53;

bool readReg(uint8_t addr, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);                 // select register pointer only
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 1) != 1) return false;
  value = Wire.read();
  return true;
}

void dumpRange(uint8_t first, uint8_t last) {
  Serial.printf("\nRegister range 0x%02X..0x%02X\n", first, last);
  for (uint16_t r = first; r <= last; ++r) {
    uint8_t v = 0;
    if (readReg(TARGET, (uint8_t)r, v)) {
      if ((r & 7) == 0) Serial.printf("\n%02X: ", (unsigned)r);
      Serial.printf("%02X ", v);
    } else {
      Serial.printf("\n%02X: ?? ", (unsigned)r);
    }
    delay(2);
  }
  Serial.println();
}

void statusSamples() {
  const uint8_t regs[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                          0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F};
  Serial.println("\nRepeated sample of registers 0x00..0x0F (4 samples, 1 s apart):");
  for (uint8_t s = 0; s < 4; ++s) {
    Serial.printf("Sample %u: ", s + 1);
    for (uint8_t i = 0; i < sizeof(regs); ++i) {
      uint8_t v = 0;
      if (readReg(TARGET, regs[i], v)) Serial.printf("%02X ", v);
      else Serial.print("?? ");
    }
    Serial.println();
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Serial.println("\n==============================================");
  Serial.println("CYD Satellite Tracker - I2C 0x2C extended probe");
  Serial.println("READ-ONLY: no motor control; no sensor config writes");
  Serial.printf("SDA=GPIO%u SCL=GPIO%u I2C clock=100kHz\n", SDA_PIN, SCL_PIN);
  Serial.println("==============================================");

  Wire.beginTransmission(TARGET);
  if (Wire.endTransmission() != 0) {
    Serial.println("Device 0x2C did not ACK. Check wiring and power.");
    return;
  }
  Serial.println("Device 0x2C ACK confirmed.");

  uint8_t devid = 0;
  if (readReg(ADXL, 0x00, devid))
    Serial.printf("ADXL345 at 0x53 DEVID=0x%02X (expected E5)\n", devid);

  dumpRange(0x00, 0x3F);
  statusSamples();
  Serial.println("\nProbe complete. Repeats in 15 seconds.");
}

void loop() {
  delay(15000);
  Serial.println("\n--- Repeat read-only probe ---");
  Wire.beginTransmission(TARGET);
  if (Wire.endTransmission() == 0) {
    dumpRange(0x00, 0x3F);
    statusSamples();
  } else {
    Serial.println("Device 0x2C no longer ACKs.");
  }
}
