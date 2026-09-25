import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { adminClient, redeemCodeOnce } from "../_shared/orbit_transaction.ts";
import { assertRequestId } from "../_shared/orbit_contract.ts";

export const cors = {
  "Access-Control-Allow-Origin": "http://127.0.0.1:7777",
  "Access-Control-Allow-Methods": "POST,OPTIONS",
  "Access-Control-Allow-Headers": "authorization, apikey, content-type, x-request-id",
};

export async function digestCode(code: string): Promise<string> {
  const normalized = code.trim().toUpperCase();
  const bytes = new TextEncoder().encode(normalized);
  const hash = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(hash)].map((part) => part.toString(16).padStart(2, "0")).join("");
}

async function currentUser(req: Request) {
  const authorization = req.headers.get("Authorization");
  if (!authorization?.startsWith("Bearer ")) return null;
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: authorization } } });
  const { data } = await client.auth.getUser();
  return data.user || null;
}

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  if (req.method !== "POST") return Response.json({ error: "method_not_allowed" }, { status: 405, headers: cors });
  try {
    const user = await currentUser(req);
    if (!user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
    const body = await req.json().catch(() => ({}));
    const requestId = String(body.requestId || req.headers.get("x-request-id") || "");
    const code = String(body.code || "").trim();
    assertRequestId(requestId);
    if (!/^[A-Z0-9-]{8,128}$/i.test(code)) return Response.json({ error: "invalid_code" }, { status: 400, headers: cors });
    const result = await redeemCodeOnce(adminClient(), requestId, await digestCode(code), user.id);
    return Response.json(result, { headers: cors });
  } catch (error) {
    const message = error instanceof Error ? error.message : "redemption_failed";
    const status = message.includes("authentication") ? 401 : message.includes("invalid_") ? 400 : message.includes("not_found") ? 404 : 409;
    return Response.json({ error: message }, { status, headers: cors });
  }
});
