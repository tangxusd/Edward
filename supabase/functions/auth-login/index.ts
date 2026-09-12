import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { rateCheck } from "../_shared/risk.ts";
const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  try {
    const body = await req.json(); const identifier = String(body.identifier || body.email || "").trim(); const password = String(body.password || "");
    if (!identifier || !password) return Response.json({ error: "invalid_credentials" }, { status: 400, headers: cors });
    const risk = await rateCheck(`${req.headers.get("x-forwarded-for") || "unknown"}:${identifier}`, "login", 10); if (!risk.allowed) return Response.json({ error: "rate_limited" }, { status: 429, headers: cors });
    let email = identifier.toLowerCase();
    if (!email.includes("@")) { const admin = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!); const { data: profile } = await admin.from("profiles").select("user_id").eq("username_normalized", email).maybeSingle(); if (!profile) return Response.json({ error: "invalid_credentials" }, { status: 401, headers: cors }); const { data: user } = await admin.auth.admin.getUserById(profile.user_id); email = user.user?.email || ""; }
    const anon = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!); const { data, error } = await anon.auth.signInWithPassword({ email, password });
    if (error || !data.session) return Response.json({ error: "invalid_credentials" }, { status: 401, headers: cors });
    return Response.json({ access_token: data.session.access_token, refresh_token: data.session.refresh_token, expires_in: data.session.expires_in, user: data.user }, { headers: cors });
  } catch { return Response.json({ error: "invalid_credentials" }, { status: 401, headers: cors }); }
});
