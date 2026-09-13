import { adminClient, cors, requireAuthenticated } from "../huifu-recurring.ts";

Deno.serve(async (req) => {
  const auth = await requireAuthenticated(req);
  if (auth.response) return auth.response;
  try {
    const body = await req.json();
    const admin = adminClient();
    const { data, error } = await admin.from("subscriptions").update({ cancel_at_period_end: true, updated_at: new Date().toISOString() }).eq("id", String(body.subscriptionId || "")).eq("user_id", auth.user.id).in("status", ["active", "past_due"]).select("id,status,current_period_end,cancel_at_period_end").maybeSingle();
    if (error) return Response.json({ error: "subscription_cancel_unavailable" }, { status: 409, headers: cors });
    if (!data) return Response.json({ error: "subscription_not_found" }, { status: 404, headers: cors });
    return Response.json(data, { headers: cors });
  } catch {
    return Response.json({ error: "invalid_request" }, { status: 400, headers: cors });
  }
});
