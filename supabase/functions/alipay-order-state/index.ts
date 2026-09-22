import { cors, requireUser } from "../_shared/billing.ts";
import { adminClient } from "../_shared/huifu.ts";

Deno.serve(async req => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  if (req.method !== "POST") return Response.json({ error: "method_not_allowed" }, { status: 405, headers: cors });
  const { user } = await requireUser(req);
  if (!user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  try {
    const body = await req.json();
    const orderId = String(body.orderId || "");
    if (!/^[0-9a-f-]{36}$/i.test(orderId)) return Response.json({ error: "invalid_order" }, { status: 400, headers: cors });
    const { data, error } = await adminClient().from("orders")
      .select("id,status,provider,paid_amount,provider_request_id,provider_order_id,updated_at")
      .eq("id", orderId).eq("user_id", user.id).maybeSingle();
    if (error) return Response.json({ error: "order_state_unavailable" }, { status: 502, headers: cors });
    if (!data) return Response.json({ error: "order_not_found" }, { status: 404, headers: cors });
    return Response.json(data, { headers: { ...cors, "Cache-Control": "no-store" } });
  } catch {
    return Response.json({ error: "invalid_request" }, { status: 400, headers: cors });
  }
});
