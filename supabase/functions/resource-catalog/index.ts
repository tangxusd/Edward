import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const cors = { "Access-Control-Allow-Origin": "*", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
const allowedTabs = new Set(["media", "text", "audio", "cards", "chart", "background", "annotation", "number"]);

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const auth = req.headers.get("Authorization");
  if (!auth?.startsWith("Bearer ")) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: auth } } });
  const { data: user } = await client.auth.getUser();
  if (!user.user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const url = new URL(req.url);
  const tabKey = url.searchParams.get("tabKey") || "";
  if (!allowedTabs.has(tabKey)) return Response.json({ error: "invalid_tab" }, { status: 400, headers: cors });
  const categoryId = url.searchParams.get("categoryId");
  const sort = url.searchParams.get("sort") === "popular" ? "popular" : url.searchParams.get("sort") === "favorites" ? "favorites" : "latest";
  const limit = Math.min(40, Math.max(1, Number(url.searchParams.get("limit") || 24)));
  const offset = Math.max(0, Number(url.searchParams.get("offset") || 0));
  let query = client.from("resources").select("id,component_id,tab_key,category_id,name,summary,favorite_count,view_count,created_at,published_at").eq("tab_key", tabKey).eq("status", "published").in("visibility", ["public", "unlisted"]);
  if (categoryId) query = query.eq("category_id", categoryId);
  if (sort === "popular" || sort === "favorites") query = query.order("favorite_count", { ascending: false }).order("id", { ascending: false });
  else query = query.order("published_at", { ascending: false, nullsFirst: false }).order("id", { ascending: false });
  const { data, error } = await query.range(offset, offset + limit - 1);
  if (error) return Response.json({ error: "catalog_unavailable" }, { status: 502, headers: cors });
  return Response.json({ items: data || [], nextOffset: (data?.length || 0) === limit ? offset + limit : null }, { headers: { ...cors, "Cache-Control": "private, max-age=30" } });
});
