import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { authCors, deviceFingerprint, encryptDeviceValue, normalizeDeviceEvidence } from "../_shared/auth-lifecycle.ts";
import { adminClient, rateCheck } from "../_shared/risk.ts";

const success = () => Response.json({ code: "recovery_sent", message: "如该邮箱可使用，重置链接已发送，请检查邮箱。" }, { headers: authCors });

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: authCors });
  if (req.method !== "POST") return Response.json({ code: "method_not_allowed", message: "请求方式不正确。" }, { status: 405, headers: authCors });
  try {
    const body = await req.json();
    const email = String(body.email || "").trim().toLowerCase();
    const evidence = normalizeDeviceEvidence({ serial: String(body.deviceSerial || ""), mac: String(body.deviceMac || "") });
    if (!email.includes("@")) return success();
    if (!evidence) return Response.json({ code: "invalid_device", message: "无法读取设备标识，请检查系统权限后重试。" }, { status: 400, headers: authCors });
    const clientAddress = req.headers.get("cf-connecting-ip") || req.headers.get("x-real-ip") || "unknown";
    const risk = await rateCheck(`${clientAddress}:${email}`, "password-recovery", 5);
    if (!risk.allowed) return Response.json({ code: "rate_limited", message: "请求过于频繁，请稍后重试。" }, { status: 429, headers: authCors });
    const admin = adminClient();
    const { data: account } = await admin.from("auth_registration_states")
      .select("id,user_id,device_fingerprint,state").eq("email_normalized", email).maybeSingle();
    const fingerprint = await deviceFingerprint(evidence);
    if (!account?.user_id || account.state !== "active" || account.device_fingerprint !== fingerprint) return success();
    const { error: lockError } = await admin.rpc("begin_password_recovery", { p_user_id: account.user_id });
    if (lockError) return Response.json({ code: "service_unavailable", message: "认证服务暂时不可用，请稍后重试。" }, { status: 503, headers: authCors });
    const anon = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!);
    const { error: mailError } = await anon.auth.resetPasswordForEmail(email, { redirectTo: "https://auth.edward.uno/?flow=recovery" });
    if (mailError) {
      await admin.rpc("complete_password_recovery", { p_user_id: account.user_id });
      return Response.json({ code: "mail_unavailable", message: "邮件暂时无法发送，请稍后重试。" }, { status: 503, headers: authCors });
    }
    await admin.from("device_security_audits").insert({
      registration_state_id: account.id, user_id: account.user_id, device_fingerprint: fingerprint,
      serial_ciphertext: await encryptDeviceValue(evidence.serial), mac_ciphertext: await encryptDeviceValue(evidence.mac),
      event_type: "password_recovery_requested",
    });
    return success();
  } catch {
    return Response.json({ code: "service_unavailable", message: "认证服务暂时不可用，请稍后重试。" }, { status: 503, headers: authCors });
  }
});
