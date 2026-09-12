import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { hasActiveEntitlement } from "../_shared/billing.ts";

const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
const allowedTabs = new Set(["media", "text", "audio", "cards", "chart", "background", "annotation", "number"]);

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const auth = req.headers.get("Authorization");
  if (!auth?.startsWith("Bearer ")) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: auth } } });
  const { data: user } = await client.auth.getUser();
  if (!user.user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  if (!await hasActiveEntitlement(client, user.user.id)) return Response.json({ error: "entitlement_required" }, { status: 403, headers: cors });
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
  const items: Array<Record<string, unknown>> = (data || []).map((item) => ({ ...item }));
  const ids = items.map((item) => item.id);
  if (ids.length) {
    const { data: versions } = await client.from("resource_versions").select("resource_id,preview_video_path").in("resource_id", ids).not("preview_video_path", "is", null).not("published_at", "is", null).order("published_at", { ascending: false });
    const latest = new Map();
    for (const version of versions || []) if (!latest.has(version.resource_id)) latest.set(version.resource_id, version.preview_video_path);
    await Promise.all(items.map(async (item) => {
      const objectPath = latest.get(item.id);
      if (!objectPath) return;
      const { data: signed } = await client.storage.from("resource-packages").createSignedUrl(objectPath, 60);
      if (signed?.signedUrl) item.preview_url = signed.signedUrl;
    }));
  }
  return Response.json({ items, nextOffset: items.length === limit ? offset + limit : null }, { headers: { ...cors, "Cache-Control": "private, max-age=30" } });
});
