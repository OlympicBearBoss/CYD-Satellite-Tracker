#include <Arduino.h>

namespace Pins {
  constexpr uint8_t AZ_STEP = 4;
  constexpr uint8_t AZ_DIR  = 16;
  constexpr uint8_t EL_STEP = 17;
  constexpr uint8_t EL_DIR  = 26;
}

// 200 full steps/rev * 16 microsteps * 10/360 = 88.89 pulses.
constexpr uint16_t STEPS_10_DEG = 89;
constexpr uint16_t START_HALF_PERIOD_US = 4000;
constexpr uint16_t MIN_HALF_PERIOD_US = 1800;

static bool busy = false;
static char commandBuffer[24];
static size_t commandLength = 0;

static void printReady() {
  Serial.println();
  Serial.println("CYD SATELLITE TRACKER - 10 DEG MOTOR TEST");
  Serial.println("Serial: 115200 baud");
  Serial.println("Commands: AZ+, AZ-, EL+, EL-, HELP");
  Serial.println("Each command: 89 STEP pulses (about 10 deg at 1/16 microstep).");
  Serial.println("No automatic movement. EN is not controlled by firmware.");
}

static void moveAxis(const char* axis, uint8_t stepPin,
                     uint8_t dirPin, bool direction) {
  if (busy) return;
  busy = true;

  digitalWrite(stepPin, LOW);
  digitalWrite(dirPin, direction ? HIGH : LOW);
  delayMicroseconds(20);

  Serial.printf("%s%s: sending %u pulses\n",
                axis, direction ? "+" : "-", STEPS_10_DEG);

  for (uint16_t i = 0; i < STEPS_10_DEG; ++i) {
    const uint16_t ramp = (i < STEPS_10_DEG - 1 - i)
                            ? i : (STEPS_10_DEG - 1 - i);
    int halfPeriod = START_HALF_PERIOD_US - (int)ramp * 50;
    if (halfPeriod < MIN_HALF_PERIOD_US) {
      halfPeriod = MIN_HALF_PERIOD_US;
    }

    digitalWrite(stepPin, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(halfPeriod);
  }

  digitalWrite(stepPin, LOW);
  Serial.println("DONE; STEP is LOW.");
  busy = false;
}

static void handleCommand(char* command) {
  for (char* p = command; *p; ++p) {
    if (*p >= 'a' && *p <= 'z') *p -= ('a' - 'A');
  }

  if (strcmp(command, "AZ+") == 0) {
    moveAxis("AZ", Pins::AZ_STEP, Pins::AZ_DIR, true);
  } else if (strcmp(command, "AZ-") == 0) {
    moveAxis("AZ", Pins::AZ_STEP, Pins::AZ_DIR, false);
  } else if (strcmp(command, "EL+") == 0) {
    moveAxis("EL", Pins::EL_STEP, Pins::EL_DIR, true);
  } else if (strcmp(command, "EL-") == 0) {
    moveAxis("EL", Pins::EL_STEP, Pins::EL_DIR, false);
  } else if (strcmp(command, "HELP") == 0) {
    printReady();
  } else if (command[0] != '\0') {
    Serial.println("Unknown command. Use AZ+, AZ-, EL+, EL-, HELP.");
  }
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
}

void loop() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        handleCommand(commandBuffer);
        commandLength = 0;
      }
    } else if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = c;
    } else {
      commandLength = 0;
      Serial.println("ERROR: command too long; buffer cleared.");
    }
  }
}
