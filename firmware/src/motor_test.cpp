#include <Arduino.h>

// Isolated serial motor test. EN is wired externally; firmware does not drive EN.
namespace Pins {
constexpr uint8_t AZ_STEP = 4;
constexpr uint8_t AZ_DIR  = 16;
constexpr uint8_t EL_STEP = 17;
constexpr uint8_t EL_DIR  = 26;
}

static constexpr uint8_t TEST_STEPS = 32;
static constexpr uint16_t STEP_HALF_PERIOD_US = 2000; // 250 steps/s
static constexpr uint32_t STATUS_INTERVAL_MS = 2000;
static constexpr size_t COMMAND_BUFFER_SIZE = 24;

static bool busy = false;
static uint32_t lastStatusMs = 0;
static char commandBuffer[COMMAND_BUFFER_SIZE];
static size_t commandLength = 0;

static void printReady() {
  Serial.println();
  Serial.println("CYD SATELLITE TRACKER - MOTOR TEST READY");
  Serial.println("Serial: 115200 baud | EN is external (active LOW)");
  Serial.println("Commands: AZ+, AZ-, EL+, EL-, HELP");
  Serial.printf("One command = %u STEP pulses at 250 steps/s.\n", TEST_STEPS);
}

static void pulseAxis(const char* axis, uint8_t stepPin, uint8_t dirPin, bool direction) {
  if (busy) return;
  busy = true;
  digitalWrite(stepPin, LOW);
  digitalWrite(dirPin, direction ? HIGH : LOW);
  delayMicroseconds(20);
  Serial.printf("RX command accepted: %s%s; sending %u pulses\n",
                axis, direction ? "+" : "-", TEST_STEPS);
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

static void handleCommand(char* command) {
  for (char* p = command; *p; ++p) {
    if (*p >= 'a' && *p <= 'z') *p -= ('a' - 'A');
  }
  Serial.printf("Serial RX: [%s]\n", command);

  if (strcmp(command, "AZ+") == 0) pulseAxis("AZ", Pins::AZ_STEP, Pins::AZ_DIR, true);
  else if (strcmp(command, "AZ-") == 0) pulseAxis("AZ", Pins::AZ_STEP, Pins::AZ_DIR, false);
  else if (strcmp(command, "EL+") == 0) pulseAxis("EL", Pins::EL_STEP, Pins::EL_DIR, true);
  else if (strcmp(command, "EL-") == 0) pulseAxis("EL", Pins::EL_STEP, Pins::EL_DIR, false);
  else if (strcmp(command, "HELP") == 0) printReady();
  else if (command[0] != '\0') Serial.println("Unknown command. Send AZ+, AZ-, EL+, EL-, or HELP.");
}

void setup() {
  pinMode(Pins::AZ_STEP, OUTPUT);
  pinMode(Pins::AZ_DIR, OUTPUT);
  pinMode(Pins::EL_STEP, OUTPUT);
  pinMode(Pins::EL_DIR, OUTPUT);
  digitalWrite(Pins::AZ_STEP, LOW);
  digitalWrite(Pins::AZ_DIR, LOW);
  digitalWrite(Pins::EL_STEP, LOW);
  digitalWrite(Pins::EL_DIR, LOW);

  Serial.begin(115200);
  delay(300);
  printReady();
  lastStatusMs = millis();
}

void loop() {
  // Repeat status so opening Serial Monitor after boot still shows that firmware runs.
  if (millis() - lastStatusMs >= STATUS_INTERVAL_MS) {
    Serial.println("READY: waiting for AZ+/AZ-/EL+/EL-/HELP");
    lastStatusMs = millis();
  }

  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        handleCommand(commandBuffer);
        commandLength = 0;
      }
    } else if (commandLength < COMMAND_BUFFER_SIZE - 1) {
      commandBuffer[commandLength++] = c;
    } else {
      commandLength = 0;
      Serial.println("ERROR: command too long; buffer cleared.");
    }
  }
}
