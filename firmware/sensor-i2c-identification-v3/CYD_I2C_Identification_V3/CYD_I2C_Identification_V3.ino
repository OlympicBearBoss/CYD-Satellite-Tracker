#include <Arduino.h>
#include <Wire.h>

// CYD Satellite Tracker - I2C Identification V3
// READ-ONLY diagnostic. The only transmitted byte to 0x2C is a register
// pointer; no sensor configuration/data registers are written.
// Motors are never controlled.
//
// This test captures the same register map at four physical orientations.
// It can show whether any readable register changes with module rotation,
// but it cannot identify a chip solely from an address or register dump.

constexpr uint8_t SDA_PIN = 27;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t TARGET = 0x2C;
constexpr uint8_t ADXL = 0x53;
constexpr uint32_t I2C_HZ = 100000;
constexpr uint8_t REG_COUNT = 0x80;
constexpr uint8_t SAMPLES_PER_POSITION = 5;
constexpr uint16_t SAMPLE_GAP_MS = 250;

uint8_t baseline[REG_COUNT];
bool baselineValid[REG_COUNT];

bool readReg(uint8_t address, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(address);
  Wire.write(reg);                    // register pointer only
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) return false;
  uint8_t received = Wire.requestFrom((int)address, 1);
  if (received != 1 || !Wire.available()) return false;
  value = (uint8_t)Wire.read();
  return true;
}

bool targetPresent() {
  Wire.beginTransmission(TARGET);
  return Wire.endTransmission() == 0;
}

void scanBus() {
  Serial.println(F("\nI2C scan (0x08..0x77):"));
  uint8_t count = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  ACK 0x%02X\n", address);
      ++count;
    }
    delay(2);
  }
  Serial.printf("Devices found: %u\n", count);
}

void checkAdxl() {
  uint8_t id = 0;
  Serial.print(F("ADXL345 0x53 DEVID: "));
  if (readReg(ADXL, 0x00, id)) {
    Serial.printf("0x%02X (expected 0xE5)\n", id);
  } else {
    Serial.println(F("read failed"));
  }
}

void readMap(uint8_t *values, bool *valid) {
  for (uint8_t reg = 0; reg < REG_COUNT; ++reg) {
    valid[reg] = readReg(TARGET, reg, values[reg]);
    delay(1);
  }
}

void printMap(const uint8_t *values, const bool *valid) {
  Serial.println(F("Register map 0x00..0x7F:"));
  for (uint8_t reg = 0; reg < REG_COUNT; ++reg) {
    if ((reg & 0x0F) == 0) Serial.printf("%02X: ", reg);
    if (valid[reg]) Serial.printf("%02X ", values[reg]);
    else Serial.print(F("?? "));
    if ((reg & 0x0F) == 0x0F) Serial.println();
  }
}

void compareMap(const uint8_t *values, const bool *valid) {
  if (!baselineValid[0]) {
    Serial.println(F("No baseline available; comparison skipped."));
    return;
  }
  uint8_t changed = 0;
  uint8_t failed = 0;
  Serial.println(F("Differences from BASELINE (register: baseline -> current):"));
  for (uint8_t reg = 0; reg < REG_COUNT; ++reg) {
    if (!valid[reg] || !baselineValid[reg]) {
      ++failed;
      continue;
    }
    if (values[reg] != baseline[reg]) {
      Serial.printf("  0x%02X: 0x%02X -> 0x%02X\n",
                    reg, baseline[reg], values[reg]);
      ++changed;
    }
  }
  Serial.printf("Changed registers: %u; read failures: %u\n", changed, failed);
  if (changed == 0) {
    Serial.println(F("No register value changed relative to baseline."));
  }
}

void capturePosition(const char *label, bool isBaseline) {
  Serial.printf("\n========== POSITION: %s ==========\n", label);
  if (!targetPresent()) {
    Serial.println(F("Target 0x2C is not responding."));
    return;
  }

  uint8_t values[REG_COUNT];
  bool valid[REG_COUNT];
  uint8_t changedAcrossSamples = 0;
  uint8_t first[REG_COUNT];
  bool firstValid[REG_COUNT];

  for (uint8_t sample = 0; sample < SAMPLES_PER_POSITION; ++sample) {
    readMap(values, valid);
    if (sample == 0) {
      memcpy(first, values, sizeof(first));
      memcpy(firstValid, valid, sizeof(firstValid));
    } else {
      for (uint8_t reg = 0; reg < REG_COUNT; ++reg) {
        if (valid[reg] && firstValid[reg] && values[reg] != first[reg]) {
          ++changedAcrossSamples;
        }
      }
    }
    delay(SAMPLE_GAP_MS);
  }

  Serial.printf("Captured %u complete register-map samples.\n", SAMPLES_PER_POSITION);
  Serial.printf("Within-position differing register observations: %u\n",
                changedAcrossSamples);
  printMap(first, firstValid);

  if (isBaseline) {
    memcpy(baseline, first, sizeof(baseline));
    memcpy(baselineValid, firstValid, sizeof(baselineValid));
    Serial.println(F("Baseline stored."));
  } else {
    compareMap(first, firstValid);
  }
  checkAdxl();
}

void waitForEnter(const char *instruction) {
  while (Serial.available()) Serial.read();
  Serial.println();
  Serial.println(instruction);
  Serial.println(F("When ready, press ENTER in Serial Monitor."));
  while (!Serial.available()) delay(10);
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') break;
  }
  delay(150);
  while (Serial.available()) Serial.read();
}

void runSequence() {
  memset(baseline, 0, sizeof(baseline));
  memset(baselineValid, 0, sizeof(baselineValid));

  scanBus();
  checkAdxl();

  if (!targetPresent()) {
    Serial.println(F("Target 0x2C absent. Check power and wiring."));
    return;
  }

  capturePosition("BASELINE - start orientation", true);

  waitForEnter("Rotate ONLY the GY-273 module by about 90 degrees. Keep wires connected and avoid nearby magnets/steel.");
  capturePosition("ROTATED ABOUT 90 DEGREES", false);

  waitForEnter("Rotate the same module another 90 degrees (about 180 degrees from start).");
  capturePosition("ROTATED ABOUT 180 DEGREES", false);

  waitForEnter("Rotate the same module another 90 degrees (about 270 degrees from start).");
  capturePosition("ROTATED ABOUT 270 DEGREES", false);

  Serial.println(F("\nV3 sequence complete. No configuration registers were written."));
  Serial.println(F("Send the complete Serial Monitor output for analysis."));
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_HZ);

  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F("CYD SATELLITE TRACKER - I2C IDENTIFICATION V3"));
  Serial.println(F("READ-ONLY | MOTORS NOT CONTROLLED"));
  Serial.println(F("Target 0x2C; ADXL345 cross-check 0x53"));
  Serial.printf("SDA=GPIO%u SCL=GPIO%u I2C=%lu Hz\n",
                SDA_PIN, SCL_PIN, (unsigned long)I2C_HZ);
  Serial.println(F("Register-pointer selection only; no sensor configuration writes."));
  Serial.println(F("Serial Monitor: 115200 baud, line ending Newline or Both NL & CR."));
  Serial.println(F("================================================"));
  runSequence();
}

void loop() {
  delay(1000);
}
