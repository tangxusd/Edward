import { adminClient, verifyResponse } from "../_shared/huifu.ts";
Deno.serve(async req => {
  if (req.method !== "POST") return new Response("method_not_allowed", { status: 405 });
  try {
    const body = await req.json();
    if (!await verifyResponse(body)) return Response.json({ resp_code: "FAIL" }, { status: 401 });
    const status = String(body.trans_stat || "");
    const requestId = String(body.req_seq_id || "");
    if (!requestId || !["S", "F", "P"].includes(status)) return Response.json({ resp_code: "FAIL" }, { status: 400 });
    const admin = adminClient();
    const next = status === "S" ? "paid" : status === "F" ? "failed" : "pending";
    await admin.from("orders").update({ status: next, provider_order_id: body.party_order_id || body.order_id || null, provider_response: body }).eq("provider_request_id", requestId);
    return Response.json({ resp_code: "00000000" });
  } catch { return Response.json({ resp_code: "FAIL" }, { status: 400 }); }
});
