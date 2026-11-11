import {json,digest,readJson} from "../auth/_shared.js";
export async function onRequestPost({request,env}) {
  const auth=request.headers.get("Authorization")||"",token=auth.startsWith("Bearer ")?auth.slice(7).trim():"";
  if(token.length<32||token.length>256)return json({error:"Требуется токен устройства"},401);
  const body=await readJson(request)||{};
  const telemetry={
    wifi_rssi:Number.isFinite(body.wifi_rssi)?Math.max(-127,Math.min(0,body.wifi_rssi)):null,
    ip:String(body.ip||"").slice(0,48),gps_valid:body.gps_valid===true,
    latitude:Number.isFinite(body.latitude)?Math.max(-90,Math.min(90,body.latitude)):null,
    longitude:Number.isFinite(body.longitude)?Math.max(-180,Math.min(180,body.longitude)):null,
    altitude:Number.isFinite(body.altitude)?Math.max(-1000,Math.min(100000,body.altitude)):null,
    gps_satellites:Number.isFinite(body.gps_satellites)?Math.max(0,Math.min(99,Math.round(body.gps_satellites))):null,
    firmware:String(body.firmware||"").slice(0,40)
  };
  const tokenHash=await digest(token),now=Math.floor(Date.now()/1000);
  const result=await env.DB.prepare("UPDATE device_links SET last_seen_at=?,telemetry_json=?,telemetry_at=? WHERE token_hash=? AND revoked_at IS NULL AND linked_at IS NOT NULL").bind(now,JSON.stringify(telemetry),now,tokenHash).run();
  if(!(result.meta?.changes||0))return json({error:"Устройство не авторизовано или отключено"},401);
  return json({ok:true,server_time:now});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"POST"});}
