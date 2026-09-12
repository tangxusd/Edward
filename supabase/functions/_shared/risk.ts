import { createHash } from "node:crypto";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

export const adminClient = () => createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!);
export const hashIdentifier = (value: string) => createHash("sha256").update(`${Deno.env.get("RISK_HASH_SALT") || "edward-risk"}:${value.toLowerCase()}`).digest("hex");
export async function rateCheck(identifier: string, type: string, limit = 10) {
  const admin = adminClient();
  const hash = hashIdentifier(identifier);
  const since = new Date(Date.now() - 15 * 60_000).toISOString();
  const { count } = await admin.from("risk_events").select("id", { count: "exact", head: true }).eq("event_type", type).eq("identifier_hash", hash).gte("occurred_at", since);
  if ((count || 0) >= limit) return { allowed: false, hash };
  await admin.from("risk_events").insert({ event_type: type, identifier_hash: hash });
  return { allowed: true, hash };
}
