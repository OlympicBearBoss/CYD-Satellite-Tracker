#pragma once
#include <Arduino.h>

// CYD ESP32-2432S028 pin map confirmed for the current hardware profile.
namespace Pins {
constexpr int TFT_BACKLIGHT = 21;
constexpr int I2C_SDA = 27;
constexpr int I2C_SCL = 22;

// HR4988 step/direction signals.
constexpr int AZ_STEP = 4;
constexpr int AZ_DIR  = 16;
constexpr int EL_STEP = 17;
constexpr int EL_DIR  = 26;

// NEO-6M TX -> ESP32 UART2 RX on GPIO34.
// GPIO34 is input-only on ESP32 and is used only as GPS UART RX.
// GPS module RX is not connected / not used by this firmware.
constexpr int GPS_RX = 34;
constexpr int GPS_TX_UNUSED = -1;

constexpr int EL_AS5600_ANALOG = 35;
}

namespace Devices {
constexpr uint8_t AS5600_ADDRESS = 0x36;
constexpr uint8_t PCF8574_ADDRESS = 0x20; // Verify with I2C scan.
constexpr uint8_t PCF_LIMIT_MASK = 0x0F;  // P0..P3
constexpr uint8_t PCF_ESTOP_BIT = 0x20;   // P5, auxiliary NC loop only
}

namespace Safety {
// Set true ONLY after verifying the actual HR4988 carrier, ENABLE polarity,
// wiring and external pull-up. This firmware build deliberately never enables
// drivers automatically, regardless of this setting.
constexpr bool DRIVER_ENABLE_HARDWARE_VERIFIED = false;
}
