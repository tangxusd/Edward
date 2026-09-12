import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const auth = req.headers.get("Authorization");
  if (!auth?.startsWith("Bearer ")) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: auth } } });
  const { data: user } = await client.auth.getUser();
  const body = await req.json().catch(() => ({}));
  if (!user.user || typeof body.resourceId !== "string") return Response.json({ error: "invalid_request" }, { status: 400, headers: cors });
  const table = client.from("resource_favorites");
  if (body.favorite === false) await table.delete().eq("user_id", user.user.id).eq("resource_id", body.resourceId);
  else await table.upsert({ user_id: user.user.id, resource_id: body.resourceId }, { onConflict: "user_id,resource_id" });
  const { count } = await client.from("resource_favorites").select("resource_id", { count: "exact", head: true }).eq("resource_id", body.resourceId);
  return Response.json({ resourceId: body.resourceId, favorite: body.favorite !== false, favoriteCount: count || 0 }, { headers: cors });
});
