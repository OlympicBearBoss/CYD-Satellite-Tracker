import {json,readJson,requireAdmin,sameOrigin,audit} from "../auth/_shared.js";
export async function onRequestPost({request,env}){
 if(!sameOrigin(request))return json({error:"Недопустимый источник запроса"},403);
 const gate=await requireAdmin(request,env);if(gate.response)return gate.response;
 const body=await readJson(request),id=String(body?.userId||""),action=String(body?.action||"");
 if(!id||id.length>80)return json({error:"Некорректный пользователь"},400);
 const target=await env.DB.prepare("SELECT id,role,status,email FROM users WHERE id=?").bind(id).first();
 if(!target)return json({error:"Пользователь не найден"},404);
 if(action==="block"||action==="unblock"){
  const status=action==="block"?"blocked":"active";
  if(id===gate.user.id&&status==="blocked")return json({error:"Нельзя заблокировать собственный аккаунт"},400);
  if(action==="block"&&target.role==="admin"){const n=await env.DB.prepare("SELECT COUNT(*) AS n FROM users WHERE role='admin' AND status='active'").first();if((n?.n||0)<=1)return json({error:"Нельзя заблокировать последнего активного администратора"},400);}
  await env.DB.prepare("UPDATE users SET status=?,updated_at=? WHERE id=?").bind(status,Math.floor(Date.now()/1000),id).run();
  if(status==="blocked")await env.DB.prepare("DELETE FROM sessions WHERE user_id=?").bind(id).run();
  await audit(env,gate.user.id,id,action);return json({ok:true});
 }
 if(action==="role"){
  const role=body.role==="admin"?"admin":"user";
  if(id===gate.user.id&&role!=="admin")return json({error:"Нельзя снять собственную роль администратора"},400);
  if(target.role==="admin"&&role!=="admin"){const n=await env.DB.prepare("SELECT COUNT(*) AS n FROM users WHERE role='admin'").first();if((n?.n||0)<=1)return json({error:"Нельзя снять роль последнего администратора"},400);}
  await env.DB.prepare("UPDATE users SET role=?,updated_at=? WHERE id=?").bind(role,Math.floor(Date.now()/1000),id).run();
  await audit(env,gate.user.id,id,"role",{role});return json({ok:true});
 }
 if(action==="device_revoke"){
  const deviceId=String(body.deviceId||"");
  if(!deviceId||deviceId.length>80)return json({error:"Некорректное устройство"},400);
  const result=await env.DB.prepare("UPDATE device_links SET revoked_at=?,code_hash=NULL,code_expires_at=NULL,token_hash=NULL WHERE id=? AND revoked_at IS NULL").bind(Math.floor(Date.now()/1000),deviceId).run();
  if(!(result.meta?.changes||0))return json({error:"Устройство не найдено или уже отключено"},404);
  await audit(env,gate.user.id,null,"device_revoke",{device_id:deviceId});
  return json({ok:true});
 }
 if(action==="sessions"){const r=await env.DB.prepare("DELETE FROM sessions WHERE user_id=?").bind(id).run();await audit(env,gate.user.id,id,"revoke_sessions");return json({ok:true,deleted:r.meta?.changes||0});}
 if(action==="delete"){
  if(id===gate.user.id)return json({error:"Нельзя удалить собственный аккаунт"},400);
  if(target.role==="admin"){const n=await env.DB.prepare("SELECT COUNT(*) AS n FROM users WHERE role='admin'").first();if((n?.n||0)<=1)return json({error:"Нельзя удалить последнего администратора"},400);}
  await audit(env,gate.user.id,id,"delete",{email:target.email});await env.DB.prepare("DELETE FROM users WHERE id=?").bind(id).run();return json({ok:true});
 }
 return json({error:"Неизвестное действие"},400);
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"POST"});}
