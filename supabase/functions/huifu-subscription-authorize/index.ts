import { adminClient, cors, huifuPost, recurringEnabled, recurringUnavailable, requireAuthenticated, requestId } from "../huifu-recurring.ts";

Deno.serve(async (req) => {
  const auth = await requireAuthenticated(req);
  if (auth.response) return auth.response;
  if (!recurringEnabled()) return recurringUnavailable();
  try {
    const body = await req.json();
    const planKey = String(body.planKey || "");
    const admin = adminClient();
    const { data: plan } = await admin.from("subscription_plans").select("id,currency,unit_amount,billing_interval,is_recurring,status").eq("plan_key", planKey).eq("status", "active").eq("is_recurring", true).maybeSingle();
    if (!plan) return Response.json({ error: "recurring_plan_unavailable" }, { status: 404, headers: cors });
    const now = new Date();
    const periodEnd = new Date(now);
    if (plan.billing_interval === "day") periodEnd.setUTCDate(periodEnd.getUTCDate() + 1);
    else if (plan.billing_interval === "year") periodEnd.setUTCFullYear(periodEnd.getUTCFullYear() + 1);
    else periodEnd.setUTCMonth(periodEnd.getUTCMonth() + 1);
    const { data: subscription, error } = await admin.from("subscriptions").insert({ user_id: auth.user.id, plan_id: plan.id, status: "active", current_period_start: now.toISOString(), current_period_end: periodEnd.toISOString(), provider: "huifu" }).select("id").single();
    if (error) return Response.json({ error: "subscription_unavailable" }, { status: 409, headers: cors });
    const providerResult = await huifuPost(Deno.env.get("HUIFU_RECURRING_AUTH_PATH")!, {
      req_date: now.toISOString().slice(0, 10).replaceAll("-", ""),
      req_seq_id: requestId("edwardauth"),
      huifu_id: Deno.env.get("HUIFU_MERCHANT_ID"),
      out_cust_id: auth.user.id,
      return_url: Deno.env.get("HUIFU_RECURRING_RETURN_URL"),
      // 客户端不得注入汇付扩展字段；待商户产品确认后由服务端白名单生成。
      extend_infos: {},
    });
    const token = String(providerResult.token_no || providerResult.card_bind_id || providerResult.bind_id || "");
    if (!token) return Response.json({ error: "provider_binding_missing", detail: providerResult }, { status: 502, headers: cors });
    await admin.from("subscription_provider_bindings").insert({ subscription_id: subscription.id, user_id: auth.user.id, provider_token: token, provider_customer_id: providerResult.user_huifu_id || null, status: "active", authorized_at: now.toISOString() });
    return Response.json({ subscriptionId: subscription.id, status: "active", authorization: providerResult }, { status: 201, headers: cors });
  } catch (error) {
    console.error("huifu-subscription-authorize failed", String(error).slice(0, 180));
    return Response.json({ error: "subscription_authorization_failed" }, { status: 502, headers: cors });
  }
});
