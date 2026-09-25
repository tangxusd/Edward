import { createClient, type SupabaseClient } from "https://esm.sh/@supabase/supabase-js@2";
import { assertRequestId, assertSha256 } from "./orbit_contract.ts";

export function adminClient(): SupabaseClient {
  const url = Deno.env.get("SUPABASE_URL");
  const key = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY");
  if (!url || !key) throw new Error("supabase_admin_not_configured");
  return createClient(url, key);
}

export async function redeemCodeOnce(client: SupabaseClient, requestId: string, codeDigest: string, userId: string) {
  assertRequestId(requestId);
  assertSha256(codeDigest);
  const { data, error } = await client.rpc("redeem_code_once", {
    p_request_id: requestId,
    p_code_digest: codeDigest,
    p_user_id: userId,
  });
  if (error) throw new Error(error.message || "redemption_failed");
  return data;
}

export async function catalogManifest(client: SupabaseClient, catalogType: "color" | "font", knownRevision: string | null) {
  const { data, error } = await client.rpc("published_catalog_manifest", {
    catalog_type: catalogType,
    known_revision: knownRevision,
  });
  if (error) throw new Error(error.message || "catalog_unavailable");
  return data;
}
