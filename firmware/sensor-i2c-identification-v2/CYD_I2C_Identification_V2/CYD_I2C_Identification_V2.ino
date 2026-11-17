#include <Arduino.h>
#include <Wire.h>

// CYD Satellite Tracker - I2C identification probe v2
// READ-ONLY: no sensor configuration/data registers are written.
// Motors are never controlled. Register-pointer selection is the only
// transmitted byte before a read.
constexpr uint8_t SDA_PIN = 27;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t TARGET = 0x2C;
constexpr uint8_t ADXL = 0x53;
constexpr uint32_t I2C_HZ = 100000;

bool readReg(uint8_t address, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) return false;
  if (Wire.requestFrom((int)address, 1) != 1) return false;
  value = (uint8_t)Wire.read();
  return true;
}

void scanBus() {
  Serial.println("\nI2C address scan (0x08..0x77):");
  uint8_t count = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  ACK 0x%02X\n", address);
      ++count;
    }
    delay(2);
  }
  Serial.printf("I2C devices found: %u\n", count);
}

void dumpTarget() {
  Serial.println("\nFull register-pointer readout: 0x2C, registers 0x00..0x7F");
  Serial.println("A pair of question marks means that the read failed.");
  for (uint16_t reg = 0; reg <= 0x7F; ++reg) {
    if ((reg & 0x0F) == 0) Serial.printf("\n%02X: ", (unsigned)reg);
    uint8_t value = 0;
    if (readReg(TARGET, (uint8_t)reg, value)) Serial.printf("%02X ", value);
    else Serial.print("?? ");
    delay(2);
  }
  Serial.println("\n");
}

void repeatedSamples() {
  Serial.println("Repeated reads 0x00..0x1F (4 samples, 1 second apart):");
  for (uint8_t sample = 1; sample <= 4; ++sample) {
    Serial.printf("Sample %u: ", sample);
    for (uint8_t reg = 0; reg <= 0x1F; ++reg) {
      uint8_t value = 0;
      if (readReg(TARGET, reg, value)) Serial.printf("%02X ", value);
      else Serial.print("?? ");
    }
    Serial.println();
    if (sample != 4) delay(1000);
  }
}

void checkAdxl() {
  uint8_t id = 0;
  Serial.println("\nADXL345 cross-check at 0x53:");
  if (readReg(ADXL, 0x00, id)) {
    Serial.printf("DEVID register = 0x%02X; expected 0xE5\n", id);
  } else {
    Serial.println("Could not read ADXL345 DEVID.");
  }
}

void runProbe() {
  scanBus();
  Wire.beginTransmission(TARGET);
  if (Wire.endTransmission() != 0) {
    Serial.println("\nTarget 0x2C does not ACK. Check module power and wiring.");
    checkAdxl();
    return;
  }
  Serial.println("\nTarget 0x2C ACK confirmed.");
  dumpTarget();
  repeatedSamples();
  checkAdxl();
  Serial.println("\nV2 probe cycle complete.");
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_HZ);
  Serial.println("\n================================================");
  Serial.println("CYD SATELLITE TRACKER - I2C IDENTIFICATION V2");
  Serial.println("READ-ONLY TEST | MOTORS NOT CONTROLLED");
  Serial.println("No sensor configuration registers are written.");
  Serial.printf("SDA=GPIO%u SCL=GPIO%u I2C=%lu Hz\n",
                SDA_PIN, SCL_PIN, (unsigned long)I2C_HZ);
  Serial.println("================================================");
  runProbe();
}

void loop() {
  delay(15000);
  Serial.println("\n\n========== REPEATING V2 PROBE ==========");
  runProbe();
}
