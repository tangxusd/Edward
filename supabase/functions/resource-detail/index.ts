import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { hasActiveEntitlement } from "../_shared/billing.ts";

const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const auth = req.headers.get("Authorization");
  if (!auth?.startsWith("Bearer ")) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: auth } } });
  const { data: user } = await client.auth.getUser();
  const url = new URL(req.url);
  const resourceId = url.searchParams.get("resourceId") || url.searchParams.get("id");
  const requestedVersion = url.searchParams.get("version");
  const requestedHash = url.searchParams.get("contentHash");
  if (!user.user || !resourceId) return Response.json({ error: "invalid_request" }, { status: 400, headers: cors });
  if (!await hasActiveEntitlement(client, user.user.id)) return Response.json({ error: "entitlement_required" }, { status: 403, headers: cors });

  const { data: resource, error: resourceError } = await client.from("resources").select("id,component_id,target,tab_key,category_id,name,detail_markdown,required_plan,status,visibility,created_at,updated_at").eq("id", resourceId).eq("status", "published").maybeSingle();
  if (resourceError || !resource) return Response.json({ error: "resource_not_found" }, { status: 404, headers: cors });

  let versionQuery = client.from("resource_versions").select("id,version,content_hash,manifest_path,package_path,preview_image_path,preview_video_path,file_size,mime_type,min_app_version,compatibility").eq("resource_id", resourceId).not("published_at", "is", null);
  if (requestedVersion) versionQuery = versionQuery.eq("version", requestedVersion);
  if (requestedHash) versionQuery = versionQuery.eq("content_hash", requestedHash);
  const { data: version, error: versionError } = await versionQuery.order("published_at", { ascending: false }).limit(1).maybeSingle();
  if (versionError || !version?.package_path || !version.preview_video_path || !version.manifest_path) return Response.json({ error: "resource_version_not_found" }, { status: 404, headers: cors });

  const bucket = client.storage.from("resource-packages");
  const [packageSigned, previewSigned, manifestSigned] = await Promise.all([
    bucket.createSignedUrl(version.package_path, 60),
    bucket.createSignedUrl(version.preview_video_path, 60),
    bucket.createSignedUrl(version.manifest_path, 60),
  ]);
  if (!packageSigned.data?.signedUrl || !previewSigned.data?.signedUrl || !manifestSigned.data?.signedUrl) return Response.json({ error: "resource_download_unavailable" }, { status: 502, headers: cors });
  return Response.json({ resource, version, packageUrl: packageSigned.data.signedUrl, previewUrl: previewSigned.data.signedUrl, manifestUrl: manifestSigned.data.signedUrl }, { headers: { ...cors, "Cache-Control": "private, no-store" } });
});
