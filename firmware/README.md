# CYD Firmware

Планируемая платформа: ESP32 Arduino или ESP-IDF с LVGL 9 и проверенным драйвером дисплея/тачскрина для конкретной ревизии CYD.

## Экраны
HOME, TRACK, RADAR, MAP, CONTROL, SATELLITES, INFO, SYSTEM, WIFI, GPS, MOTORS, CALIBRATION.

## Перед реализацией
- Уточнить ревизию CYD и фактическую распиновку TFT, touch, SD, audio и RGB LED.
- Проверить доступность GPIO для STEP/DIR и I²C.
- Настроить безопасные состояния при старте и потере питания.
- Проверить калибровку сенсорного экрана.

Это план, а не готовая прошивка. Не использовать предварительные GPIO без проверки pinout платы.

## Safe boot firmware prototype

A first PlatformIO implementation is now available in this folder:
- `platformio.ini` — ESP32 Arduino build target and libraries.
- `include/BoardConfig.h` — provisional GPIO map.
- `src/main.cpp` — Russian boot diagnostics screen and sensor checks.

The implementation is deliberately conservative: STEP outputs are held LOW,
drivers are never enabled, and no motor movement or homing is performed.
This code has not been compiled against the exact board revision or tested on
physical hardware. Verify the board/module, display wiring, PCF8574 address,
NC loop polarity, and driver carrier before flashing.


## GPS NEO-6M wiring (V95)

For the ESP32-2432S028R profile used by this firmware:
- NEO-6M TX -> CYD GPIO34 (UART2 RX, 9600 baud).
- NEO-6M VCC -> 3.3 V; GND -> GND.
- Leave NEO-6M RX disconnected; this firmware only receives NMEA data.
- Disconnect any GPS wire from GPIO4: this pin is assigned to AZ_STEP / the CYD RGB LED.
- Do not use GPIO26 for GPS on this profile: it is connected to the CYD speaker amplifier input.

GPIO34 is also connected to the on-board light sensor. The firmware does not use
that sensor; the GPS UART input takes priority. Confirm the exact CYD PCB revision
before wiring. A valid fix requires an outdoor/open-sky view and may take several
minutes on a cold start.
