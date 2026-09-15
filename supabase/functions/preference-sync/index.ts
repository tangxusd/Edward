import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
const cors = { "Access-Control-Allow-Origin": "http://127.0.0.1:7777", "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type" };
const required = ["eventId", "installationId", "componentId", "componentFamily", "componentVersion", "manifestHash", "semanticPath", "propertyPath", "valueType", "value", "creationSessionId", "source"];
function validFact(value: unknown) {
  if (!value || typeof value !== "object") return false;
  const fact = value as Record<string, unknown>;
  if (required.some((key) => typeof fact[key] !== "string" && key !== "value")) return false;
  if (fact.source !== "user-confirmed") return false;
  if (["projectId", "projectPath", "projectName", "timelineId"].some((key) => key in fact)) return false;
  if (fact.valueType === "color") return typeof fact.value === "string" && /^#[0-9a-f]{6}([0-9a-f]{2})?$/i.test(fact.value);
  return ["string", "integer", "float", "boolean"].includes(String(fact.valueType));
}
Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });
  const auth = req.headers.get("Authorization");
  if (!auth?.startsWith("Bearer ")) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  const client = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, { global: { headers: { Authorization: auth } } });
  const { data } = await client.auth.getUser();
  if (!data.user) return Response.json({ error: "authentication_required" }, { status: 401, headers: cors });
  if (req.method === "GET") {
    const result = await client.from("preference_facts").select("fact").eq("user_id", data.user.id).limit(5000);
    if (result.error) return Response.json({ error: "preference_read_failed" }, { status: 500, headers: cors });
    return Response.json({ schemaVersion: 1, manifestVersion: 1, facts: result.data.map((row) => row.fact) }, { headers: cors });
  }
  if (req.method !== "POST") return Response.json({ error: "method_not_allowed" }, { status: 405, headers: cors });
  const body = await req.json().catch(() => ({}));
  if (body.schemaVersion !== 1 || body.manifestVersion !== 1 || !Array.isArray(body.facts) || body.facts.length > 500) return Response.json({ error: "invalid_payload" }, { status: 400, headers: cors });
  const facts = body.facts.filter((fact: unknown) => validFact(fact)) as Record<string, unknown>[];
  if (facts.length !== body.facts.length) return Response.json({ error: "invalid_fact" }, { status: 400, headers: cors });
  const result = await client.from("preference_facts").upsert(facts.map((fact) => ({ user_id: data.user.id, event_id: fact.eventId, fact })), { onConflict: "user_id,event_id", ignoreDuplicates: true });
  if (result.error) return Response.json({ error: "preference_write_failed" }, { status: 500, headers: cors });
  return Response.json({ accepted: facts.length }, { headers: cors });
});
