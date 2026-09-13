import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { adminClient, rateCheck } from "../_shared/risk.ts";
const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
const usernameRe = /^[\p{L}\p{N}_-]{3,32}$/u;
Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  if (req.method !== "POST") return Response.json({ error: "method_not_allowed" }, { status: 405, headers: cors });
  try {
    const body = await req.json(); const email = String(body.email || "").trim().toLowerCase(); const password = String(body.password || ""); const username = String(body.username || email.split("@")[0] || "").trim();
    if (!email.includes("@") || password.length < 8 || !usernameRe.test(username)) return Response.json({ error: "invalid_registration" }, { status: 400, headers: cors });
    const risk = await rateCheck(`${req.headers.get("x-forwarded-for") || "unknown"}:${email}`, "register", 5); if (!risk.allowed) return Response.json({ error: "rate_limited" }, { status: 429, headers: cors });
    const admin = adminClient(); const normalized = username.toLocaleLowerCase();
    const { data: existing } = await admin.from("profiles").select("user_id").eq("username_normalized", normalized).maybeSingle(); if (existing) return Response.json({ error: "registration_unavailable" }, { status: 409, headers: cors });
    const anon = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!); const { data, error } = await anon.auth.signUp({ email, password, options: { emailRedirectTo: "http://127.0.0.1:7777/auth/confirmed" } });
    if (error || !data.user) return Response.json({ error: "registration_unavailable" }, { status: 400, headers: cors });
    const code = `${data.user.id.replaceAll("-", "").slice(0, 10)}${crypto.randomUUID().replaceAll("-", "").slice(0, 6)}`;
    const { error: profileError } = await admin.from("profiles").insert({ user_id: data.user.id, username_normalized: normalized, username_display: username, referral_code: code });
    if (profileError) { await admin.auth.admin.deleteUser(data.user.id); return Response.json({ error: "registration_unavailable" }, { status: 409, headers: cors }); }
    return Response.json({ userId: data.user.id, emailVerificationRequired: !data.session }, { status: 201, headers: cors });
  } catch { return Response.json({ error: "registration_unavailable" }, { status: 400, headers: cors }); }
});
