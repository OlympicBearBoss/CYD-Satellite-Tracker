import {json,readJson,currentUser,sameOrigin} from "../auth/_shared.js";
export async function onRequestGet({request,env}) {
  const user=await currentUser(request,env);
  return user?json({user}):json({error:"Требуется вход в аккаунт"},401);
}
export async function onRequestPost({request,env}) {
  if(!sameOrigin(request))return json({error:"Недопустимый источник запроса"},403);
  const user=await currentUser(request,env);if(!user)return json({error:"Требуется вход в аккаунт"},401);
  const body=await readJson(request);if(!body)return json({error:"Некорректные данные"},400);
  const name=String(body.display_name??"").trim();
  const avatar=String(body.avatar_url??"").trim();
  if(name.length>80)return json({error:"Имя должно быть не длиннее 80 символов"},400);
  if(avatar.length>500)return json({error:"Ссылка на аватар слишком длинная"},400);
  if(avatar){try{const u=new URL(avatar);if(u.protocol!=="https:")throw new Error();}catch{return json({error:"Для аватара укажите корректную HTTPS-ссылку"},400);}}
  const now=Math.floor(Date.now()/1000);
  await env.DB.prepare("UPDATE users SET display_name=?,avatar_url=?,updated_at=? WHERE id=?").bind(name,avatar||null,now,user.id).run();
  const updated=await env.DB.prepare("SELECT id,email,display_name,avatar_url,email_verified,role,status FROM users WHERE id=?").bind(user.id).first();
  return json({user:updated});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"GET, POST"});}
