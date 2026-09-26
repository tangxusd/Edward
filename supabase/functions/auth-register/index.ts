import { authCors, registerAccount } from "../_shared/auth-lifecycle.ts";
import { rateCheck } from "../_shared/risk.ts";
Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: authCors });
  if (req.method !== "POST") return Response.json({ code: "method_not_allowed", message: "请求方式不正确。" }, { status: 405, headers: authCors });
  try {
    const body = await req.json();
    const email = String(body.email || "").trim().toLowerCase();
    const clientAddress = req.headers.get("cf-connecting-ip") || req.headers.get("x-real-ip") || "unknown";
    const risk = await rateCheck(`${clientAddress}:${email}`, "register", 5);
    if (!risk.allowed) return Response.json({ code: "rate_limited", message: "请求过于频繁，请稍后重试。" }, { status: 429, headers: authCors });
    const outcome = await registerAccount({
      email, password: String(body.password || ""), username: String(body.username || email.split("@")[0] || ""),
      serial: String(body.deviceSerial || ""), mac: String(body.deviceMac || ""),
    });
    return Response.json(outcome.body, { status: outcome.status, headers: authCors });
  } catch { return Response.json({ code: "registration_unavailable", message: "注册暂时无法完成，请稍后重试。" }, { status: 503, headers: authCors }); }
});
