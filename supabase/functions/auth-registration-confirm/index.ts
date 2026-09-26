import { emailLandingCors } from "../_shared/auth-lifecycle.ts";
import { adminClient } from "../_shared/risk.ts";

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: emailLandingCors });
  if (req.method !== "POST") return Response.json({ message: "请求方式不正确。" }, { status: 405, headers: emailLandingCors });
  const token = req.headers.get("authorization")?.replace(/^Bearer\s+/i, "") || "";
  if (!token) return Response.json({ message: "确认链接已失效或已过期，请重新发送确认邮件。" }, { status: 401, headers: emailLandingCors });
  const admin = adminClient();
  const { data } = await admin.auth.getUser(token);
  const confirmedAt = data.user?.email_confirmed_at;
  if (!data.user || !confirmedAt) return Response.json({ message: "确认链接已失效或已过期，请重新发送确认邮件。" }, { status: 401, headers: emailLandingCors });
  const { data: completed, error } = await admin.rpc("complete_registration_confirmation", {
    p_user_id: data.user.id, p_confirmed_at: confirmedAt,
  });
  if (error || !completed) return Response.json({ message: "确认链接已失效或已过期，请重新发送确认邮件。" }, { status: 401, headers: emailLandingCors });
  return Response.json({ message: "邮箱已确认，请返回 Orbit 登录。" }, { headers: emailLandingCors });
});
