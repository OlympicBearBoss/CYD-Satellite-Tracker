import {json,readJson,currentUser,sameOrigin,digest} from "../auth/_shared.js";
export async function onRequestPost({request,env}) {
 if(!sameOrigin(request))return json({error:"Недопустимый источник запроса"},403);
 const user=await currentUser(request,env);if(!user)return json({error:"Требуется вход в аккаунт"},401);
 const body=await readJson(request)||{},deviceId=String(body.device_id||"");
 if(!["telemetry_refresh","stop"].includes(body.command))return json({error:"Команда пока не поддерживается"},400);
 const device=await env.DB.prepare("SELECT id FROM device_links WHERE id=? AND user_id=? AND linked_at IS NOT NULL AND revoked_at IS NULL").bind(deviceId,user.id).first();
 if(!device)return json({error:"Устройство не найдено или отключено"},404);
 const id=crypto.randomUUID(),now=Math.floor(Date.now()/1000);
 await env.DB.prepare("INSERT INTO device_commands(id,device_id,user_id,command,payload_json,created_at) VALUES(?,?,?,?,?,?)").bind(id,deviceId,user.id,body.command,"{}",now).run();
 return json({ok:true,command_id:id},201);
}
export async function onRequestGet({request,env}) {
 const auth=request.headers.get("Authorization")||"",token=auth.startsWith("Bearer ")?auth.slice(7).trim():"";
 if(token.length<32||token.length>256)return json({error:"Требуется токен устройства"},401);
 const hash=await digest(token),device=await env.DB.prepare("SELECT id FROM device_links WHERE token_hash=? AND revoked_at IS NULL AND linked_at IS NOT NULL").bind(hash).first();
 if(!device)return json({error:"Устройство не авторизовано"},401);
 const now=Math.floor(Date.now()/1000),rows=await env.DB.prepare("SELECT id,command,payload_json FROM device_commands WHERE device_id=? AND delivered_at IS NULL ORDER BY created_at LIMIT 5").bind(device.id).all(),commands=rows.results||[];
 for(const c of commands)await env.DB.prepare("UPDATE device_commands SET delivered_at=? WHERE id=? AND delivered_at IS NULL").bind(now,c.id).run();
 return json({commands:commands.map(c=>({id:c.id,command:c.command,payload:JSON.parse(c.payload_json)}))});
}
export async function onRequestPut({request,env}) {
 const auth=request.headers.get("Authorization")||"",token=auth.startsWith("Bearer ")?auth.slice(7).trim():"";
 if(token.length<32||token.length>256)return json({error:"Требуется токен устройства"},401);
 const body=await readJson(request)||{},hash=await digest(token),device=await env.DB.prepare("SELECT id FROM device_links WHERE token_hash=? AND revoked_at IS NULL AND linked_at IS NOT NULL").bind(hash).first();
 if(!device)return json({error:"Устройство не авторизовано"},401);
 const result=String(body.result||"").slice(0,240);
 const updated=await env.DB.prepare("UPDATE device_commands SET completed_at=?,result=? WHERE id=? AND device_id=? AND delivered_at IS NOT NULL AND completed_at IS NULL").bind(Math.floor(Date.now()/1000),result,String(body.id||""),device.id).run();
 return json({ok:!!(updated.meta?.changes||0)});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"GET, POST, PUT"});}
