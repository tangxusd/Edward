import { adminClient, cors, requireUser } from "./_shared/billing.ts";
import { huifuPost } from "./_shared/huifu.ts";

function requestId(prefix: string) {
  return `${prefix}${Date.now()}${crypto.randomUUID().replaceAll("-", "").slice(0, 12)}`.slice(0, 32);
}

function recurringEnabled() {
  return Deno.env.get("HUIFU_RECURRING_ENABLED") === "true" && Boolean(Deno.env.get("HUIFU_RECURRING_AUTH_PATH"));
}

export function recurringUnavailable() {
  return Response.json({ error: "recurring_not_configured" }, { status: 503, headers: cors });
}

export async function requireAuthenticated(req: Request) {
  if (req.method === "OPTIONS") return { response: new Response("ok", { headers: cors }) };
  const { user } = await requireUser(req);
  if (!user) return { response: Response.json({ error: "authentication_required" }, { status: 401, headers: cors }) };
  return { user };
}

export { adminClient, cors, huifuPost, requestId, recurringEnabled };
