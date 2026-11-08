import {json,readJson,randomId,digest,passwordHash,sessionCookie,sameOrigin} from "./_shared.js";
export async function onRequestPost({request,env}) {
  if(!sameOrigin(request)) return json({error:"Недопустимый источник запроса"},403);
  const body=await readJson(request), email=String(body?.email||"").trim().toLowerCase(), password=String(body?.password||""), name=String(body?.name||"").trim().slice(0,80);
  if(!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)||email.length>254) return json({error:"Укажите корректный email"},400);
  if(password.length<12||password.length>128) return json({error:"Пароль должен содержать от 12 до 128 символов"},400);
  const now=Math.floor(Date.now()/1000), userId=randomId(), salt=randomId(), token=randomId()+randomId(), expires=now+2592000;
  try {
    const hash=await passwordHash(password,salt);
    await env.DB.batch([
      env.DB.prepare("INSERT INTO users(id,email,display_name,email_verified,created_at,updated_at) VALUES(?,?,?,?,?,?)").bind(userId,email,name,0,now,now),
      env.DB.prepare("INSERT INTO identities(id,user_id,provider,provider_subject,password_hash,password_salt,created_at) VALUES(?,?,?,?,?,?,?)").bind(randomId(),userId,"password",email,hash,salt,now),
      env.DB.prepare("INSERT INTO sessions(id_hash,user_id,expires_at,created_at,last_seen_at,user_agent) VALUES(?,?,?,?,?,?)").bind(await digest(token),userId,expires,now,now,(request.headers.get("User-Agent")||"").slice(0,300))
    ]);
    return json({user:{id:userId,email,display_name:name,email_verified:false}},201,{"Set-Cookie":sessionCookie(token)});
  } catch(error) {
    if(String(error?.message||"").includes("UNIQUE")) return json({error:"Аккаунт с таким email уже существует"},409);
    return json({error:"Не удалось создать аккаунт"},500);
  }
}
export function onRequest({request}) { return json({error:"Method not allowed"},405,{Allow:"POST"}); }
