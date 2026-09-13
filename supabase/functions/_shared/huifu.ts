import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

export const adminClient = () => createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!);
const apiUrl = (Deno.env.get("HUIFU_API_URL") || "https://api.huifu.com").replace(/\/$/, "");

function encodePrivateKey(value: string): ArrayBuffer {
  const b64 = value.replace(/-----[^-]+-----/g, "").replace(/\s+/g, "");
  const bytes = Uint8Array.from(atob(b64), c => c.charCodeAt(0));
  return bytes.buffer;
}
function canonical(params: Record<string, unknown>) {
  return Object.keys(params).filter(k => k !== "sign" && params[k] !== undefined && params[k] !== null && params[k] !== "")
    .sort().map(k => `${k}=${typeof params[k] === "object" ? JSON.stringify(params[k]) : String(params[k])}`).join("&");
}
async function sign(params: Record<string, unknown>) {
  const key = await crypto.subtle.importKey("pkcs8", encodePrivateKey(Deno.env.get("HUIFU_RSA_PRIVATE_KEY")!), { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" }, false, ["sign"]);
  return btoa(String.fromCharCode(...new Uint8Array(await crypto.subtle.sign("RSASSA-PKCS1-v1_5", key, new TextEncoder().encode(canonical(params))))));
}
export async function huifuPost(path: string, payload: Record<string, unknown>) {
  const request = { ...payload, sys_id: Deno.env.get("HUIFU_SYS_ID"), product_id: Deno.env.get("HUIFU_PRODUCT_ID") };
  const response = await fetch(`${apiUrl}/${path.replace(/^\//, "")}`, { method: "POST", headers: { "Content-Type": "application/json", Accept: "application/json" }, body: JSON.stringify({ ...request, sign: await sign(request) }) });
  const text = await response.text();
  let value: unknown; try { value = JSON.parse(text); } catch { value = { resp_desc: text.slice(0, 500) }; }
  if (!response.ok) throw new Error(`huifu_http_${response.status}`);
  return value as Record<string, unknown>;
}
