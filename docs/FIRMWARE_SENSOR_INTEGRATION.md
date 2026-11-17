# CYD Satellite Tracker — firmware sensor integration

Status: integration specification, based on the currently verified hardware tests. This document does not claim that every sensor is already integrated into the production UI.

## 1. Confirmed controller and pin map

- Controller: ESP32-2432S028 / CYD 2.8, ESP32-D0WD-V3.
- Motor signals: AZ STEP GPIO4, AZ DIR GPIO16, EL STEP GPIO17, EL DIR GPIO26.
- GPS NEO-6M: module TX -> ESP32 UART2 RX GPIO34; GPS module RX is unused.
- GPIO34 is input-only on ESP32. Use it only as UART RX, never as a motor/control output.
- I²C: SDA GPIO27, SCL GPIO22, 100 kHz.
- Logic voltage: 3.3 V; share ground. Never apply 5 V to I²C signal pins.
- Do not start motors during sensor bring-up or calibration.

Confirmed I²C devices:
| Device | Address | Evidence / current status |
|---|---:|---|
| QMC5883P magnetometer | 0x2C | V6 continuous test: ready=1, overflow=0, successful reads and stable plateaus after hand rotation |
| ADXL345 accelerometer | 0x53 | DEVID 0xE5 read successfully during the QMC test; continuous acceleration sampling is not yet validated |
| AS5600 azimuth angle sensor (planned) | 0x36 | Wiring plan only; not yet installed or validated |
| PCF8574 limit-input expander (planned) | usually 0x20 | Wiring plan only; scan and pin state validation required |

Addresses must be checked together with an I²C scan before adding any new device. Do not assume planned devices are physically connected.

## 2. Sensor ownership and startup

Create one sensor service as the sole owner of I²C initialization and sensor polling. The display, web API and tracking logic consume a read-only snapshot; they must not access I²C independently.

Startup order:
1. Start serial diagnostics and initialise the I²C bus on GPIO27/GPIO22 at 100 kHz.
2. Scan the bus and record which expected addresses respond.
3. Initialise QMC5883P using the exact, user-confirmed register write order:
   - register 0x0B <- 0x01
   - register 0x0A <- 0x01
   - register 0x09 <- 0x1D
4. Start the Adafruit_QMC5883P driver at address 0x2C and verify chip ID 0x80.
5. Identify ADXL345 at 0x53; expected DEVID register 0x00 = 0xE5. Its full measurement configuration is a separate implementation/test step.
6. Probe future angle/limit devices only when installed and verified.
7. Start GPS UART2 receive on GPIO34 (9600 baud, RX only), then the UI/network services.
8. Keep all motor STEP outputs inactive and driver ENABLE locked. Sensor failure must never enable motion.

The QMC5883P register sequence above is fixed. Do not replace its driver with QMC5883L or HMC5883L code.

## 3. Read-only telemetry contract

Expose a snapshot to the UI and API. Suggested fields:

- gps: present, fix_valid, latitude_deg, longitude_deg, altitude_m, satellites, utc.
- magnetometer: present, ready, overflow, read_ok, raw_x, raw_y, raw_z, calibrated, heading_valid, heading_deg.
- accelerometer: present, read_ok, raw_x, raw_y, raw_z, tilt_valid.
- azimuth_sensor and elevation_sensor: present, read_ok, angle_deg, calibrated.
- limits: each switch state plus valid.
- system: I²C health, last sensor error, uptime and firmware version.

Every sensor value must carry validity/status information. Never display a stale numeric value as current when a read fails. A missing optional sensor should be shown as unavailable, not as zero degrees.

## 4. Processing rules

- QMC raw XYZ values are diagnostic until the magnetometer is calibrated in its final mechanical installation.
- Do not calculate a production heading from the current hand-rotation data. The test proves communication and response, not absolute azimuth accuracy.
- A reliable compass heading requires hard-/soft-iron calibration and tilt compensation using the accelerometer. Recalibrate after mounting changes or nearby ferromagnetic/electrical changes.
- GPS provides station coordinates and UTC. GPS course-over-ground is not a stationary antenna heading.
- Satellite pointing calculations use station coordinates, current UTC and satellite orbit data. They produce target azimuth/elevation; they do not directly authorise motor motion.
- The final antenna axis feedback should come from installed angle sensors/encoders, not from assuming motor step count is actual position.

## 5. Scheduling and fault handling

- Poll sensors with non-blocking periodic tasks; do not use long delays in the display or network loop.
- Keep I²C transactions short and serialise access through the sensor service.
- On a failed read, preserve the last sample for diagnostics but mark it stale/invalid.
- Repeated I²C errors should set a sensor fault and be visible in the System screen and /api/v1/status.
- A GPS no-fix condition is not an I²C fault; show GPS receiver presence separately from fix validity.
- A sensor fault, stale angle feedback, active limit, expired motion lease or lost controller heartbeat blocks motion.

## 6. Motion safety boundary

The controller must remain safe even if the mobile app, website or Wi-Fi connection fails:

- Hardware E-stop independently interrupts motor power/driver enable.
- Limit switches block travel farther into the active limit and allow controlled retreat.
- Motion commands are range-checked locally and require a short-lived dead-man/lease.
- A watchdog stops motion on command timeout or controller fault.
- HR4988 driver ENABLE remains disabled until the exact carrier board, enable polarity, current-limit setting and wiring are verified.
- No automatic homing, tracking movement or driver enable is part of sensor integration.

## 7. Current verification ledger

| Item | Status |
|---|---|
| Confirmed motor GPIO mapping: 4, 16, 17, 26 | Confirmed by user and motor test firmware |
| GPS NEO-6M TX -> UART2 RX GPIO34 | Confirmed pin assignment; live GPS fix still to be checked |
| QMC5883P detected at 0x2C | Verified |
| QMC startup register writes in required order | Verified |
| QMC continuous reads, ready=1, overflow=0 | Verified in V6 |
| QMC response to approximate hand rotation | Verified; not a precision angle test |
| ADXL345 identity at 0x53 / DEVID 0xE5 | Verified |
| ADXL345 continuous acceleration and tilt | Not yet tested |
| AS5600 angle feedback | Planned; not installed/verified |
| PCF8574 limit inputs | Planned; not installed/verified |
| HR4988 motor control | Locked; carrier and safety wiring must be verified first |
| Integrated sensor dashboard/API | Not yet implemented |

## 8. Next implementation sequence

1. Add a sensor service and typed telemetry snapshot without changing motor states.
2. Integrate QMC5883P polling using the proven driver and fixed startup sequence.
3. Add ADXL345 continuous sampling and validate axes while stationary and tilted.
4. Connect sensor validity/staleness to the System and Antenna screens and status API.
5. Only after the rotator is assembled, calibrate the magnetic sensor and validate installed angle feedback.
6. Implement motor control as a separate safety-gated subsystem.
