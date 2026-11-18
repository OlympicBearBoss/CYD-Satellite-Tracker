#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_QMC5883P.h>
#include "BoardConfig.h"

// QMC5883P magnetometer and ADXL345 accelerometer share I2C0.
// Mounting convention: +X forward, +Y right, +Z upward.
// Adjust signs only after checking the actual sensor-board orientation.
namespace SensorMount {
constexpr float MAG_X_SIGN = 1.0f;
constexpr float MAG_Y_SIGN = 1.0f;
constexpr float MAG_Z_SIGN = 1.0f;
constexpr float ACCEL_PITCH_SIGN = 1.0f;
}

struct SensorSuiteState {
  bool mag_present=false, mag_ready=false, mag_read_ok=false;
  bool mag_data_ready=false, mag_overflow=false;
  int16_t mag_x=0, mag_y=0, mag_z=0;
  uint32_t mag_last_good_ms=0;
  bool accel_present=false, accel_read_ok=false, tilt_valid=false;
  int16_t accel_raw_x=0, accel_raw_y=0, accel_raw_z=0;
  float accel_x_g=0.0f, accel_y_g=0.0f, accel_z_g=1.0f;
  float roll_deg=0.0f, pitch_deg=0.0f;
  uint32_t accel_last_good_ms=0;
  bool heading_valid=false;
  float magnetic_heading_deg=0.0f, field_magnitude=0.0f;
};

static SensorSuiteState sensorSuite;
static Adafruit_QMC5883P qmc5883p;
static constexpr uint8_t QMC5883P_ADDRESS=0x2C;
static constexpr uint8_t ADXL345_ADDRESS=0x53;
static constexpr uint32_t SENSOR_POLL_INTERVAL_MS=10; // service loop at up to 100 Hz
static constexpr uint32_t QMC_SAMPLE_INTERVAL_MS=10; // consume fresh QMC samples at up to 100 Hz
static uint32_t sensorLastPollMs=0, qmcLastSampleMs=0;

static bool sensorWriteRegister(uint8_t address,uint8_t reg,uint8_t value) {
  Wire.beginTransmission(address); Wire.write(reg); Wire.write(value);
  return Wire.endTransmission()==0;
}
static bool sensorReadRegisters(uint8_t address,uint8_t reg,uint8_t* data,size_t length) {
  Wire.beginTransmission(address); Wire.write(reg);
  if(Wire.endTransmission(false)!=0) return false;
  const size_t received=Wire.requestFrom(address,(uint8_t)length);
  if(received!=length) { while(Wire.available()) (void)Wire.read(); return false; }
  for(size_t i=0;i<length;i++) data[i]=(uint8_t)Wire.read();
  return true;
}
static bool sensorReadRegister(uint8_t address,uint8_t reg,uint8_t& value) {
  return sensorReadRegisters(address,reg,&value,1);
}
static float sensorNormalize360(float value) {
  while(value<0.0f)value+=360.0f;
  while(value>=360.0f)value-=360.0f;
  return value;
}
static void sensorCalculateOrientation() {
  const float ax=sensorSuite.accel_x_g, ay=sensorSuite.accel_y_g, az=sensorSuite.accel_z_g;
  const float norm=sqrtf(ax*ax+ay*ay+az*az);
  sensorSuite.tilt_valid=sensorSuite.accel_read_ok && norm>=0.75f && norm<=1.25f;
  if(sensorSuite.tilt_valid) {
    sensorSuite.roll_deg=atan2f(ay,az)*57.2957795f;
    sensorSuite.pitch_deg=atan2f(-ax,sqrtf(ay*ay+az*az))*57.2957795f*SensorMount::ACCEL_PITCH_SIGN;
  }
  const float mx=sensorSuite.mag_x*SensorMount::MAG_X_SIGN;
  const float my=sensorSuite.mag_y*SensorMount::MAG_Y_SIGN;
  const float mz=sensorSuite.mag_z*SensorMount::MAG_Z_SIGN;
  sensorSuite.field_magnitude=sqrtf(mx*mx+my*my+mz*mz);
  sensorSuite.heading_valid=sensorSuite.mag_present && sensorSuite.mag_read_ok &&
    sensorSuite.tilt_valid && sensorSuite.mag_last_good_ms &&
    millis()-sensorSuite.mag_last_good_ms<1500 && sensorSuite.field_magnitude>1.0f;
  if(sensorSuite.heading_valid) {
    const float roll=sensorSuite.roll_deg*0.01745329252f;
    const float pitch=sensorSuite.pitch_deg*0.01745329252f;
    const float hx=mx*cosf(pitch)+mz*sinf(pitch);
    const float hy=mx*sinf(roll)*sinf(pitch)+my*cosf(roll)-mz*sinf(roll)*cosf(pitch);
    // +X points forward and +Y to the right: magnetic north is atan2(-Y, X).
    sensorSuite.magnetic_heading_deg=sensorNormalize360(atan2f(-hy,hx)*57.2957795f);
  }
}

static bool beginSensorSuite() {
  Wire.begin(Pins::I2C_SDA,Pins::I2C_SCL);
  Wire.setClock(100000);
  Serial.printf("[SENSOR] I2C SDA=%d SCL=%d 100kHz\n",Pins::I2C_SDA,Pins::I2C_SCL);

  // These three writes and their order are retained from the previously
  // hardware-checked QMC5883P setup. The Adafruit driver is then attached
  // explicitly to address 0x2C on the existing Wire bus.
  const bool q1=sensorWriteRegister(QMC5883P_ADDRESS,0x0B,0x01);
  delay(10);
  const bool q2=sensorWriteRegister(QMC5883P_ADDRESS,0x0A,0x01);
  delay(10);
  const bool q3=sensorWriteRegister(QMC5883P_ADDRESS,0x09,0x1D);
  delay(20);
  Serial.printf("[QMC5883P] startup writes 0B=%s 0A=%s 09=%s\n",
    q1?"ACK":"FAIL",q2?"ACK":"FAIL",q3?"ACK":"FAIL");
  if(q1&&q2&&q3) {
    sensorSuite.mag_present=true;
    sensorSuite.mag_ready=qmc5883p.begin(QMC5883P_ADDRESS,&Wire);
    if(!sensorSuite.mag_ready) sensorSuite.mag_present=false;
  }
  Serial.printf("[QMC5883P] present=%u begin=%s addr=0x2C\n",
    sensorSuite.mag_present?1:0,sensorSuite.mag_ready?"OK":"FAIL");

  uint8_t devid=0;
  sensorSuite.accel_present=sensorReadRegister(ADXL345_ADDRESS,0x00,devid) && devid==0xE5;
  if(sensorSuite.accel_present) {
    const bool rateOk=sensorWriteRegister(ADXL345_ADDRESS,0x2C,0x0A); // 100 Hz
    const bool formatOk=sensorWriteRegister(ADXL345_ADDRESS,0x31,0x08); // full resolution, +/-2 g
    const bool powerOk=sensorWriteRegister(ADXL345_ADDRESS,0x2D,0x08); // measurement mode
    sensorSuite.accel_read_ok=rateOk&&formatOk&&powerOk;
  }
  Serial.printf("[ADXL345] present=%u DEVID=0x%02X init=%s addr=0x53\n",
    sensorSuite.accel_present?1:0,devid,sensorSuite.accel_read_ok?"OK":"FAIL");
  sensorLastPollMs=millis()-SENSOR_POLL_INTERVAL_MS;
  qmcLastSampleMs=millis()-QMC_SAMPLE_INTERVAL_MS;
  return sensorSuite.mag_ready || sensorSuite.accel_read_ok;
}

static void serviceSensorSuite() {
  const uint32_t now=millis();
  if(now-sensorLastPollMs<SENSOR_POLL_INTERVAL_MS)return;
  sensorLastPollMs=now;

  if(sensorSuite.mag_ready && now-qmcLastSampleMs>=QMC_SAMPLE_INTERVAL_MS) {
    qmcLastSampleMs=now;
    sensorSuite.mag_data_ready=qmc5883p.isDataReady();
    sensorSuite.mag_overflow=qmc5883p.isOverflow();
    // Do not repeatedly read the same conversion. Keep the last good sample
    // until the QMC5883P signals a fresh one; discard overflowed conversions.
    if(sensorSuite.mag_data_ready && !sensorSuite.mag_overflow) {
      int16_t x=0,y=0,z=0;
      const bool readOk=qmc5883p.getRawMagnetic(&x,&y,&z);
      sensorSuite.mag_read_ok=readOk;
      if(readOk) {
        sensorSuite.mag_x=x; sensorSuite.mag_y=y; sensorSuite.mag_z=z;
        sensorSuite.mag_last_good_ms=now;
      }
    } else if(sensorSuite.mag_overflow) {
      sensorSuite.mag_read_ok=false;
    }
  }

  if(sensorSuite.accel_present) {
    uint8_t d[6];
    if(sensorReadRegisters(ADXL345_ADDRESS,0x32,d,6)) {
      const int16_t x=(int16_t)((uint16_t)d[1]<<8|d[0]);
      const int16_t y=(int16_t)((uint16_t)d[3]<<8|d[2]);
      const int16_t z=(int16_t)((uint16_t)d[5]<<8|d[4]);
      sensorSuite.accel_raw_x=x; sensorSuite.accel_raw_y=y; sensorSuite.accel_raw_z=z;
      const float gx=x*0.0039f, gy=y*0.0039f, gz=z*0.0039f;
      if(!sensorSuite.accel_last_good_ms) {
        sensorSuite.accel_x_g=gx; sensorSuite.accel_y_g=gy; sensorSuite.accel_z_g=gz;
      } else {
        constexpr float alpha=0.25f;
        sensorSuite.accel_x_g+=alpha*(gx-sensorSuite.accel_x_g);
        sensorSuite.accel_y_g+=alpha*(gy-sensorSuite.accel_y_g);
        sensorSuite.accel_z_g+=alpha*(gz-sensorSuite.accel_z_g);
      }
      sensorSuite.accel_read_ok=true; sensorSuite.accel_last_good_ms=now;
    } else sensorSuite.accel_read_ok=false;
  }
  sensorCalculateOrientation();

  static uint32_t lastLog=0;
  if(now-lastLog>=5000) {
    lastLog=now;
    Serial.printf("[SENSORS] QMC=%u/%u RDY=%u OVF=%u raw=%d,%d,%d hdg=%s%.1f | ADXL=%u/%u g=%.2f,%.2f,%.2f roll=%.1f pitch=%.1f\n",
      sensorSuite.mag_present?1:0,sensorSuite.mag_read_ok?1:0,
      sensorSuite.mag_data_ready?1:0,sensorSuite.mag_overflow?1:0,
      sensorSuite.mag_x,sensorSuite.mag_y,sensorSuite.mag_z,
      sensorSuite.heading_valid?"":"--",sensorSuite.magnetic_heading_deg,
      sensorSuite.accel_present?1:0,sensorSuite.accel_read_ok?1:0,
      sensorSuite.accel_x_g,sensorSuite.accel_y_g,sensorSuite.accel_z_g,
      sensorSuite.roll_deg,sensorSuite.pitch_deg);
  }
}
