import { cors, requireUser } from "../_shared/billing.ts";
import { adminClient } from "../_shared/huifu.ts";
import { ALIPAY_GATEWAY, buildAlipayGatewayRequest } from "../_shared/alipay.ts";

const gateway = () => (Deno.env.get("ALIPAY_GATEWAY") || ALIPAY_GATEWAY).replace(/\/$/, "");

Deno.serve(async req => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const { user } = await requireUser(req);
  if (!user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  try {
    const body = await req.json();
    const planKey = String(body.planKey || "");
    const idempotencyKey = String(body.idempotencyKey || crypto.randomUUID());
    const appId = Deno.env.get("ALIPAY_APP_ID");
    const privateKey = Deno.env.get("ALIPAY_PRIVATE_KEY");
    const notifyUrl = Deno.env.get("ALIPAY_NOTIFY_URL");
    if (!appId || !privateKey || !notifyUrl?.startsWith("https://")) return Response.json({ error: "payment_config_error" }, { status: 503, headers: cors });
    const admin = adminClient();
    const { data: existing } = await admin.from("orders").select("id,status,paid_amount,provider_qr_code,provider_order_id").eq("idempotency_key", idempotencyKey).maybeSingle();
    if (existing) return Response.json(existing, { headers: cors });
    const { data: plan } = await admin.from("subscription_plans").select("id,name,currency,unit_amount,status,effective_from,effective_to").eq("plan_key", planKey).eq("status", "active").maybeSingle();
    if (!plan || plan.currency !== "CNY") return Response.json({ error: "plan_unavailable" }, { status: 404, headers: cors });
    const amount = Number(plan.unit_amount);
    if (!Number.isInteger(amount) || amount < 1) return Response.json({ error: "invalid_plan_amount" }, { status: 409, headers: cors });
    const outTradeNo = `edward_${Date.now()}_${crypto.randomUUID().replaceAll("-", "").slice(0, 12)}`.slice(0, 64);
    const { data: order, error } = await admin.from("orders").insert({ user_id: user.id, plan_id: plan.id, idempotency_key: idempotencyKey, currency: "CNY", original_amount: amount, discount_amount: 0, credit_amount: 0, paid_amount: amount, rule_version: 1, status: "pending", provider: "alipay", provider_request_id: outTradeNo }).select("id,status,paid_amount").single();
    if (error) return Response.json({ error: "order_unavailable" }, { status: 409, headers: cors });
    const params = await buildAlipayGatewayRequest("alipay.trade.precreate", { out_trade_no: outTradeNo, total_amount: (amount / 100).toFixed(2), subject: String(plan.name).slice(0, 256), product_code: "FACE_TO_FACE_PAYMENT" }, { appId, privateKeyPem: privateKey, notifyUrl });
    const upstream = await fetch(gateway(), { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded;charset=utf-8", Accept: "application/json" }, body: new URLSearchParams(params) });
    const raw = await upstream.text();
    let response: Record<string, unknown> = {};
    try { response = JSON.parse(raw); } catch { /* 诊断信息通过原文返回 */ }
    const result = (response.alipay_trade_precreate_response || {}) as Record<string, unknown>;
    if (!upstream.ok || String(result.code || "") !== "10000" || !result.qr_code) {
      await admin.from("orders").update({ status: "failed", provider_response: { httpStatus: upstream.status, response: response || raw.slice(0, 1000) } }).eq("id", order.id);
      return Response.json({ error: "payment_create_failed", upstreamStatus: upstream.status, providerCode: result.code || null, providerMessage: result.sub_msg || result.msg || null }, { status: 502, headers: cors });
    }
    await admin.from("orders").update({ provider_order_id: result.out_trade_no || outTradeNo, provider_qr_code: result.qr_code, provider_response: response }).eq("id", order.id);
    return Response.json({ orderId: order.id, status: "pending", qrCode: result.qr_code, requestId: outTradeNo }, { status: 201, headers: cors });
  } catch (error) {
    console.error("alipay-create-order failed", String(error));
    return Response.json({ error: "payment_unavailable", detail: String(error).slice(0, 180) }, { status: 503, headers: cors });
  }
});
