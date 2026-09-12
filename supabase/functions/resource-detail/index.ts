import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
const cors = { "Access-Control-Allow-Origin": "*", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const auth = req.headers.get("Authorization");
  if (!auth?.startsWith("Bearer ")) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: auth } } });
  const { data: user } = await client.auth.getUser();
  const url = new URL(req.url), id = url.searchParams.get("resourceId");
  if (!user.user || !id) return Response.json({ error: "invalid_request" }, { status: 400, headers: cors });
  const { data, error } = await client.from("resources").select("id,component_id,tab_key,category_id,name,summary,detail_markdown,required_plan,status,visibility,created_at,updated_at").eq("id", id).eq("status", "published").maybeSingle();
  if (error || !data) return Response.json({ error: "resource_not_found" }, { status: 404, headers: cors });
  const { data: version } = await client.from("resource_versions").select("id,version,content_hash,manifest_path,package_path,preview_image_path,preview_video_path,file_size,mime_type,min_app_version,compatibility").eq("resource_id", id).not("published_at", "is", null).order("published_at", { ascending: false }).limit(1).maybeSingle();
  return Response.json({ resource: data, version }, { headers: { ...cors, "Cache-Control": "private, max-age=30" } });
});
