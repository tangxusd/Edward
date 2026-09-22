import { calculateOrderAmounts, cors, requireUser } from "../_shared/billing.ts";
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
    const { data: existing } = await admin.from("orders")
      .select("id,status,paid_amount,provider_qr_code,provider_request_id,created_at")
      .eq("idempotency_key", idempotencyKey).eq("user_id", user.id).maybeSingle();
    const existingExpired = existing?.status === "pending" && Date.now() - Date.parse(existing.created_at) >= 30 * 60 * 1000;
    if (existingExpired) {
      await admin.rpc("release_order_credits", { p_order_id: existing.id });
      await admin.from("orders").update({ status: "expired", updated_at: new Date().toISOString() }).eq("id", existing.id).eq("status", "pending");
    }
    const orderIdempotencyKey = existingExpired ? crypto.randomUUID() : idempotencyKey;
    if (existing?.provider_qr_code && !existingExpired) {
      return Response.json({ orderId: existing.id, status: existing.status, qrCode: existing.provider_qr_code, requestId: existing.provider_request_id }, { headers: cors });
    }
    if (existing && !existingExpired) {
      if (existing.status === "pending" && !existing.provider_qr_code && Date.now() - Date.parse(existing.created_at) > 30_000) {
        await admin.rpc("release_order_credits", { p_order_id: existing.id });
        await admin.from("orders").update({ status: "expired", updated_at: new Date().toISOString() }).eq("id", existing.id).eq("status", "pending");
      } else {
      const retryable = ["failed", "expired", "refunded"].includes(String(existing.status));
      return Response.json({ orderId: existing.id, status: existing.status, error: retryable ? "payment_order_retryable" : "payment_order_pending" }, { status: 409, headers: cors });
      }
    }
    const now = new Date().toISOString();
    const { data: plan } = await admin.from("subscription_plans").select("id,name,currency,unit_amount,discount_config,status,effective_from,effective_to").eq("plan_key", planKey).eq("status", "active").lte("effective_from", now).or(`effective_to.is.null,effective_to.gt.${now}`).maybeSingle();
    if (!plan || plan.currency !== "CNY") return Response.json({ error: "plan_unavailable" }, { status: 404, headers: cors });
    const { data: pendingOrder } = await admin.from("orders")
      .select("id,status,provider_qr_code,provider_request_id,created_at")
      .eq("user_id", user.id).eq("plan_id", plan.id).eq("provider", "alipay").eq("status", "pending")
      .maybeSingle();
    const pendingExpired = pendingOrder && Date.now() - Date.parse(pendingOrder.created_at) >= 30 * 60 * 1000;
    if (pendingExpired) {
      await admin.rpc("release_order_credits", { p_order_id: pendingOrder.id });
      await admin.from("orders").update({ status: "expired", updated_at: new Date().toISOString() }).eq("id", pendingOrder.id).eq("status", "pending");
    }
    if (pendingOrder?.provider_qr_code && !pendingExpired) {
      return Response.json({ orderId: pendingOrder.id, status: pendingOrder.status, qrCode: pendingOrder.provider_qr_code, requestId: pendingOrder.provider_request_id }, { headers: cors });
    }
    if (pendingOrder && !pendingExpired) {
      if (!pendingOrder.provider_qr_code && Date.now() - Date.parse(pendingOrder.created_at) > 30_000) {
        await admin.rpc("release_order_credits", { p_order_id: pendingOrder.id });
        await admin.from("orders").update({ status: "expired", updated_at: new Date().toISOString() }).eq("id", pendingOrder.id).eq("status", "pending");
      } else return Response.json({ orderId: pendingOrder.id, status: pendingOrder.status, error: "payment_order_pending" }, { status: 409, headers: cors });
    }
    const amounts = calculateOrderAmounts(Number(plan.unit_amount), Number(plan.discount_config?.percent || 0), 0, 0);
    if (!Number.isInteger(amounts.original) || amounts.original < 1 || amounts.paidAmount < 1) return Response.json({ error: "invalid_plan_amount" }, { status: 409, headers: cors });
    const outTradeNo = `edward_${Date.now()}_${crypto.randomUUID().replaceAll("-", "").slice(0, 12)}`.slice(0, 64);
    const { data: order, error } = await admin.from("orders").insert({ user_id: user.id, plan_id: plan.id, idempotency_key: orderIdempotencyKey, currency: "CNY", original_amount: amounts.original, discount_amount: amounts.discountAmount, credit_amount: amounts.creditAmount, paid_amount: amounts.paidAmount, rule_version: 1, status: "pending", provider: "alipay", provider_request_id: outTradeNo }).select("id,status,paid_amount").single();
    if (error) {
      if (error.code === "23505") {
        const { data: concurrentOrder } = await admin.from("orders")
          .select("id,status,provider_qr_code,provider_request_id,created_at")
          .eq("user_id", user.id).eq("plan_id", plan.id).eq("provider", "alipay").eq("status", "pending")
          .maybeSingle();
        const concurrentExpired = concurrentOrder && Date.now() - Date.parse(concurrentOrder.created_at) >= 30 * 60 * 1000;
            if (concurrentExpired) {
              await admin.rpc("release_order_credits", { p_order_id: concurrentOrder.id });
              await admin.from("orders").update({ status: "expired", updated_at: new Date().toISOString() }).eq("id", concurrentOrder.id).eq("status", "pending");
            }
        if (concurrentOrder?.provider_qr_code && !concurrentExpired) return Response.json({ orderId: concurrentOrder.id, status: concurrentOrder.status, qrCode: concurrentOrder.provider_qr_code, requestId: concurrentOrder.provider_request_id }, { headers: cors });
        if (concurrentOrder && !concurrentExpired) {
          if (!concurrentOrder.provider_qr_code && Date.now() - Date.parse(concurrentOrder.created_at) > 30_000) {
            await admin.rpc("release_order_credits", { p_order_id: concurrentOrder.id });
            await admin.from("orders").update({ status: "expired", updated_at: new Date().toISOString() }).eq("id", concurrentOrder.id).eq("status", "pending");
          } else return Response.json({ orderId: concurrentOrder.id, status: concurrentOrder.status, error: "payment_order_pending" }, { status: 409, headers: cors });
        }
      }
      return Response.json({ error: "order_unavailable" }, { status: 409, headers: cors });
    }
    const { error: creditError } = await admin.rpc("reserve_order_credits", { p_order_id: order.id });
    if (creditError) {
      await admin.from("orders").update({ status: "failed", updated_at: new Date().toISOString() }).eq("id", order.id).eq("status", "pending");
      return Response.json({ error: "credit_unavailable" }, { status: 409, headers: cors });
    }
    const timeoutExpress = "30m";
    let params: Record<string, string>;
    try {
      params = await buildAlipayGatewayRequest("alipay.trade.precreate", { out_trade_no: outTradeNo, total_amount: (amounts.paidAmount / 100).toFixed(2), subject: String(plan.name).slice(0, 256), product_code: "FACE_TO_FACE_PAYMENT", timeout_express: timeoutExpress }, { appId, privateKeyPem: privateKey, notifyUrl });
    } catch {
      await admin.rpc("release_order_credits", { p_order_id: order.id });
      await admin.from("orders").update({ status: "failed", updated_at: new Date().toISOString() }).eq("id", order.id).eq("status", "pending");
      return Response.json({ error: "payment_request_build_failed" }, { status: 502, headers: cors });
    }
    await admin.from("payment_audit_events").insert({ order_id: order.id, provider: "alipay", phase: "create_request", payload: { method: "alipay.trade.precreate", out_trade_no: outTradeNo, app_id: appId, original_amount: amounts.original, discount_amount: amounts.discountAmount, paid_amount: amounts.paidAmount, timeout_express: timeoutExpress, biz_content: params.biz_content, notify_url: notifyUrl } });
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 15000);
    let upstream: Response;
    try {
      upstream = await fetch(gateway(), { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded;charset=utf-8", Accept: "application/json" }, body: new URLSearchParams(params), signal: controller.signal });
    } catch (error) {
      if (error instanceof DOMException && error.name === "AbortError") {
        await admin.rpc("release_order_credits", { p_order_id: order.id });
        await admin.from("orders").update({ status: "failed", updated_at: new Date().toISOString() }).eq("id", order.id).eq("status", "pending");
        return Response.json({ error: "payment_gateway_timeout" }, { status: 504, headers: cors });
      }
      throw error;
    } finally {
      clearTimeout(timeout);
    }
    const raw = await upstream.text();
    let response: Record<string, unknown> = {};
    try { response = JSON.parse(raw); } catch { /* 诊断信息通过原文返回 */ }
    const result = (response.alipay_trade_precreate_response || {}) as Record<string, unknown>;
    await admin.from("payment_audit_events").insert({ order_id: order.id, provider: "alipay", phase: "create_response", http_status: upstream.status, provider_code: String(result.code || ""), provider_status: String(result.msg || ""), payload: response });
    if (!upstream.ok || String(result.code || "") !== "10000" || !result.qr_code) {
      await admin.rpc("release_order_credits", { p_order_id: order.id });
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
