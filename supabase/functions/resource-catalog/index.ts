import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { hasActiveEntitlement } from "../_shared/billing.ts";

const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
const allowedTabs = new Set(["media", "text", "audio", "cards", "chart", "background", "annotation", "number"]);
const allowedFilters = new Set(["favorites", "latest", "popular"]);
const resourceFields = "id,component_id,target,tab_key,category_id,name,favorite_count,view_count,created_at,published_at";

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
  const categoryId = url.searchParams.get("categoryId") || null;
  const requestedFilter = url.searchParams.get("filter") || url.searchParams.get("sort") || "latest";
  const filter = allowedFilters.has(requestedFilter) ? requestedFilter : "latest";
  const limit = Math.min(40, Math.max(1, Number(url.searchParams.get("limit") || 24)));
  const offset = Math.max(0, Number(url.searchParams.get("offset") || 0));

  let data: Array<Record<string, unknown>> | null = null;
  let error: { message: string } | null = null;
  if (filter === "favorites") {
    ({ data, error } = await client.rpc("list_favorite_resources", { p_tab_key: tabKey, p_category_id: categoryId, p_limit: limit, p_offset: offset }));
  } else {
    let query = client.from("resources").select(resourceFields).eq("tab_key", tabKey).eq("status", "published").in("visibility", ["public", "unlisted"]);
    if (categoryId) query = query.eq("category_id", categoryId);
    if (filter === "popular") query = query.order("favorite_count", { ascending: false }).order("published_at", { ascending: false, nullsFirst: false }).order("id", { ascending: false });
    else query = query.order("published_at", { ascending: false, nullsFirst: false }).order("id", { ascending: false });
    ({ data, error } = await query.range(offset, offset + limit - 1));
  }
  if (error) return Response.json({ error: "catalog_unavailable" }, { status: 502, headers: cors });

  const items: Array<Record<string, unknown>> = (data || []).map((item) => ({ ...item, is_favorite: filter === "favorites" }));
  const ids = items.map((item) => String(item.id));
  if (ids.length) {
    if (filter !== "favorites") {
      const { data: favorites } = await client.from("resource_favorites").select("resource_id").eq("user_id", user.user.id).in("resource_id", ids);
      const favoriteIds = new Set((favorites || []).map((favorite) => favorite.resource_id));
      for (const item of items) item.is_favorite = favoriteIds.has(String(item.id));
    }
    const { data: versions } = await client.from("resource_versions").select("resource_id,version,content_hash,preview_video_path,published_at").in("resource_id", ids).not("preview_video_path", "is", null).not("published_at", "is", null).order("published_at", { ascending: false });
    const latest = new Map<string, Record<string, unknown>>();
    for (const version of versions || []) if (!latest.has(version.resource_id)) latest.set(version.resource_id, version);
    await Promise.all(items.map(async (item) => {
      const version = latest.get(String(item.id));
      if (!version) return;
      item.version = version.version;
      item.content_hash = version.content_hash;
      const { data: signed } = await client.storage.from("resource-packages").createSignedUrl(String(version.preview_video_path), 60);
      if (signed?.signedUrl) item.preview_url = signed.signedUrl;
    }));
  }
  return Response.json({ items, nextOffset: items.length === limit ? offset + limit : null }, { headers: { ...cors, "Cache-Control": "private, max-age=30" } });
});
