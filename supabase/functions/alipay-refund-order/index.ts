import { cors, requireUser } from "../_shared/billing.ts";
import { adminClient } from "../_shared/huifu.ts";
import { ALIPAY_GATEWAY, buildAlipayGatewayRequest } from "../_shared/alipay.ts";

Deno.serve(async req => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const { user } = await requireUser(req);
  if (!user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  try {
    const { orderId } = await req.json(); const id = String(orderId || "");
    const admin = adminClient();
    const { data: period } = await admin.from("subscription_periods").select("order_id,period_start,status,amount").eq("order_id", id).eq("user_id", user.id).maybeSingle();
    if (!period) return Response.json({ error: "refund_period_not_found" }, { status: 404, headers: cors });
    if (new Date(period.period_start).getTime() <= Date.now() || period.status !== "scheduled") return Response.json({ error: "current_period_not_refundable" }, { status: 409, headers: cors });
    const { data: order } = await admin.from("orders").select("id,provider,provider_request_id,provider_order_id,paid_amount,status").eq("id", id).eq("user_id", user.id).eq("status", "paid").maybeSingle();
    if (!order || order.provider !== "alipay") return Response.json({ error: "refund_order_unavailable" }, { status: 409, headers: cors });
    const appId = Deno.env.get("ALIPAY_APP_ID"); const privateKey = Deno.env.get("ALIPAY_PRIVATE_KEY");
    if (!appId || !privateKey) return Response.json({ error: "payment_config_error" }, { status: 503, headers: cors });
    const refundRequestNo = `edward_refund_${crypto.randomUUID().replaceAll("-", "")}`.slice(0, 64);
    const params = await buildAlipayGatewayRequest("alipay.trade.refund", { out_trade_no: order.provider_request_id, trade_no: order.provider_order_id || undefined, refund_amount: (Number(order.paid_amount) / 100).toFixed(2), out_request_no: refundRequestNo }, { appId, privateKeyPem: privateKey });
    await admin.from("payment_audit_events").insert({ order_id: order.id, provider: "alipay", phase: "refund_request", payload: { out_trade_no: order.provider_request_id, trade_no: order.provider_order_id, refund_amount: (Number(order.paid_amount) / 100).toFixed(2), out_request_no: refundRequestNo } });
    const upstream = await fetch(Deno.env.get("ALIPAY_GATEWAY") || ALIPAY_GATEWAY, { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded;charset=utf-8" }, body: new URLSearchParams(params) });
    const response = await upstream.json(); const result = (response.alipay_trade_refund_response || {}) as Record<string, unknown>;
    await admin.from("payment_audit_events").insert({ order_id: order.id, provider: "alipay", phase: "refund_response", http_status: upstream.status, provider_code: String(result.code || ""), provider_status: String(result.msg || ""), payload: response });
    if (!upstream.ok || String(result.code || "") !== "10000") return Response.json({ error: "refund_provider_failed", providerCode: result.code || null, providerMessage: result.sub_msg || result.msg || null }, { status: 502, headers: cors });
    const { error } = await admin.rpc("refund_scheduled_period", { p_order_id: order.id });
    if (error) return Response.json({ error: "refund_reconcile_failed" }, { status: 503, headers: cors });
    await admin.from("orders").update({ status: "refunded", provider_response: response, updated_at: new Date().toISOString() }).eq("id", order.id);
    return Response.json({ orderId: order.id, status: "refunded" }, { headers: cors });
  } catch (error) { console.error("alipay-refund-order failed", String(error)); return Response.json({ error: "refund_unavailable" }, { status: 503, headers: cors }); }
});
