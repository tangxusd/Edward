import { emailLandingCors } from "../_shared/auth-lifecycle.ts";
import { adminClient } from "../_shared/risk.ts";

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: emailLandingCors });
  if (req.method !== "POST") return Response.json({ message: "请求方式不正确。" }, { status: 405, headers: emailLandingCors });
  const token = req.headers.get("authorization")?.replace(/^Bearer\s+/i, "") || "";
  if (!token) return Response.json({ message: "重置链接已失效或已过期，请重新发起找回密码。" }, { status: 401, headers: emailLandingCors });
  const admin = adminClient();
  const { data } = await admin.auth.getUser(token);
  if (!data.user) return Response.json({ message: "重置链接已失效或已过期，请重新发起找回密码。" }, { status: 401, headers: emailLandingCors });
  const { data: completed, error } = await admin.rpc("complete_password_recovery", { p_user_id: data.user.id });
  if (error) return Response.json({ message: "密码已更新，但恢复状态暂时无法确认。" }, { status: 503, headers: emailLandingCors });
  if (!completed) return Response.json({ message: "重置链接已失效，请重新发起找回密码。" }, { status: 401, headers: emailLandingCors });
  return Response.json({ message: "密码已更新，请返回 Edward 登录。" }, { headers: emailLandingCors });
});
