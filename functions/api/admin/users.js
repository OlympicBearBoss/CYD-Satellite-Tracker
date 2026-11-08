import {json,requireAdmin} from "../auth/_shared.js";
export async function onRequestGet({request,env}){
 const gate=await requireAdmin(request,env);if(gate.response)return gate.response;
 try{
  const [u,d,a]=await Promise.all([
   env.DB.prepare("SELECT u.id,u.email,u.display_name,u.avatar_url,u.role,u.status,u.email_verified,u.created_at,u.updated_at,(SELECT MAX(last_seen_at) FROM sessions s WHERE s.user_id=u.id) AS last_seen_at,(SELECT COUNT(*) FROM sessions s WHERE s.user_id=u.id AND s.expires_at>?) AS active_sessions FROM users u ORDER BY u.created_at DESC LIMIT 500").bind(Math.floor(Date.now()/1000)).all(),
   env.DB.prepare("SELECT d.id,d.user_id,d.device_name,d.platform,d.linked_at,d.last_seen_at,d.revoked_at,u.email FROM device_links d JOIN users u ON u.id=d.user_id ORDER BY d.created_at DESC LIMIT 500").all(),
   env.DB.prepare("SELECT id,actor_user_id,target_user_id,action,details,created_at FROM admin_audit ORDER BY created_at DESC LIMIT 100").all()
  ]);
  return json({users:u.results||[],devices:d.results||[],audit:a.results||[]});
 }catch(e){return json({error:"Не удалось загрузить данные администрирования. Проверьте миграции D1."},503);}
}
export function onRequest({request}){return json({error:"Method not allowed"},405,{Allow:"GET"});}
