import {json,digest,readJson} from "../auth/_shared.js";

const finite=(value,min,max)=>typeof value==="number"&&Number.isFinite(value)
  ?Math.max(min,Math.min(max,value)):null;
const integer=(value,min,max)=>typeof value==="number"&&Number.isFinite(value)
  ?Math.max(min,Math.min(max,Math.round(value))):null;

export async function onRequestPost({request,env}) {
  const auth=request.headers.get("Authorization")||"";
  const token=auth.startsWith("Bearer ")?auth.slice(7).trim():"";
  if(token.length<32||token.length>256)return json({error:"Требуется токен устройства"},401);

  const body=await readJson(request)||{};
  const mag=body.magnetometer&&typeof body.magnetometer==="object"?body.magnetometer:{};
  const accel=body.accelerometer&&typeof body.accelerometer==="object"?body.accelerometer:{};
  const telemetry={
    wifi_rssi:finite(body.wifi_rssi,-127,0),
    ip:String(body.ip||"").slice(0,48),
    gps_valid:body.gps_valid===true,
    latitude:finite(body.latitude,-90,90),
    longitude:finite(body.longitude,-180,180),
    altitude:finite(body.altitude,-1000,100000),
    gps_satellites:integer(body.gps_satellites,0,99),
    gps_hdop:finite(body.gps_hdop,0,999),
    gps_age_ms:integer(body.gps_age_ms,0,3600000),
    firmware:String(body.firmware||"").slice(0,40),
    hardware_id:/^CYD-[A-F0-9]{12}$/.test(String(body.hardware_id||""))?String(body.hardware_id):null,
    gps_connected:body.gps_connected===true,
    angle_sensor_connected:body.angle_sensor_connected===true,
    compass_connected:body.compass_connected===true,
    motor_drivers_connected:body.motor_drivers_connected===true,
    limit_switches_connected:body.limit_switches_connected===true,
    magnetometer:{
      type:"QMC5883P",
      address:"0x2C",
      present:mag.present===true,
      read_ok:mag.read_ok===true,
      data_ready:mag.data_ready===true,
      overflow:mag.overflow===true,
      heading_valid:mag.heading_valid===true,
      calibrated:mag.calibrated===true,
      raw_x:integer(mag.raw_x,-32768,32767),
      raw_y:integer(mag.raw_y,-32768,32767),
      raw_z:integer(mag.raw_z,-32768,32767),
      field_magnitude_raw:finite(mag.field_magnitude_raw,0,1000000),
      magnetic_heading_deg:finite(mag.magnetic_heading_deg,0,360),
      sample_age_ms:integer(mag.sample_age_ms,0,3600000)
    },
    accelerometer:{
      type:"ADXL345",
      address:"0x53",
      present:accel.present===true,
      read_ok:accel.read_ok===true,
      tilt_valid:accel.tilt_valid===true,
      raw_x:integer(accel.raw_x,-32768,32767),
      raw_y:integer(accel.raw_y,-32768,32767),
      raw_z:integer(accel.raw_z,-32768,32767),
      x_g:finite(accel.x_g,-16,16),
      y_g:finite(accel.y_g,-16,16),
      z_g:finite(accel.z_g,-16,16),
      roll_deg:finite(accel.roll_deg,-180,180),
      pitch_deg:finite(accel.pitch_deg,-180,180),
      sample_age_ms:integer(accel.sample_age_ms,0,3600000)
    }
  };

  const tokenHash=await digest(token),now=Math.floor(Date.now()/1000);
  const result=await env.DB.prepare("UPDATE device_links SET last_seen_at=?,telemetry_json=?,telemetry_at=? WHERE token_hash=? AND revoked_at IS NULL AND linked_at IS NOT NULL")
    .bind(now,JSON.stringify(telemetry),now,tokenHash).run();
  if(!(result.meta?.changes||0))return json({error:"Устройство не авторизовано или отключено"},401);
  return json({ok:true,server_time:now});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"POST"});}
