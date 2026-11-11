import {json,currentUser} from "../auth/_shared.js";
export async function onRequestGet({request,env}) {
 const user=await currentUser(request,env);if(!user)return json({error:"Требуется вход в аккаунт"},401);
 const rows=await env.DB.prepare("SELECT id,device_name,last_seen_at,telemetry_json,telemetry_at,revoked_at FROM device_links WHERE user_id=? AND linked_at IS NOT NULL ORDER BY created_at DESC").bind(user.id).all();
 const now=Math.floor(Date.now()/1000);
 return json({devices:(rows.results||[]).map(d=>({id:d.id,device_name:d.device_name,status:d.revoked_at?"revoked":d.last_seen_at&&now-Number(d.last_seen_at)<=180?"online":"offline",last_seen_at:d.last_seen_at,telemetry_at:d.telemetry_at,telemetry:d.telemetry_json?JSON.parse(d.telemetry_json):null}))});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"GET"});}
