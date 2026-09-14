import { cors, requireUser } from "../_shared/billing.ts";
import { adminClient } from "../_shared/huifu.ts";

Deno.serve(async req => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const { user } = await requireUser(req);
  if (!user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  try {
    const body = await req.json();
    const orderId = String(body.orderId || "");
    if (!orderId) return Response.json({ error: "invalid_order" }, { status: 400, headers: cors });
    const { data, error } = await adminClient().from("orders").select("id,status,provider,paid_amount").eq("id", orderId).eq("user_id", user.id).maybeSingle();
    if (error) return Response.json({ error: "order_query_failed" }, { status: 502, headers: cors });
    if (!data) return Response.json({ error: "order_not_found" }, { status: 404, headers: cors });
    return Response.json(data, { headers: cors });
  } catch { return Response.json({ error: "invalid_request" }, { status: 400, headers: cors }); }
});
