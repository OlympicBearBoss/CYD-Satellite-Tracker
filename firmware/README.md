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
