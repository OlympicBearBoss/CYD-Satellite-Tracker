import {json,digest} from "../auth/_shared.js";
export async function onRequestPost({request,env}) {
  const auth=request.headers.get("Authorization")||"",token=auth.startsWith("Bearer ")?auth.slice(7).trim():"";
  if(token.length<32||token.length>256)return json({error:"Требуется токен устройства"},401);
  const tokenHash=await digest(token),now=Math.floor(Date.now()/1000);
  const result=await env.DB.prepare("UPDATE device_links SET last_seen_at=? WHERE token_hash=? AND revoked_at IS NULL AND linked_at IS NOT NULL").bind(now,tokenHash).run();
  if(!(result.meta?.changes||0))return json({error:"Устройство не авторизовано или отключено"},401);
  return json({ok:true,server_time:now});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"POST"});}
