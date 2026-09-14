import { adminClient } from "../_shared/huifu.ts";
import { alipayVerify } from "../_shared/alipay.ts";

Deno.serve(async req => {
  if (req.method !== "POST") return new Response("method_not_allowed", { status: 405 });
  try {
    const form = await req.formData();
    const body = Object.fromEntries([...form.entries()].map(([key, value]) => [key, String(value)]));
    const signature = body.sign || "";
    const publicKey = Deno.env.get("ALIPAY_PUBLIC_KEY");
    if (!publicKey || !signature || !(await alipayVerify(body, signature, publicKey))) return new Response("fail", { status: 401 });
    const appId = Deno.env.get("ALIPAY_APP_ID");
    if (appId && body.app_id !== appId) return new Response("fail", { status: 401 });
    const status = body.trade_status || "";
    const outTradeNo = body.out_trade_no || "";
    if (!outTradeNo || !["TRADE_SUCCESS", "TRADE_FINISHED"].includes(status)) return new Response("success");
    const admin = adminClient();
    const { data: order } = await admin.from("orders").select("id,paid_amount,status").eq("provider_request_id", outTradeNo).maybeSingle();
    if (!order) return new Response("fail", { status: 404 });
    await admin.from("payment_audit_events").insert({ order_id: order.id, provider: "alipay", phase: "notify", provider_code: body.code || null, provider_status: status, payload: body });
    const paidCents = Math.round(Number(body.total_amount || 0) * 100);
    if (!Number.isFinite(paidCents) || paidCents !== Number(order.paid_amount)) return new Response("fail", { status: 400 });
    if (order.status !== "paid") {
      const { error: updateError } = await admin.from("orders").update({ status: "paid", provider_order_id: body.trade_no || null, provider_response: body, updated_at: new Date().toISOString() }).eq("id", order.id).eq("status", "pending");
      if (updateError) return new Response("fail", { status: 503 });
    }
    const { error: entitlementError } = await admin.rpc("apply_paid_order", { p_order_id: order.id });
    if (entitlementError) return new Response("fail", { status: 503 });
    return new Response("success");
  } catch (error) {
    console.error("alipay-notify failed", String(error));
    return new Response("fail", { status: 400 });
  }
});
