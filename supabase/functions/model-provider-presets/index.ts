import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  if (req.method !== "GET") return Response.json({ error: "method_not_allowed" }, { status: 405, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!);
  const result = await client.from("model_provider_presets")
    .select("id,name,endpoint,default_model").eq("is_active", true).order("sort_order");
  if (result.error) return Response.json({ error: "provider_presets_unavailable" }, { status: 503, headers: cors });
  return Response.json({ presets: result.data || [] }, { headers: { ...cors, "Cache-Control": "public, max-age=300" } });
});
