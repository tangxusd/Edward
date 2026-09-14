import { cors, requireUser } from "../_shared/billing.ts";
import { adminClient } from "../_shared/huifu.ts";
import { ALIPAY_GATEWAY, buildAlipayGatewayRequest } from "../_shared/alipay.ts";

Deno.serve(async req => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const { user } = await requireUser(req);
  if (!user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  try {
    const body = await req.json();
    const orderId = String(body.orderId || "");
    if (!orderId) return Response.json({ error: "invalid_order" }, { status: 400, headers: cors });
    const admin = adminClient();
    const { data, error } = await admin.from("orders").select("id,status,provider,paid_amount,provider_request_id,provider_order_id").eq("id", orderId).eq("user_id", user.id).maybeSingle();
    if (error) return Response.json({ error: "order_query_failed" }, { status: 502, headers: cors });
    if (!data) return Response.json({ error: "order_not_found" }, { status: 404, headers: cors });
    let current = data;
    if (data.provider === "alipay" && data.status === "pending") {
      const appId = Deno.env.get("ALIPAY_APP_ID"); const privateKey = Deno.env.get("ALIPAY_PRIVATE_KEY");
      if (appId && privateKey) {
        const params = await buildAlipayGatewayRequest("alipay.trade.query", { out_trade_no: data.provider_request_id }, { appId, privateKeyPem: privateKey });
        const upstream = await fetch((Deno.env.get("ALIPAY_GATEWAY") || ALIPAY_GATEWAY), { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded;charset=utf-8" }, body: new URLSearchParams(params) });
        const response = await upstream.json(); const result = (response.alipay_trade_query_response || {}) as Record<string, unknown>;
        await admin.from("payment_audit_events").insert({ order_id: data.id, provider: "alipay", phase: "query", http_status: upstream.status, provider_code: String(result.code || ""), provider_status: String(result.trade_status || ""), payload: response });
        if (upstream.ok && String(result.code || "") === "10000" && ["TRADE_SUCCESS", "TRADE_FINISHED"].includes(String(result.trade_status || "")) && Math.round(Number(result.total_amount || 0) * 100) === Number(data.paid_amount)) {
          await admin.from("orders").update({ status: "paid", provider_order_id: result.trade_no || data.provider_order_id, provider_response: response, updated_at: new Date().toISOString() }).eq("id", data.id).eq("status", "pending");
          await admin.rpc("apply_paid_order", { p_order_id: data.id });
          current = { ...data, status: "paid", provider_order_id: result.trade_no || data.provider_order_id };
        }
      }
    }
    return Response.json(current, { headers: cors });
  } catch { return Response.json({ error: "invalid_request" }, { status: 400, headers: cors }); }
});
