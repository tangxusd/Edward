import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

export const cors = {
  "Access-Control-Allow-Origin": "http://127.0.0.1:7777",
  "Access-Control-Allow-Methods": "GET,OPTIONS",
  "Access-Control-Allow-Headers": "authorization, apikey, content-type",
};

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  if (req.method !== "GET") return Response.json({ error: "method_not_allowed" }, { status: 405, headers: cors });
  const authorization = req.headers.get("Authorization");
  if (!authorization?.startsWith("Bearer ")) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: authorization } } });
  const { data: auth } = await client.auth.getUser();
  if (!auth.user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const [{ data: profile, error: profileError }, { data: ledger, error: ledgerError }] = await Promise.all([
    client.from("profiles").select("referral_code").eq("user_id", auth.user.id).maybeSingle(),
    client.from("credit_ledger").select("amount,state,expires_at,created_at").eq("user_id", auth.user.id).order("created_at", { ascending: false }),
  ]);
  if (profileError || ledgerError || !profile) return Response.json({ error: "referral_unavailable" }, { status: 503, headers: cors });
  const availableCredit = (ledger || []).filter((entry) => entry.state === "available" && (!entry.expires_at || entry.expires_at > new Date().toISOString())).reduce((sum, entry) => sum + Number(entry.amount), 0);
  const code = String(profile.referral_code);
  const base = Deno.env.get("ORBIT_PUBLIC_BASE_URL") || "https://edward.uno";
  return Response.json({ code, inviteUrl: `${base}/signup?ref=${encodeURIComponent(code)}`, availableCredit, ledgerEntries: ledger || [] }, { headers: cors });
});
