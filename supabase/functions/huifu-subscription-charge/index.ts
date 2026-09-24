import { adminClient, cors, huifuPost, recurringEnabled, recurringUnavailable, requireAuthenticated, requestId } from "../huifu-recurring.ts";

Deno.serve(async (req) => {
  const auth = await requireAuthenticated(req);
  if (auth.response) return auth.response;
  if (!recurringEnabled()) return recurringUnavailable();
  try {
    const body = await req.json();
    const admin = adminClient();
    const { data: subscription } = await admin.from("subscriptions").select("id,plan_id,current_period_end,cancel_at_period_end,status").eq("id", String(body.subscriptionId || "")).eq("user_id", auth.user.id).maybeSingle();
    if (!subscription || !["active", "past_due"].includes(subscription.status) || subscription.cancel_at_period_end && new Date(subscription.current_period_end) <= new Date()) return Response.json({ error: "subscription_unavailable" }, { status: 404, headers: cors });
    const { data: plan } = await admin.from("subscription_plans").select("currency,unit_amount,billing_interval").eq("id", subscription.plan_id).single();
    const { data: binding } = await admin.from("subscription_provider_bindings").select("provider_token,provider_customer_id,status").eq("subscription_id", subscription.id).eq("status", "active").single();
    if (!plan || !binding) return Response.json({ error: "subscription_authorization_required" }, { status: 409, headers: cors });
    const periodStart = new Date(subscription.current_period_end);
    const periodEnd = new Date(periodStart);
    if (plan.billing_interval === "day") periodEnd.setUTCDate(periodEnd.getUTCDate() + 1);
    else if (plan.billing_interval === "year") periodEnd.setUTCFullYear(periodEnd.getUTCFullYear() + 1);
    else periodEnd.setUTCMonth(periodEnd.getUTCMonth() + 1);
    const providerRequestId = requestId("edwardcharge");
    const idempotencyKey = `sub:${subscription.id}:${periodStart.toISOString()}`;
    const { data: charge, error } = await admin.from("subscription_charges").insert({ subscription_id: subscription.id, user_id: auth.user.id, period_start: periodStart.toISOString(), period_end: periodEnd.toISOString(), amount: Number(plan.unit_amount), currency: plan.currency, idempotency_key: idempotencyKey, provider_request_id: providerRequestId, attempt_count: 1 }).select("id,status,amount,currency,period_end").single();
    if (error) return Response.json({ error: "charge_already_created" }, { status: 409, headers: cors });
    const result = await huifuPost(Deno.env.get("HUIFU_RECURRING_CHARGE_PATH") || "v2/trade/onlinepayment/withholdpay", { req_date: new Date().toISOString().slice(0, 10).replaceAll("-", ""), req_seq_id: providerRequestId, huifu_id: Deno.env.get("HUIFU_MERCHANT_ID"), user_huifu_id: binding.provider_customer_id || auth.user.id, card_bind_id: binding.provider_token, trans_amt: (Number(plan.unit_amount) / 100).toFixed(2), goods_desc: "Orbit订阅续费", withhold_type: "01", notify_url: Deno.env.get("HUIFU_NOTIFY_URL") });
    await admin.from("subscription_charges").update({ status: String(result.trans_stat || "") === "S" ? "paid" : "pending", provider_order_id: result.party_order_id || result.order_id || null, provider_response: result }).eq("id", charge.id);
    return Response.json({ chargeId: charge.id, status: String(result.trans_stat || "") === "S" ? "paid" : "pending", provider: result }, { status: 201, headers: cors });
  } catch (error) {
    console.error("huifu-subscription-charge failed", String(error).slice(0, 180));
    return Response.json({ error: "subscription_charge_failed" }, { status: 502, headers: cors });
  }
});
