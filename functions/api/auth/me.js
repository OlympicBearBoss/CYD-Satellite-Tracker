import {json,currentUser} from "./_shared.js";
export async function onRequestGet({request,env}) {
  const user=await currentUser(request,env);
  return user?json({user}):json({user:null},401);
}
export function onRequest({request}) { return json({error:"Method not allowed"},405,{Allow:"GET"}); }
