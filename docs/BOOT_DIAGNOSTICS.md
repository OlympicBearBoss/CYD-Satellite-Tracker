# CYD Satellite Tracker — boot diagnostics and startup screen

Version: 1.0
Target: ESP32-2432S028, 320×240 landscape
Status: implementation specification; not yet flashed or hardware-tested.

## Startup safety contract
1. Configure both STEP outputs LOW immediately.
2. Keep both driver ENABLE inputs inactive using external hardware pull-ups. Do not rely on firmware alone for the power-up safe state.
3. Do not issue any STEP pulses during automatic initialization.
4. Initialize the display and show the diagnostic screen.
5. Read sensors, limit switches, GPS and emergency-stop auxiliary contact.
6. Keep motor power/driver enable inhibited until the operator explicitly starts a permitted test or tracking operation.
7. Never automatically home or move the antenna at boot. A movement-based test/homing sequence requires a deliberate user command, clear travel path and verified mechanical limits.

## Startup checklist and state machine
States: WAITING, RUNNING, PASS, WARNING, FAIL, SKIPPED.

| Order | Test | PASS condition | WARNING / FAIL behavior |
|---:|---|---|---|
| 1 | ESP32 runtime | Firmware started, watchdog configured | Restart/fatal fault if runtime setup fails |
| 2 | TFT display | ILI9341 initialized and frame drawn | Fatal UI fault; motors remain disabled |
| 3 | Touch controller | XPT2046 responds to touch test if library supports controller check | WARNING if controller cannot be positively queried; manual UI unavailable |
| 4 | I2C bus | Bus starts on SDA GPIO27 / SCL GPIO22 | FAIL if bus cannot start |
| 5 | AS5600 azimuth | I2C address 0x36 found; angle/status data plausible and stable | FAIL; no azimuth movement |
| 6 | PCF8574 | Address responds (commonly 0x20 with A0–A2 grounded); read inputs | FAIL; no motor enable |
| 7 | Four limit loops | Read P0–P3; identify actuated limits. Both limits on the same axis active simultaneously is a fault | Active single limit is a valid state; movement toward it is prohibited |
| 8 | E-stop auxiliary loop | P5 reports emergency stop released/closed | FAIL; motor power must remain physically interrupted |
| 9 | AS5600 elevation | GPIO35 ADC signal is within calibrated electrical range and stable; convert to angle only after calibration | FAIL; no elevation movement |
| 10 | GPS NEO-6M | UART2 RX on GPIO34 receives valid NMEA sentences | WARNING if no satellite fix yet; valid module communication is distinct from 3D fix |
| 11 | HR4988 control lines | STEP/DIR outputs configured; ENABLE remains inactive until all interlocks pass | Display “control lines ready”; actual driver health cannot be confirmed without a diagnostic output |
| 12 | Motor/axis response | Only after explicit user-started low-speed test: command a tiny safe movement and verify corresponding AS5600 change | NOT RUN at boot; report “test required”. Do not infer motor health from STEP/DIR pin setup |

## GPIO notes for this board plan
- I2C SDA=GPIO27, SCL=GPIO22.
- AS5600 azimuth uses I2C address 0x36.
- AS5600 elevation analog OUT=GPIO35 (input-only ADC); confirm breakout is powered at 3.3 V and OUT never exceeds 3.3 V.
- GPS TX -> GPIO34 (UART2 RX). GPS RX is not required for read-only NMEA reception and remains disconnected. GPIO34 is input-only and is also used by the onboard LDR on common CYD boards; LDR is sacrificed.
- AZ STEP=GPIO4, AZ DIR=GPIO18; EL STEP=GPIO19, EL DIR=GPIO23. The SD card is not used; GPIO18/19/23 are reassigned. GPIO4 onboard RGB channel is sacrificed.
- GPIO16/17 are deliberately avoided because some 2432S028 versions use an ESP32-WROVER module with PSRAM on these pins.
- PCF8574 P0/P1/P2/P3 = AZ_LEFT/AZ_RIGHT/EL_BOTTOM/EL_TOP; P4 = optional shared active-low driver enable request only after exact driver carrier is verified; P5 = E-stop auxiliary NC loop.
- Verify the exact ESP32 module marking and PCB revision before wiring. GPIO assignments are a planned allocation, not a substitute for continuity checks on the actual board.

## Motor safety and what can actually be diagnosed
The HR4988 carrier modules do not provide a universally available digital driver-healthy response. Unless the exact module exposes a documented fault output, firmware cannot prove that the driver, motor windings, motor cable or mechanical transmission is healthy merely by setting STEP/DIR pins.

At boot, firmware must:
- hold STEP LOW;
- hold active-low ENABLE inactive through a physical pull-up;
- configure DIR to a known idle level;
- show “управление подготовлено, силовой тест не выполнялся”;
- never pulse a motor automatically.

A motor response test is a separate operator-started procedure. It must use a conservative current limit established from the exact driver carrier and motor datasheet, a clear mechanical area, a reachable emergency stop, and a small movement. Confirm movement from the corresponding AS5600 sensor. Do not perform this test until the HR4988 board pinout, current-sense resistor and motor coil pairs have been verified.

## Display design — Russian default
Landscape 320×240, Space Dark palette (navy background, blue/cyan accents):
- Header: CYD SATELLITE TRACKER, firmware version, supply voltage if measured, clock.
- Main panel title: ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ.
- Progress bar and percentage based on completed checklist items.
- Each row has a status glyph: green check = passed; cyan spinner = running; amber triangle = warning/waiting for GPS fix; red cross = failure; grey dash = not run/not available.
- Right-side cards: GPS status, azimuth angle, elevation angle.
- Bottom row: Настройки, Диагностика, Пропустить ожидание GPS (only if sensor communication is good; does not enable satellite tracking), and a large red СТОП.
- The STOP control is always visible during any motor test and tracking screen.

Suggested checklist rows:
1. Дисплей и сенсор
2. Шина I²C
3. Энкодер азимута AS5600
4. Энкодер угла места AS5600
5. Концевики и аварийный стоп
6. GPS NEO-6M
7. Драйверы: линии STEP/DIR
8. Проверка двигателей — только по команде

## End states
### READY
All required local hardware checks passed, E-stop released, no conflicting limits. Motors remain disabled until the operator selects a manual test or tracking mode. GPS fix may still be pending; satellite auto-tracking remains unavailable until a valid fix and current orbit data are available.

### READY WITH WARNING
Local sensors and safety inputs are valid, but GPS has no fix or another non-motion-critical service is waiting. Manual diagnostics may be available; satellite auto-tracking is locked until GPS fix.

### BLOCKED
Any required angle sensor, PCF8574, limit loop, E-stop auxiliary loop, display/runtime or safety interlock fails. Keep drivers disabled and show the exact fault.

## Acceptance tests
- Power-cycle repeatedly: no motor movement or STEP pulses at startup.
- Disconnect each sensor in turn: correct fault is shown and motors stay disabled.
- Open each NC limit loop: corresponding limit/fault is shown.
- Press E-stop: motor supply is physically disconnected independently of software.
- Remove GPS antenna / wait without sky view: GPS communication can pass while fix is shown as waiting; satellite tracking remains locked.
- Only after all wiring and current settings are verified, run a manually initiated low-speed axis test and confirm angle feedback changes in the correct direction.