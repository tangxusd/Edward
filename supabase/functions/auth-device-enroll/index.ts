import { authCors, deviceFingerprint, encryptDeviceValue, normalizeDeviceEvidence } from "../_shared/auth-lifecycle.ts";
import { adminClient } from "../_shared/risk.ts";

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: authCors });
  if (req.method !== "POST") return Response.json({ message: "请求方式不正确。" }, { status: 405, headers: authCors });
  try {
    const token = req.headers.get("authorization")?.replace(/^Bearer\s+/i, "") || "";
    const admin = adminClient();
    const { data: userResult, error: userError } = await admin.auth.getUser(token);
    if (userError || !userResult.user) return Response.json({ message: "登录状态已失效，请重新登录。" }, { status: 401, headers: authCors });
    const body = await req.json();
    const evidence = normalizeDeviceEvidence({ serial: String(body.deviceSerial || ""), mac: String(body.deviceMac || "") });
    if (!evidence) return Response.json({ message: "无法读取设备标识，请检查系统权限后重试。" }, { status: 400, headers: authCors });
    const fingerprint = await deviceFingerprint(evidence);
    const { data: state } = await admin.from("auth_registration_states")
      .select("id,device_fingerprint,state").eq("user_id", userResult.user.id).maybeSingle();
    if (!state || state.state !== "active") return Response.json({ message: "当前账户无法完成设备校验。" }, { status: 403, headers: authCors });
    if (state.device_fingerprint && state.device_fingerprint !== fingerprint)
      return Response.json({ message: "该账户已绑定其他设备，无法在当前设备执行找回密码。" }, { status: 403, headers: authCors });
    if (!state.device_fingerprint) {
      const { error: updateError } = await admin.from("auth_registration_states")
        .update({ device_fingerprint: fingerprint, updated_at: new Date().toISOString() })
        .eq("id", state.id).is("device_fingerprint", null);
      if (updateError) return Response.json({ message: "设备校验暂时无法完成，请稍后重试。" }, { status: 503, headers: authCors });
      await admin.from("device_security_audits").insert({
        registration_state_id: state.id, user_id: userResult.user.id, device_fingerprint: fingerprint,
        serial_ciphertext: await encryptDeviceValue(evidence.serial), mac_ciphertext: await encryptDeviceValue(evidence.mac),
        event_type: "legacy_device_enrolled",
      });
    }
    return Response.json({ message: "设备校验完成。" }, { headers: authCors });
  } catch {
    return Response.json({ message: "设备校验暂时无法完成，请稍后重试。" }, { status: 503, headers: authCors });
  }
});
