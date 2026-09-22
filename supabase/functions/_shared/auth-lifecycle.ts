import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { adminClient } from "./risk.ts";

export const authCors = {
  "Access-Control-Allow-Origin": "http://127.0.0.1:7777",
  "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type",
  "Content-Type": "application/json; charset=utf-8",
};

export const emailLandingCors = {
  "Access-Control-Allow-Origin": "https://auth-recovery.vercel.app",
  "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type",
  "Content-Type": "application/json; charset=utf-8",
};

export type DeviceEvidence = { serial: string; mac: string };
export type RegisterInput = DeviceEvidence & { email: string; password: string; username: string };
export type LifecycleReply = { status: number; body: Record<string, unknown> };

const messageByCode: Record<string, string> = {
  confirmation_pending: "确认邮件已发送，请在 10 分钟内确认完成。",
  account_active: "该邮箱已完成注册，请直接登录。",
  invalid_registration: "请使用有效邮箱，密码至少 8 位，用户名为 3～32 个字符。",
  username_taken: "该用户名已被使用，请更换用户名。",
  device_already_registered: "该设备已注册账号，请直接登录或使用其他设备。",
  invalid_device: "无法读取设备标识，请检查系统权限后重试。",
  rate_limited: "请求过于频繁，请稍后重试。",
  mail_unavailable: "邮件暂时无法发送，请稍后重试。",
  registration_unavailable: "注册暂时无法完成，请稍后重试。",
  service_unavailable: "认证服务暂时不可用，请稍后重试。",
};

export const publicAuthMessage = (code: string) => messageByCode[code] || messageByCode.service_unavailable;
export const reply = (status: number, code: string, extra: Record<string, unknown> = {}): LifecycleReply => ({
  status,
  body: { code, message: publicAuthMessage(code), ...extra },
});

const bytesFromBase64 = (value: string) => Uint8Array.from(atob(value), (char) => char.charCodeAt(0));
const base64FromBytes = (value: Uint8Array) => btoa(String.fromCharCode(...value));

const requireSecret = (name: string) => {
  const value = Deno.env.get(name);
  if (!value) throw new Error(`missing_${name}`);
  return value;
};

export const normalizeDeviceEvidence = (evidence: DeviceEvidence): DeviceEvidence | null => {
  const serial = evidence.serial.trim().toUpperCase();
  const rawMac = evidence.mac.trim().toUpperCase().replaceAll("-", ":");
  const mac = /^[0-9A-F]{2}(:[0-9A-F]{2}){5}$/.test(rawMac) ? rawMac : "";
  if (!serial || serial.length > 256 || !mac) return null;
  return { serial, mac };
};

export async function deviceFingerprint(evidence: DeviceEvidence): Promise<string> {
  const key = await crypto.subtle.importKey(
    "raw", bytesFromBase64(requireSecret("DEVICE_FINGERPRINT_HMAC_KEY")), { name: "HMAC", hash: "SHA-256" }, false, ["sign"],
  );
  const input = new TextEncoder().encode(`${evidence.serial}\n${evidence.mac}`);
  return base64FromBytes(new Uint8Array(await crypto.subtle.sign("HMAC", key, input)));
}

export async function encryptDeviceValue(value: string): Promise<string> {
  const rawKey = bytesFromBase64(requireSecret("DEVICE_AUDIT_ENCRYPTION_KEY"));
  if (rawKey.byteLength !== 32) throw new Error("invalid_DEVICE_AUDIT_ENCRYPTION_KEY");
  const key = await crypto.subtle.importKey("raw", rawKey, { name: "AES-GCM" }, false, ["encrypt"]);
  const nonce = crypto.getRandomValues(new Uint8Array(12));
  const ciphertext = new Uint8Array(await crypto.subtle.encrypt({ name: "AES-GCM", iv: nonce }, key, new TextEncoder().encode(value)));
  return `v1.${base64FromBytes(nonce)}.${base64FromBytes(ciphertext)}`;
}

const usernameRe = /^[\p{L}\p{N}_-]{3,32}$/u;
const signupRedirect = "https://auth-recovery.vercel.app/?flow=signup";

export async function cleanupExpiredRegistrations(now = new Date()): Promise<number> {
  const admin = adminClient();
  const { data: records, error } = await admin.from("auth_registration_states")
    .select("id,user_id").eq("state", "registration_pending").lte("expires_at", now.toISOString());
  if (error) throw error;
  let count = 0;
  for (const record of records || []) {
    if (record.user_id) {
      const { data: userResult } = await admin.auth.admin.getUserById(record.user_id);
      const confirmedAt = userResult.user?.email_confirmed_at;
      if (confirmedAt) {
        await admin.rpc("complete_registration_confirmation", { p_user_id: record.user_id, p_confirmed_at: confirmedAt });
        continue;
      }
      const { error: deleteError } = await admin.auth.admin.deleteUser(record.user_id);
      if (deleteError) continue;
    } else {
      const { error: deleteStateError } = await admin.from("auth_registration_states").delete().eq("id", record.id);
      if (deleteStateError) continue;
    }
    count += 1;
  }
  return count;
}

export async function registerAccount(input: RegisterInput): Promise<LifecycleReply> {
  const email = input.email.trim().toLowerCase();
  const password = input.password;
  const username = input.username.trim();
  const evidence = normalizeDeviceEvidence({ serial: input.serial, mac: input.mac });
  if (!email.includes("@") || password.length < 8 || !usernameRe.test(username)) return reply(400, "invalid_registration");
  if (!evidence) return reply(400, "invalid_device");
  try {
    const fingerprint = await deviceFingerprint(evidence);
    const admin = adminClient();
    const { data: existingUsername, error: usernameLookupError } = await admin.from("profiles")
      .select("user_id").eq("username_normalized", username.toLocaleLowerCase()).maybeSingle();
    if (usernameLookupError) return reply(503, "service_unavailable");
    if (existingUsername) return reply(409, "username_taken");
    let reservation = await admin.rpc("reserve_registration_attempt", {
      p_email_normalized: email,
      p_device_fingerprint: fingerprint,
    });
    if (reservation.error || !reservation.data?.[0]) return reply(503, "service_unavailable");
    let state = reservation.data[0];
    if (state.action === "expired_cleanup") {
      if (state.user_id) {
        const { error } = await admin.auth.admin.deleteUser(state.user_id);
        if (error) return reply(503, "service_unavailable");
      } else {
        const { error } = await admin.from("auth_registration_states").delete().eq("id", state.registration_id);
        if (error) return reply(503, "service_unavailable");
      }
      reservation = await admin.rpc("reserve_registration_attempt", {
        p_email_normalized: email,
        p_device_fingerprint: fingerprint,
      });
      if (reservation.error || !reservation.data?.[0] || reservation.data[0].action !== "create") return reply(503, "service_unavailable");
      state = reservation.data[0];
    }
    const anon = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!);
    if (state.action === "active") return reply(409, "account_active");
    if (state.action === "device_already_registered") return reply(409, "device_already_registered");
    if (state.action === "pending") {
      const { error: resendError } = await anon.auth.resend({ type: "signup", email, options: { emailRedirectTo: signupRedirect } });
      if (resendError) return reply(503, "mail_unavailable");
      return reply(200, "confirmation_pending", { emailVerificationRequired: true });
    }
    const { data, error } = await anon.auth.signUp({ email, password, options: { emailRedirectTo: signupRedirect } });
    if (error || !data.user) {
      await admin.from("auth_registration_states").delete().eq("id", state.registration_id);
      return reply(503, "mail_unavailable");
    }
    const referralCode = `${data.user.id.replaceAll("-", "").slice(0, 10)}${crypto.randomUUID().replaceAll("-", "").slice(0, 6)}`;
    const { error: profileError } = await admin.from("profiles").insert({
      user_id: data.user.id, username_normalized: username.toLocaleLowerCase(), username_display: username, referral_code: referralCode,
    });
    const { error: attachError, data: attached } = await admin.rpc("attach_registration_user", {
      p_registration_id: state.registration_id, p_user_id: data.user.id,
    });
    const serialCiphertext = await encryptDeviceValue(evidence.serial);
    const macCiphertext = await encryptDeviceValue(evidence.mac);
    const { error: auditError } = await admin.from("device_security_audits").insert({
      registration_state_id: state.registration_id, user_id: data.user.id, device_fingerprint: fingerprint,
      serial_ciphertext: serialCiphertext, mac_ciphertext: macCiphertext, event_type: "registration_requested",
    });
    if (profileError || attachError || !attached || auditError) {
      await admin.auth.admin.deleteUser(data.user.id);
      await admin.from("auth_registration_states").delete().eq("id", state.registration_id);
      if (profileError?.code === "23505") return reply(409, "username_taken");
      return reply(503, "registration_unavailable");
    }
    return reply(201, "confirmation_pending", { emailVerificationRequired: true });
  } catch {
    return reply(503, "service_unavailable");
  }
}
