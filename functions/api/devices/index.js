import {json,readJson,currentUser,sameOrigin,randomId,digest} from "../auth/_shared.js";
const makeCode=()=>String(crypto.getRandomValues(new Uint32Array(1))[0]%100000000).padStart(8,"0");
export async function onRequestGet({request,env}) {
  const user=await currentUser(request,env);if(!user)return json({error:"Требуется вход в аккаунт"},401);
  const result=await env.DB.prepare("SELECT id,device_name,platform,linked_at,last_seen_at,revoked_at,code_expires_at FROM device_links WHERE user_id=? ORDER BY created_at DESC").bind(user.id).all();
  return json({devices:result.results||[]});
}
export async function onRequestPost({request,env}) {
  if(!sameOrigin(request))return json({error:"Недопустимый источник запроса"},403);
  const user=await currentUser(request,env);if(!user)return json({error:"Требуется вход в аккаунт"},401);
  const body=await readJson(request),name=String(body?.device_name||"CYD ESP32").trim().slice(0,80)||"CYD ESP32";
  const now=Math.floor(Date.now()/1000),id=randomId();
  let code="",hash="";
  for(let i=0;i<4;i++){code=makeCode();hash=await digest(code);const exists=await env.DB.prepare("SELECT id FROM device_links WHERE code_hash=?").bind(hash).first();if(!exists)break;code="";}
  if(!code)return json({error:"Не удалось создать код. Попробуйте ещё раз."},503);
  await env.DB.prepare("INSERT INTO device_links(id,user_id,device_name,platform,code_hash,code_expires_at,created_at) VALUES(?,?,?,?,?,?,?)").bind(id,user.id,name,"esp32-cyd",hash,now+600,now).run();
  return json({ok:true,device_id:id,code,expires_at:now+600,expires_in:600},201);
}
export async function onRequestDelete({request,env}) {
  if(!sameOrigin(request))return json({error:"Недопустимый источник запроса"},403);
  const user=await currentUser(request,env);if(!user)return json({error:"Требуется вход в аккаунт"},401);
  const id=new URL(request.url).searchParams.get("id")||"";
  if(!id||id.length>80)return json({error:"Некорректное устройство"},400);
  const result=await env.DB.prepare("UPDATE device_links SET revoked_at=?,code_hash=NULL,code_expires_at=NULL,token_hash=NULL WHERE id=? AND user_id=? AND revoked_at IS NULL").bind(Math.floor(Date.now()/1000),id,user.id).run();
  if(!(result.meta?.changes||0))return json({error:"Устройство не найдено или уже отключено"},404);
  return json({ok:true});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"GET, POST, DELETE"});}
