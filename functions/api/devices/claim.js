import {json,readJson,randomId,digest} from "../auth/_shared.js";
const tokenValue=()=>{const bytes=crypto.getRandomValues(new Uint8Array(32));return btoa(String.fromCharCode(...bytes)).replace(/\+/g,"-").replace(/\//g,"_").replace(/=+$/,"");};
export async function onRequestPost({request,env}) {
  const body=await readJson(request),code=String(body?.code||"").replace(/\D/g,""),deviceName=String(body?.device_name||"CYD ESP32").trim().slice(0,80)||"CYD ESP32";
  if(code.length!==8)return json({error:"Введите восьмизначный код привязки"},400);
  const now=Math.floor(Date.now()/1000),codeHash=await digest(code);
  const row=await env.DB.prepare("SELECT id,user_id FROM device_links WHERE code_hash=? AND code_expires_at>? AND linked_at IS NULL AND revoked_at IS NULL").bind(codeHash,now).first();
  if(!row)return json({error:"Код недействителен или срок его действия истёк"},400);
  const token=tokenValue(),tokenHash=await digest(token);
  const result=await env.DB.prepare("UPDATE device_links SET device_name=?,linked_at=?,last_seen_at=?,code_hash=NULL,code_expires_at=NULL,token_hash=? WHERE id=? AND code_hash=? AND linked_at IS NULL AND revoked_at IS NULL").bind(deviceName,now,now,tokenHash,row.id,codeHash).run();
  if(!(result.meta?.changes||0))return json({error:"Код уже использован. Создайте новый."},409);
  return json({ok:true,device_id:row.id,device_token:token});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"POST"});}
