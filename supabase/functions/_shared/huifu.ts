import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

export const adminClient = () => createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!);
export class HuifuHttpError extends Error {
  status: number; body: string; requestPath: string;
  constructor(status: number, body: string, requestPath = "") {
    super(`huifu_http_${status}`);
    this.status = status;
    this.body = body.slice(0, 1000);
    this.requestPath = requestPath;
  }
}
const apiUrl = (Deno.env.get("HUIFU_API_URL") || "https://api.huifu.com").replace(/\/$/, "");

function encodePrivateKey(value: string): ArrayBuffer {
  const b64 = value.replace(/-----[^-]+-----/g, "").replace(/\s+/g, "");
  const bytes = Uint8Array.from(atob(b64), c => c.charCodeAt(0));
  return bytes.buffer;
}
function omitEmptyTopLevel(params: Record<string, unknown>) {
  return Object.fromEntries(Object.entries(params).filter(([, value]) => value !== undefined && value !== null && value !== ""));
}

/** Go encoding/json sorts map keys; this makes the Deno signature text deterministic. */
function sortJson(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(sortJson);
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.entries(value as Record<string, unknown>).sort(([a], [b]) => a.localeCompare(b)).map(([key, item]) => [key, sortJson(item)]));
  }
  return value;
}

export function formatSignSrcText(value: Record<string, unknown>) {
  return JSON.stringify(sortJson(value));
}

export function flattenHuifuData(params: Record<string, unknown>) {
  const base = omitEmptyTopLevel(params);
  const extensions = base.extend_infos;
  delete base.extend_infos;
  if (extensions && typeof extensions === "object" && !Array.isArray(extensions)) {
    Object.assign(base, omitEmptyTopLevel(extensions as Record<string, unknown>));
  }
  return base;
}

async function signText(content: string) {
  const key = await crypto.subtle.importKey("pkcs8", encodePrivateKey(Deno.env.get("HUIFU_RSA_PRIVATE_KEY")!), { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" }, false, ["sign"]);
  return btoa(String.fromCharCode(...new Uint8Array(await crypto.subtle.sign("RSASSA-PKCS1-v1_5", key, new TextEncoder().encode(content)))));
}

async function signData(data: Record<string, unknown>) {
  return signText(formatSignSrcText(data));
}

export async function buildHuifuEnvelope(params: Record<string, unknown>) {
  const data = flattenHuifuData(params);
  return {
    sys_id: Deno.env.get("HUIFU_SYS_ID"),
    product_id: Deno.env.get("HUIFU_PRODUCT_ID"),
    sign: await signData(data),
    data,
  };
}

export async function verifyResponse(params: Record<string, unknown>) {
  const raw = String(params.sign || "");
  if (!raw || !Deno.env.get("HUIFU_PUBLIC_KEY")) return false;
  const b64 = Deno.env.get("HUIFU_PUBLIC_KEY")!.replace(/-----[^-]+-----/g, "").replace(/\s+/g, "");
  const key = await crypto.subtle.importKey("spki", Uint8Array.from(atob(b64), c => c.charCodeAt(0)), { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" }, false, ["verify"]);
  const data = (params.data && typeof params.data === "object") ? params.data as Record<string, unknown> : params;
  return crypto.subtle.verify("RSASSA-PKCS1-v1_5", key, Uint8Array.from(atob(raw), c => c.charCodeAt(0)), new TextEncoder().encode(formatSignSrcText(data)));
}
export async function huifuPost(path: string, payload: Record<string, unknown>) {
  const requestPath = path.replace(/^\//, "");
  const envelope = await buildHuifuEnvelope(payload);
  const response = await fetch(`${apiUrl}/${requestPath}`, { method: "POST", headers: { "Content-Type": "application/json", Accept: "application/json" }, body: formatSignSrcText(envelope) });
  const text = await response.text();
  let value: unknown;
  try { value = text.trim() ? JSON.parse(text) : null; } catch { value = null; }
  if (!response.ok) {
    const desc = value && typeof value === "object" ? String((value as Record<string, unknown>).resp_desc || text) : text;
    throw new HuifuHttpError(response.status, desc, requestPath);
  }
  if (!value || typeof value !== "object") throw new HuifuHttpError(response.status, text || "empty_response", requestPath);
  const envelopeResponse = value as Record<string, unknown>;
  if (envelopeResponse.sign && envelopeResponse.data && Deno.env.get("HUIFU_PUBLIC_KEY")) {
    if (!await verifyResponse(envelopeResponse)) throw new HuifuHttpError(response.status, "signature_invalid", requestPath);
  }
  const data = envelopeResponse.data && typeof envelopeResponse.data === "object" ? envelopeResponse.data as Record<string, unknown> : envelopeResponse;
  if (!Object.keys(data).length) throw new HuifuHttpError(response.status, "empty_response", requestPath);
  return data;
}
