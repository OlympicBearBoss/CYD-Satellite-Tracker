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
| HMC5883L magnetometer | 0x1E | Firmware now checks ID registers 0x48/0x34/0x33 and polls XYZ data; physical test pending |
| ADXL345 accelerometer | 0x53 | DEVID 0xE5 read successfully during the QMC test; continuous acceleration sampling is not yet validated |
| AS5600 azimuth angle sensor (planned) | 0x36 | Wiring plan only; not yet installed or validated |
| PCF8574 limit-input expander (planned) | usually 0x20 | Wiring plan only; scan and pin state validation required |

Addresses must be checked together with an I²C scan before adding any new device. Do not assume planned devices are physically connected.

## 2. Sensor ownership and startup

Create one sensor service as the sole owner of I²C initialization and sensor polling. The display, web API and tracking logic consume a read-only snapshot; they must not access I²C independently.

Startup order:
1. Start serial diagnostics and initialise the I²C bus on GPIO27/GPIO22 at 100 kHz.
2. Scan the bus and record which expected addresses respond.
3. Identify HMC5883L at 0x1E by checking ID registers 0x0A–0x0C for 0x48, 0x34, 0x33.
4. Configure HMC5883L: CRA 0x00=0x70 (8-sample average, 15 Hz), CRB 0x01=0x20 (±1.3 Ga), MODE 0x02=0x00 (continuous). Read XYZ from 0x03–0x08 as signed big-endian values in X/Z/Y register order.
5. Identify ADXL345 at 0x53; DEVID register 0x00 must equal 0xE5. Configure BW_RATE 0x2C=0x0A, DATA_FORMAT 0x31=0x08 (full-resolution ±2 g), POWER_CTL 0x2D=0x08 (measurement). Read XYZ from 0x32–0x37 as signed little-endian values.
6. Probe optional AS5600 shaft encoders and limit devices only when installed and verified.
7. Start GPS UART2 receive on GPIO34 (9600 baud, RX only), then the UI/network services.
8. Keep all motor STEP outputs inactive and driver ENABLE locked. Sensor failure must never enable motion.

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

- HMC5883L raw XYZ values are diagnostic until hard-/soft-iron calibration in the final mechanical installation. Firmware reports a preliminary tilt-compensated magnetic heading but marks it uncalibrated.
- ADXL345 derives roll/pitch from gravity with a low-pass filter. Tilt is valid only while acceleration magnitude remains near 1 g; strong motion/vibration invalidates tilt.
- Default mounting assumes +X forward, +Y right, +Z upward for both boards. Verify orientation physically; adjust axis signs if the modules are mounted differently.
- Convert magnetic heading to true-north azimuth only after applying local magnetic declination. Firmware currently reports magnetic heading, not true heading.
- Recalibrate after mounting changes or nearby ferromagnetic/electrical changes.
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
| HMC5883L ID check and register configuration | Implemented in firmware; physical test pending |
| HMC5883L continuous XYZ sampling | Implemented in firmware; physical test pending |
| ADXL345 identity at 0x53 / DEVID 0xE5 | Verified |
| ADXL345 continuous XYZ sampling and tilt calculation | Implemented in firmware; physical test pending |
| AS5600 angle feedback | Planned; not installed/verified |
| PCF8574 limit inputs | Planned; not installed/verified |
| HR4988 motor control | Locked; carrier and safety wiring must be verified first |
| Integrated sensor dashboard/API | Not yet implemented |

## 8. Next implementation sequence

1. Add a sensor service and typed telemetry snapshot without changing motor states.
2. Validate HMC5883L identity, continuous XYZ sampling and axis orientation on the assembled board.
3. Validate ADXL345 gravity vector and pitch sign in level and tilted positions.
4. Calibrate HMC hard-/soft-iron offsets and apply local magnetic declination before treating heading as true azimuth.
5. Validate combined heading/tilt telemetry on the final antenna mount.
6. Implement motor control as a separate safety-gated subsystem.


## 9. Dual AS5600 axis feedback

- AZ AS5600: I2C0 / Wire, SDA GPIO27, SCL GPIO22, address 0x36. It shares the bus with HMC5883L (0x1E) and ADXL345 (0x53); addresses do not conflict.
- EL AS5600: I2C1 / ElevationWire, SDA GPIO18, SCL GPIO19, address 0x36. GPIO18/19 are the CYD SD-slot SPI pins; SD-card use is disabled/reserved in this pin profile.
- Both sensors are polled every 100 ms. Firmware reads STATUS (0x0B), RAW ANGLE high (0x0C) and low (0x0D); the 12-bit value is converted to sensor degrees. STATUS.MD (bit 5) indicates that a magnet is detected. Bus response, register read, magnet detection and zero calibration are reported separately.
- /api/v1/status exposes axis_sensors.azimuth and axis_sensors.elevation, including bus, presence, read status, magnet detection, raw angle, calibrated angle (only after zeroing), calibration state and sample age.
- Local API command: {"command":"calibrate_zero","axis":"azimuth"} when the antenna is physically aimed at true north; {"command":"calibrate_zero","axis":"elevation"} when the elevation axis is physically level (0°). Offsets are stored in ESP32 NVS.
- Before zero calibration, readings are raw encoder shaft angles and must not be treated as true compass azimuth/elevation. The sensors have not yet been physically installed or hardware-tested; motor movement remains locked.
