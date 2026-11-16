#include <Arduino.h>

// Isolated serial-only pulse test. No enable GPIO and no automatic motion.
namespace Pins {
constexpr uint8_t AZ_STEP = 4;
constexpr uint8_t AZ_DIR  = 16;
constexpr uint8_t EL_STEP = 17;
constexpr uint8_t EL_DIR  = 26;
}
static constexpr uint8_t TEST_STEPS = 8;
static constexpr uint16_t STEP_HALF_PERIOD_US = 2000; // 250 steps/s
static bool busy = false;

static void pulseAxis(const char* axis, uint8_t stepPin, uint8_t dirPin, bool direction) {
  if (busy) return;
  busy = true;
  digitalWrite(stepPin, LOW);
  digitalWrite(dirPin, direction ? HIGH : LOW);
  delayMicroseconds(20);
  Serial.printf("TEST %s %s: %u pulses, 250 steps/s\n", axis, direction ? "+" : "-", TEST_STEPS);
  for (uint8_t i = 0; i < TEST_STEPS; ++i) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(STEP_HALF_PERIOD_US);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(STEP_HALF_PERIOD_US);
  }
  digitalWrite(stepPin, LOW);
  Serial.println("DONE; STEP is LOW.");
  busy = false;
}
void setup() {
  pinMode(Pins::AZ_STEP, OUTPUT); pinMode(Pins::AZ_DIR, OUTPUT);
  pinMode(Pins::EL_STEP, OUTPUT); pinMode(Pins::EL_DIR, OUTPUT);
  digitalWrite(Pins::AZ_STEP, LOW); digitalWrite(Pins::AZ_DIR, LOW);
  digitalWrite(Pins::EL_STEP, LOW); digitalWrite(Pins::EL_DIR, LOW);
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("CYD SATELLITE TRACKER - MOTOR PULSE TEST");
  Serial.println("SAFE IDLE: no automatic motion; STEP pins LOW; no EN control.");
  Serial.println("Commands: AZ+, AZ-, EL+, EL-, HELP");
  Serial.println("Each command sends exactly 8 pulses at 250 steps/s.");
}
void loop() {
  if (!Serial.available()) return;
  String command = Serial.readStringUntil('\n');
  command.trim(); command.toUpperCase();
  if (command == "AZ+") pulseAxis("AZ", Pins::AZ_STEP, Pins::AZ_DIR, true);
  else if (command == "AZ-") pulseAxis("AZ", Pins::AZ_STEP, Pins::AZ_DIR, false);
  else if (command == "EL+") pulseAxis("EL", Pins::EL_STEP, Pins::EL_DIR, true);
  else if (command == "EL-") pulseAxis("EL", Pins::EL_STEP, Pins::EL_DIR, false);
  else if (command == "HELP" || command.length() == 0) Serial.println("Commands: AZ+, AZ-, EL+, EL-. One command = 8 pulses.");
  else Serial.println("Unknown command. Type HELP.");
}
