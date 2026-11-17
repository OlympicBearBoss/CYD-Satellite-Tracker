#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "BoardConfig.h"

// Two AS5600 encoders share no I2C bus with each other:
// AZ is on I2C0 (Wire), EL is on I2C1 (ElevationWire).
// AZ may coexist with QMC5883P/ADXL345 because their I2C addresses differ.
static TwoWire ElevationWire(1);

struct AxisEncoderState {
  bool present = false;       // STATUS register responded
  bool read_ok = false;       // STATUS and both RAW ANGLE bytes read
  bool magnet_detected = false; // AS5600 STATUS.MD (bit 5)
  uint16_t raw = 0;            // 12-bit RAW ANGLE
  float sensor_angle_deg = 0.0f;
  uint32_t last_good_at = 0;
};

static AxisEncoderState azimuthEncoder;
static AxisEncoderState elevationEncoder;
static uint32_t lastAxisEncoderSampleAt = 0;
static constexpr uint32_t AXIS_ENCODER_SAMPLE_INTERVAL_MS = 100;

static bool readAS5600Register(TwoWire &bus, uint8_t reg, uint8_t &value) {
  bus.beginTransmission(Devices::AS5600_ADDRESS);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return false;
  if (bus.requestFrom((uint8_t)Devices::AS5600_ADDRESS, (uint8_t)1) != 1) return false;
  value = (uint8_t)bus.read();
  return true;
}

static void readAS5600(TwoWire &bus, AxisEncoderState &state, uint32_t now) {
  uint8_t status = 0, high = 0, low = 0;
  const bool status_ok = readAS5600Register(bus, 0x0B, status);
  const bool high_ok = readAS5600Register(bus, 0x0C, high);
  const bool low_ok = readAS5600Register(bus, 0x0D, low);
  state.present = status_ok;
  state.read_ok = status_ok && high_ok && low_ok;
  state.magnet_detected = status_ok && ((status & 0x20) != 0);
  if (state.read_ok) {
    state.raw = ((uint16_t)(high & 0x0F) << 8) | low;
    state.sensor_angle_deg = (float)state.raw * (360.0f / 4096.0f);
    state.last_good_at = now;
  }
}

static bool beginAxisEncoders() {
  const bool bus_ok = ElevationWire.begin(
    Pins::EL_AS5600_SDA, Pins::EL_AS5600_SCL, 100000
  );
  const uint32_t now = millis();
  readAS5600(Wire, azimuthEncoder, now);
  readAS5600(ElevationWire, elevationEncoder, now);
  Serial.printf(
    "[AS5600] I2C1=%s SDA=%d SCL=%d | AZ present=%u magnet=%u | EL present=%u magnet=%u\n",
    bus_ok ? "OK" : "FAIL",
    Pins::EL_AS5600_SDA, Pins::EL_AS5600_SCL,
    azimuthEncoder.present ? 1 : 0, azimuthEncoder.magnet_detected ? 1 : 0,
    elevationEncoder.present ? 1 : 0, elevationEncoder.magnet_detected ? 1 : 0
  );
  return bus_ok;
}

static void serviceAxisEncoders() {
  const uint32_t now = millis();
  if (now - lastAxisEncoderSampleAt < AXIS_ENCODER_SAMPLE_INTERVAL_MS) return;
  lastAxisEncoderSampleAt = now;
  readAS5600(Wire, azimuthEncoder, now);
  readAS5600(ElevationWire, elevationEncoder, now);
  static uint32_t last_log = 0;
  if (now - last_log >= 5000) {
    last_log = now;
    Serial.printf(
      "[AS5600] AZ present=%u read=%u MD=%u raw=%u deg=%.2f | EL present=%u read=%u MD=%u raw=%u deg=%.2f\n",
      azimuthEncoder.present ? 1 : 0, azimuthEncoder.read_ok ? 1 : 0,
      azimuthEncoder.magnet_detected ? 1 : 0, azimuthEncoder.raw,
      azimuthEncoder.sensor_angle_deg,
      elevationEncoder.present ? 1 : 0, elevationEncoder.read_ok ? 1 : 0,
      elevationEncoder.magnet_detected ? 1 : 0, elevationEncoder.raw,
      elevationEncoder.sensor_angle_deg
    );
  }
}

static bool azimuthEncoderValid() {
  return azimuthEncoder.present && azimuthEncoder.read_ok && azimuthEncoder.magnet_detected;
}
static bool elevationEncoderValid() {
  return elevationEncoder.present && elevationEncoder.read_ok && elevationEncoder.magnet_detected;
}
