import {json,cookie,digest,clearSessionCookie,sameOrigin} from "./_shared.js";
export async function onRequestPost({request,env}) {
  if(!sameOrigin(request)) return json({error:"Недопустимый источник запроса"},403);
  const auth=request.headers.get("Authorization")||""; const bearer=auth.startsWith("Bearer ")?auth.slice(7).trim():""; const token=bearer||cookie(request,"cyd_session");
  if(token) await env.DB.prepare("DELETE FROM sessions WHERE id_hash=?").bind(await digest(token)).run();
  return json({ok:true},200,{"Set-Cookie":clearSessionCookie});
}
export function onRequest({request}) { return json({error:"Method not allowed"},405,{Allow:"POST"}); }
