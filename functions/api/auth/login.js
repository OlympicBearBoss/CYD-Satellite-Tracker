import {json,readJson,digest,passwordHash,randomId,sessionCookie,sameOrigin} from "./_shared.js";
export async function onRequestPost({request,env}) {
 if(!sameOrigin(request))return json({error:"Недопустимый источник запроса"},403);
 if(!env?.DB)return json({error:"Авторизация временно недоступна: база данных ещё не подключена"},503);
 const body=await readJson(request),email=String(body?.email||"").trim().toLowerCase(),password=String(body?.password||"");
 if(!email||!password||password.length>128)return json({error:"Проверьте email и пароль"},400);
 const row=await env.DB.prepare("SELECT u.id,u.email,u.display_name,u.avatar_url,u.email_verified,u.role,u.status,i.password_hash,i.password_salt FROM users u JOIN identities i ON i.user_id=u.id AND i.provider='password' WHERE u.email=?").bind(email).first();
 if(!row||row.status!=="active")return json({error:"Неверный email или пароль"},401);
 const actual=await passwordHash(password,row.password_salt);if(actual!==row.password_hash)return json({error:"Неверный email или пароль"},401);
 const now=Math.floor(Date.now()/1000),token=randomId()+randomId();
 await env.DB.prepare("INSERT INTO sessions(id_hash,user_id,expires_at,created_at,last_seen_at,user_agent) VALUES(?,?,?,?,?,?)").bind(await digest(token),row.id,now+2592000,now,now,(request.headers.get("User-Agent")||"").slice(0,300)).run();
 delete row.password_hash;delete row.password_salt;const mobile=body?.client==="mobile";return json(mobile?{user:row,session_token:token}:{user:row},200,{"Set-Cookie":sessionCookie(token)});
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"POST"});}
