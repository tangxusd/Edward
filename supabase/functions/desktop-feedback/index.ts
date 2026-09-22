import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
const forbidden = /(api[_ -]?key|authorization|bearer\s+|access[_ -]?token|project[_ -]?(path|name|content)|\/Users\/|[A-Z]:\\\\)/i;

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  if (req.method !== "POST") return Response.json({ error: "method_not_allowed" }, { status: 405, headers: cors });
  const auth = req.headers.get("Authorization");
  if (!auth?.startsWith("Bearer ")) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const body = await req.json().catch(() => ({}));
  const message = typeof body.message === "string" ? body.message.trim() : "";
  const diagnostics = body.diagnostics && typeof body.diagnostics === "object" ? body.diagnostics : {};
  if (!['error', 'feedback'].includes(body.kind) || !message || message.length > 4000 || forbidden.test(message) || forbidden.test(JSON.stringify(diagnostics)))
    return Response.json({ error: "invalid_feedback" }, { status: 400, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: auth } } });
  const { data } = await client.auth.getUser();
  if (!data.user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const result = await client.from("desktop_feedback").insert({ user_id: data.user.id, kind: body.kind, message, diagnostics });
  if (result.error) return Response.json({ error: "feedback_unavailable" }, { status: 503, headers: cors });
  return Response.json({ accepted: true }, { status: 201, headers: cors });
});
