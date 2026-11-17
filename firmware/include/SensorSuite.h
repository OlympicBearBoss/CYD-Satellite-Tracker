#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "BoardConfig.h"

// HMC5883L + ADXL345 share the CYD I2C bus.
// Default mounting convention: +X forward, +Y right, +Z upward.
// Change these signs if the physical breakout orientation differs.
namespace SensorMount {
constexpr float MAG_X_SIGN = 1.0f;
constexpr float MAG_Y_SIGN = 1.0f;
constexpr float MAG_Z_SIGN = 1.0f;
constexpr float ACCEL_PITCH_SIGN = 1.0f;
}

struct SensorSuiteState {
  bool hmc_present=false, hmc_ready=false, hmc_read_ok=false;
  bool hmc_data_ready=false, hmc_locked=false;
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
static constexpr uint8_t HMC5883L_ADDRESS=0x1E;
static constexpr uint8_t ADXL345_ADDRESS=0x53;
static constexpr uint32_t SENSOR_POLL_INTERVAL_MS=100;
static uint32_t sensorLastPollMs=0;

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
  sensorSuite.heading_valid=sensorSuite.hmc_present && sensorSuite.hmc_read_ok &&
    sensorSuite.tilt_valid && sensorSuite.mag_last_good_ms &&
    millis()-sensorSuite.mag_last_good_ms<1000 && sensorSuite.field_magnitude>1.0f;
  if(sensorSuite.heading_valid) {
    const float roll=sensorSuite.roll_deg*0.01745329252f;
    const float pitch=sensorSuite.pitch_deg*0.01745329252f;
    const float hx=mx*cosf(pitch)+mz*sinf(pitch);
    const float hy=mx*sinf(roll)*sinf(pitch)+my*cosf(roll)-mz*sinf(roll)*cosf(pitch);
    // With +X forward and +Y right, magnetic north is atan2(-Y, X).
    sensorSuite.magnetic_heading_deg=sensorNormalize360(atan2f(-hy,hx)*57.2957795f);
  }
}
static bool beginSensorSuite() {
  Wire.begin(Pins::I2C_SDA,Pins::I2C_SCL);
  Wire.setClock(100000);
  Serial.printf("[SENSOR] I2C SDA=%d SCL=%d 100kHz\n",Pins::I2C_SDA,Pins::I2C_SCL);

  uint8_t id[3]={0,0,0};
  sensorSuite.hmc_present=sensorReadRegisters(HMC5883L_ADDRESS,0x0A,id,3) &&
    id[0]==0x48 && id[1]==0x34 && id[2]==0x33;
  if(sensorSuite.hmc_present) {
    // 8-sample average, 15 Hz, normal mode; gain +/-1.3 Ga; continuous conversion.
    const bool a=sensorWriteRegister(HMC5883L_ADDRESS,0x00,0x70);
    const bool b=sensorWriteRegister(HMC5883L_ADDRESS,0x01,0x20);
    const bool c=sensorWriteRegister(HMC5883L_ADDRESS,0x02,0x00);
    sensorSuite.hmc_ready=a&&b&&c;
  }
  Serial.printf("[HMC5883L] present=%u ID=%02X %02X %02X init=%s addr=0x1E\n",
    sensorSuite.hmc_present?1:0,id[0],id[1],id[2],sensorSuite.hmc_ready?"OK":"FAIL");

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
  return sensorSuite.hmc_ready || sensorSuite.accel_read_ok;
}
static void serviceSensorSuite() {
  const uint32_t now=millis();
  if(now-sensorLastPollMs<SENSOR_POLL_INTERVAL_MS)return;
  sensorLastPollMs=now;
  if(sensorSuite.hmc_ready) {
    uint8_t status=0;
    if(sensorReadRegister(HMC5883L_ADDRESS,0x09,status)) {
      sensorSuite.hmc_data_ready=(status&0x01)!=0;
      sensorSuite.hmc_locked=(status&0x02)!=0;
      if(sensorSuite.hmc_data_ready) {
        uint8_t d[6];
        if(sensorReadRegisters(HMC5883L_ADDRESS,0x03,d,6)) {
          // HMC register order is X, Z, Y; signed big-endian.
          sensorSuite.mag_x=(int16_t)((uint16_t)d[0]<<8|d[1]);
          sensorSuite.mag_z=(int16_t)((uint16_t)d[2]<<8|d[3]);
          sensorSuite.mag_y=(int16_t)((uint16_t)d[4]<<8|d[5]);
          sensorSuite.hmc_read_ok=true; sensorSuite.mag_last_good_ms=now;
        } else sensorSuite.hmc_read_ok=false;
      } else if(!sensorSuite.mag_last_good_ms || now-sensorSuite.mag_last_good_ms>1000) sensorSuite.hmc_read_ok=false;
    } else sensorSuite.hmc_read_ok=false;
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
    Serial.printf("[SENSORS] HMC=%u/%u RDY=%u LOCK=%u raw=%d,%d,%d hdg=%s%.1f | ADXL=%u/%u g=%.2f,%.2f,%.2f roll=%.1f pitch=%.1f\n",
      sensorSuite.hmc_present?1:0,sensorSuite.hmc_read_ok?1:0,
      sensorSuite.hmc_data_ready?1:0,sensorSuite.hmc_locked?1:0,
      sensorSuite.mag_x,sensorSuite.mag_y,sensorSuite.mag_z,
      sensorSuite.heading_valid?"":"--",sensorSuite.magnetic_heading_deg,
      sensorSuite.accel_present?1:0,sensorSuite.accel_read_ok?1:0,
      sensorSuite.accel_x_g,sensorSuite.accel_y_g,sensorSuite.accel_z_g,
      sensorSuite.roll_deg,sensorSuite.pitch_deg);
  }
}
