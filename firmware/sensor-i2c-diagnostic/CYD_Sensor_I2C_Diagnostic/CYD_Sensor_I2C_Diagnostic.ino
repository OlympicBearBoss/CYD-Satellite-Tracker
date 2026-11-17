#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t I2C_SDA = 27;
constexpr uint8_t I2C_SCL = 22;
constexpr uint8_t ADDR_HMC = 0x1E;
constexpr uint8_t ADDR_QMC = 0x0D;
constexpr uint8_t ADDR_UNKNOWN = 0x2C;
constexpr uint8_t ADDR_ADXL = 0x53;

bool readRegister(uint8_t address, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) return false;
  if (Wire.requestFrom((int)address, 1) != 1) return false;
  value = Wire.read();
  return true;
}

void dumpRegisters(uint8_t address, uint8_t first, uint8_t last) {
  Serial.printf("Register dump for 0x%02X (0x%02X..0x%02X):\n",
                address, first, last);
  for (uint8_t reg = first; reg <= last; ++reg) {
    uint8_t value = 0;
    Serial.printf("  R[0x%02X] = ", reg);
    if (readRegister(address, reg, value)) Serial.printf("0x%02X\n", value);
    else Serial.println("read failed");
    delay(2);
  }
}

void checkHMC() {
  Serial.println("\n--- HMC5883L check at 0x1E ---");
  uint8_t a, b, c;
  bool okA = readRegister(ADDR_HMC, 0x0A, a);
  bool okB = readRegister(ADDR_HMC, 0x0B, b);
  bool okC = readRegister(ADDR_HMC, 0x0C, c);
  if (okA && okB && okC) {
    Serial.printf("ID registers: %c %c %c (hex %02X %02X %02X)\n",
                  (char)a, (char)b, (char)c, a, b, c);
    if (a == 'H' && b == '4' && c == '3')
      Serial.println("RESULT: HMC5883L-compatible ID detected.");
    else
      Serial.println("RESULT: Device at 0x1E, but ID is not H43.");
  } else {
    Serial.println("No readable device at 0x1E.");
  }
}

void checkQMC() {
  Serial.println("\n--- QMC5883L check at 0x0D ---");
  uint8_t id = 0;
  if (readRegister(ADDR_QMC, 0x0D, id)) {
    Serial.printf("Register 0x0D = 0x%02X\n", id);
    if (id == 0xFF) Serial.println("RESULT: QMC5883L-like chip ID (0xFF).");
    else Serial.println("Device responds at 0x0D; inspect register values below.");
    dumpRegisters(ADDR_QMC, 0x00, 0x0F);
  } else {
    Serial.println("No readable device at 0x0D.");
  }
}

void checkUnknown() {
  Serial.println("\n--- Unknown device check at 0x2C ---");
  uint8_t value = 0;
  if (readRegister(ADDR_UNKNOWN, 0x00, value)) {
    Serial.println("Device responds to register reads at 0x2C.");
    dumpRegisters(ADDR_UNKNOWN, 0x00, 0x0F);
  } else {
    Serial.println("Device ACKs address 0x2C, but register read failed.");
  }
}

void checkADXL() {
  Serial.println("\n--- ADXL345 check at 0x53 ---");
  uint8_t id = 0;
  if (readRegister(ADDR_ADXL, 0x00, id)) {
    Serial.printf("DEVID register 0x00 = 0x%02X (ADXL345 expected 0xE5)\n", id);
  } else {
    Serial.println("Could not read ADXL345 DEVID.");
  }
}

void runDiagnostic() {
  Serial.println("\n==============================================");
  Serial.println(" CYD Satellite Tracker - I2C sensor diagnostic");
  Serial.println(" Read-only test; motors are not controlled.");
  Serial.printf(" SDA=GPIO%u, SCL=GPIO%u\n", I2C_SDA, I2C_SCL);
  Serial.println("==============================================");

  bool seen1E = false, seen0D = false, seen2C = false, seen53 = false;
  uint8_t count = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      ++count;
      Serial.printf("ACK: 0x%02X\n", address);
      if (address == ADDR_HMC) seen1E = true;
      if (address == ADDR_QMC) seen0D = true;
      if (address == ADDR_UNKNOWN) seen2C = true;
      if (address == ADDR_ADXL) seen53 = true;
    }
    delay(2);
  }
  Serial.printf("Total I2C devices: %u\n", count);

  if (seen1E) checkHMC();
  else Serial.println("\nHMC5883L address 0x1E: not found.");

  if (seen0D) checkQMC();
  else Serial.println("\nQMC5883L address 0x0D: not found.");

  if (seen2C) checkUnknown();
  else Serial.println("\nAddress 0x2C: not found.");

  if (seen53) checkADXL();
  else Serial.println("\nADXL345 address 0x53: not found.");

  Serial.println("\nDiagnostic complete. Repeats in 10 seconds.");
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  runDiagnostic();
}

void loop() {
  delay(10000);
  runDiagnostic();
}
