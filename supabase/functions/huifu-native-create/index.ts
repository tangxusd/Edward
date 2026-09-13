import { cors, requireUser } from "../_shared/billing.ts";
import { adminClient, huifuPost } from "../_shared/huifu.ts";

Deno.serve(async req => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const { user } = await requireUser(req);
  if (!user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  try {
    const body = await req.json();
    const amount = Number(body.amount);
    if (!Number.isFinite(amount) || amount < 0.01 || amount > 100000) return Response.json({ error: "invalid_amount" }, { status: 400, headers: cors });
    const idempotencyKey = String(body.idempotencyKey || crypto.randomUUID());
    const admin = adminClient();
    const { data: existing } = await admin.from("orders").select("id,status,provider_qr_code,provider_response,paid_amount").eq("idempotency_key", idempotencyKey).maybeSingle();
    if (existing) return Response.json(existing, { headers: cors });
    const reqDate = new Date().toISOString().slice(0, 10).replaceAll("-", "");
    const reqSeq = `edward${Date.now()}${crypto.randomUUID().replaceAll("-", "").slice(0, 10)}`.slice(0, 32);
    const expire = new Date(Date.now() + 30 * 60_000); const pad = (n:number) => String(n).padStart(2, "0");
    const timeExpire = `${expire.getFullYear()}${pad(expire.getMonth()+1)}${pad(expire.getDate())}${pad(expire.getHours())}${pad(expire.getMinutes())}${pad(expire.getSeconds())}`;
    const notify = Deno.env.get("HUIFU_NOTIFY_URL");
    if (!notify || !notify.startsWith("https://")) return Response.json({ error: "notify_url_not_configured" }, { status: 503, headers: cors });
    const { data: order, error } = await admin.from("orders").insert({ user_id: user.id, idempotency_key: idempotencyKey, currency: "CNY", original_amount: Math.round(amount * 100), discount_amount: 0, credit_amount: 0, paid_amount: Math.round(amount * 100), rule_version: 1, status: "pending", provider: "huifu", provider_request_id: reqSeq }).select("id,status,paid_amount").single();
    if (error) return Response.json({ error: "order_unavailable" }, { status: 409, headers: cors });
    const result = await huifuPost("v3/trade/payment/jspay", { req_date: reqDate, req_seq_id: reqSeq, huifu_id: Deno.env.get("HUIFU_MERCHANT_ID"), acct_id: Deno.env.get("HUIFU_DEFAULT_ACCT_ID"), goods_desc: String(body.goodsDesc || "Edward订阅").slice(0, 127), trade_type: "T_NATIVE", trans_amt: amount.toFixed(2), time_expire: timeExpire, notify_url: notify });
    if (String(result.trans_stat || "") === "F" || String(result.resp_code || "") !== "00000000") {
      await admin.from("orders").update({ status: "failed", provider_response: result }).eq("id", order.id);
      return Response.json({ error: "payment_create_failed", detail: result }, { status: 502, headers: cors });
    }
    await admin.from("orders").update({ provider_order_id: result.party_order_id || result.order_id || null, provider_qr_code: result.qr_code || result.code_url || null, provider_response: result }).eq("id", order.id);
    return Response.json({ orderId: order.id, status: "pending", qrCode: result.qr_code || result.code_url || null, requestId: reqSeq }, { status: 201, headers: cors });
  } catch (e) { return Response.json({ error: "payment_unavailable", detail: String(e).slice(0, 200) }, { status: 503, headers: cors }); }
});
