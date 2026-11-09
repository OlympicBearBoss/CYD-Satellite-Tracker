# CYD Satellite Tracker — wiring plan (ESP32-2432S028)

Status: wiring plan v1.0. Confirm the exact carrier-board pin labels and component revisions before applying motor power.

## Hardware
- CYD ESP32-2432S028, ESP32-WROOM-32
- 2 × US-17HS4401S stepper motors
- 2 × HR4988 driver modules (carrier-board pinout and sense resistor must be checked)
- 2 × AS5600 magnetic angle sensors
- 1 × NEO-6M-0-001 GPS breakout
- 1 × PCF8574 I2C GPIO expander for four limit-switch inputs
- 4 × SPDT roller-lever limit switches, wired COM–NC
- 12 V motor supply and regulated 5 V supply for CYD

## GPIO allocation

| Function | CYD GPIO | Notes |
|---|---:|---|
| I2C SDA | 27 | CN1; AS5600 azimuth + PCF8574 |
| I2C SCL | 22 | CN1/P3; AS5600 azimuth + PCF8574 |
| AS5600 elevation OUT | 35 | P3; analog output only, GPIO35 is input-only |
| AZ STEP | 4 | RGB red LED sacrificed |
| AZ DIR | 18 | SD-card SPI pin; SD card unused |
| EL STEP | 19 | SD-card SPI pin; SD card unused |
| EL DIR | 23 | SD-card SPI pin; SD card unused |
| GPS UART2 RX | 16 | Connect GPS TX to ESP32 GPIO16 |
| GPS UART2 TX | 17 | Connect ESP32 GPIO17 to GPS RX |

GPIO16 and GPIO17 are used for GPS, so the onboard RGB LED is unavailable. GPIO4 is used for AZ STEP, so the red LED channel is also unavailable. The microSD interface is not used because GPIO18/19/23 are reassigned to motor control. USB programming/serial console remains on GPIO1/GPIO3.

## I2C devices
- AS5600 azimuth: VCC=3.3 V, GND, SDA=GPIO27, SCL=GPIO22; expected address 0x36.
- PCF8574: power at 3.3 V; SDA=GPIO27, SCL=GPIO22. If A0/A1/A2 are grounded, address is usually 0x20; verify by I2C scan.
- AS5600 elevation: VCC=3.3 V, GND, OUT=GPIO35. Configure/verify the breakout OUT pin for analog mode and confirm the output never exceeds 3.3 V before connecting to GPIO35. GPIO35 has no internal pull-up and is input-only.

## Limit switches
Connect four SPDT switches using COM and NC:
- PCF8574 P0: AZ_LEFT
- PCF8574 P1: AZ_RIGHT
- PCF8574 P2: EL_BOTTOM
- PCF8574 P3: EL_TOP
- PCF8574 P4: common HR4988 ENABLE control (both drivers together; verify the carrier exposes active-low ENABLE). Use a 10 kΩ pull-up to 3.3 V so the drivers default disabled at power-up; firmware may pull P4 low only after safety checks.

Wire each NC loop so it is closed during normal travel and opens when the switch is actuated. With PCF8574 powered at 3.3 V, initialise P0–P3 as high/input and read them periodically. An open circuit is treated as a limit/fault. Confirm the actual switch terminal markings with a multimeter.

The PCF8574 is a status/input expander, not a safety-rated emergency-stop circuit. P4 can provide a shared active-low ENABLE request only after the exact HR4988 carrier is verified; it is not a substitute for an emergency stop. Add a separate latching emergency stop that disconnects the 12 V motor supply, and use physical mechanical stops. Firmware must block movement further into an active limit while allowing controlled movement away from it.

## HR4988 driver connections
For each carrier, use the carrier's verified STEP, DIR, ENABLE, RESET, SLEEP, VDD, VMOT and GND labels:
- AZ STEP=GPIO4, AZ DIR=GPIO18
- EL STEP=GPIO19, EL DIR=GPIO23
- Both active-low ENABLE inputs may be controlled together from PCF8574 P4 through the verified carrier interface; fit a 10 kΩ pull-up to 3.3 V so power-up defaults to disabled. Firmware sets P4 low only after checking sensors and limits.
- Logic VDD from the verified 3.3 V-compatible logic supply; common ground with CYD.
- VMOT from the separate 12 V motor supply.
- Motor coils go to the two output coil pairs; identify coil pairs with a multimeter, do not rely on wire colours.
- Add local VMOT decoupling at each driver according to its carrier datasheet.

Do not set current limit, microstep jumpers, RESET/SLEEP or ENABLE wiring until the exact HR4988 carrier-board pinout and sense-resistor markings are verified. The chip name alone does not identify every carrier layout.

## Power and grounding
- 12 V supply feeds only the motor-driver VMOT power path.
- Use a suitable regulated 5 V supply for the CYD VIN/5V input.
- Power sensor logic from 3.3 V, not 5 V.
- Join logic ground and driver ground at a deliberate common ground point.
- Do not power motors from the CYD board.
- Keep motor wiring away from GPS antenna and AS5600 signal wiring.

## Mechanical arrangement
- AS5600 azimuth sensor is fixed to the stationary frame; its diametric magnet is fixed concentrically to the rotating azimuth shaft.
- AS5600 elevation sensor is fixed to the stationary yoke; its magnet is fixed concentrically to the elevation shaft.
- Two azimuth roller switches define the cable-safe travel window. Two elevation switches define the 0° and 90° travel limits.
- Since azimuth wiring is not routed through a slip ring, the usable travel must be less than the full turn by a cable-safe margin. Determine the final angle after measuring the cable loop and mechanical stops.

## Commissioning sequence
1. With all power disconnected, identify board revision and verify connector pin order.
2. Verify GPS breakout supply requirements and UART logic level.
3. Verify AS5600 OUT mode and voltage with a meter.
4. Verify PCF8574 address and all four NC switch states using 3.3 V only.
5. Verify each HR4988 carrier pinout, sense resistor, and motor coil pairs.
6. Test CYD logic and sensor readings with motor supply disconnected.
7. Test one motor at low current with the antenna unloaded and emergency stop accessible.
8. Calibrate sensor zero points and limit positions before enabling automatic satellite tracking.
