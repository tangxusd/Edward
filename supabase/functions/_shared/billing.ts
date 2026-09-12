import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
export const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
export function userClient(req: Request) { const auth = req.headers.get("Authorization"); if (!auth?.startsWith("Bearer ")) return null; return createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: auth } } }); }
export const adminClient = () => createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!);
export async function requireUser(req: Request) { const client = userClient(req); if (!client) return { client: null, user: null }; const { data } = await client.auth.getUser(); return { client, user: data.user || null }; }
export async function hasActiveEntitlement(client: ReturnType<typeof userClient>, userId: string) {
  if (!client) return false;
  const now = new Date().toISOString();
  const [trial, subscription] = await Promise.all([
    client.from("trial_grants").select("user_id").eq("user_id", userId).eq("status", "active").gt("ends_at", now).maybeSingle(),
    client.from("subscriptions").select("user_id").eq("user_id", userId).in("status", ["trialing", "active", "past_due"]).gt("current_period_end", now).maybeSingle(),
  ]);
  return Boolean(trial.data || subscription.data);
}
