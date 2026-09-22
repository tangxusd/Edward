import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { authCors } from "../_shared/auth-lifecycle.ts";
import { adminClient, rateCheck } from "../_shared/risk.ts";
Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: authCors });
  try {
    const body = await req.json(); const identifier = String(body.identifier || body.email || "").trim(); const password = String(body.password || "");
    if (!identifier || !password) return Response.json({ code: "invalid_credentials", message: "邮箱或密码不正确。" }, { status: 401, headers: authCors });
    const clientAddress = req.headers.get("cf-connecting-ip") || req.headers.get("x-real-ip") || "unknown";
    const risk = await rateCheck(`${clientAddress}:${identifier}`, "login", 10); if (!risk.allowed) return Response.json({ code: "rate_limited", message: "登录请求过于频繁，请稍后重试。" }, { status: 429, headers: authCors });
    let email = identifier.toLowerCase();
    const admin = adminClient();
    if (!email.includes("@")) {
      const { data: profile } = await admin.from("profiles").select("user_id").eq("username_normalized", email).maybeSingle();
      if (!profile) return Response.json({ code: "invalid_credentials", message: "邮箱或密码不正确。" }, { status: 401, headers: authCors });
      const { data: user } = await admin.auth.admin.getUserById(profile.user_id); email = user.user?.email || "";
    }
    const { data: registration } = await admin.from("auth_registration_states").select("user_id").eq("email_normalized", email).maybeSingle();
    if (!registration?.user_id) return Response.json({ code: "invalid_credentials", message: "邮箱或密码不正确。" }, { status: 401, headers: authCors });
    const { data: gate } = await admin.rpc("auth_login_gate", { p_user_id: registration.user_id });
    if (gate === "registration_pending") return Response.json({ code: "confirmation_pending", message: "确认邮件已发送，请在 10 分钟内确认完成。" }, { status: 403, headers: authCors });
    if (gate === "recovery_locked") return Response.json({ code: "recovery_locked", message: "密码重置进行中，请通过邮件链接完成重设后再登录。" }, { status: 423, headers: authCors });
    if (gate !== "active") return Response.json({ code: "invalid_credentials", message: "邮箱或密码不正确。" }, { status: 401, headers: authCors });
    const anon = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!); const { data, error } = await anon.auth.signInWithPassword({ email, password });
    if (error || !data.session) return Response.json({ code: "invalid_credentials", message: "邮箱或密码不正确。" }, { status: 401, headers: authCors });
    return Response.json({ access_token: data.session.access_token, refresh_token: data.session.refresh_token, expires_in: data.session.expires_in, user: data.user, message: "登录成功。" }, { headers: authCors });
  } catch { return Response.json({ code: "invalid_credentials", message: "邮箱或密码不正确。" }, { status: 401, headers: authCors }); }
});
