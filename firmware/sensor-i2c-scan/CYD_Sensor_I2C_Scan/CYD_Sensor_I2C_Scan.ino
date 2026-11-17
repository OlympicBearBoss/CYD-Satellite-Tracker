#include <Arduino.h>
#include <Wire.h>

constexpr int I2C_SDA = 27;
constexpr int I2C_SCL = 22;

void scanI2C() {
  Serial.println();
  Serial.println("=== CYD Satellite Tracker: I2C scan ===");
  uint8_t found = 0;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      ++found;
      Serial.printf("Found: 0x%02X", address);
      if (address == 0x53 || address == 0x1D) Serial.print("  (possible ADXL345)");
      if (address == 0x1E) Serial.print("  (HMC5883L address)");
      if (address == 0x0D) Serial.print("  (often QMC5883L, not HMC5883L)");
      Serial.println();
    }
    delay(2);
  }

  if (!found) {
    Serial.println("No I2C devices found. Check 3.3V, GND, SDA=GPIO27, SCL=GPIO22.");
  } else {
    Serial.printf("Total devices: %u\n", found);
    Serial.println("Expected: ADXL345 0x53 (or 0x1D), HMC5883L 0x1E.");
  }
  Serial.println("======================================");
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\nCYD Satellite Tracker - sensor I2C scanner");
  Serial.printf("SDA GPIO%d, SCL GPIO%d\n", I2C_SDA, I2C_SCL);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  scanI2C();
}

void loop() {
  delay(5000);
  scanI2C();
}
