/**
 * 支付宝 RSA2 签名基础设施。
 *
 * 这里只负责确定性签名/验签，不包含具体产品 API 路径；产品路径和字段
 * 必须在商户后台确认后，交由对应 Edge Function 使用。
 */
function keyBytes(pem: string, label: string): ArrayBuffer {
  const body = pem.replace(new RegExp(`-----BEGIN ${label}-----|-----END ${label}-----`, "g"), "").replace(/\s+/g, "");
  const bytes = Uint8Array.from(atob(body), c => c.charCodeAt(0));
  return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) as ArrayBuffer;
}

function valueText(value: unknown): string {
  if (value === null || value === undefined) return "";
  if (typeof value === "object") return JSON.stringify(value);
  return String(value);
}

/** 支付宝网关合同：按 ASCII 键排序，排除 sign，空值不参与签名；sign_type 参与验签。 */
export function buildAlipaySignContent(params: Record<string, unknown>): string {
  return Object.keys(params)
    .filter(key => key !== "sign" && valueText(params[key]) !== "")
    .sort()
    .map(key => `${key}=${valueText(params[key])}`)
    .join("&");
}

export async function alipaySign(params: Record<string, unknown>, privateKeyPem: string): Promise<string> {
  const key = await crypto.subtle.importKey(
    "pkcs8",
    keyBytes(privateKeyPem, "PRIVATE KEY"),
    { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" },
    false,
    ["sign"],
  );
  const signature = await crypto.subtle.sign("RSASSA-PKCS1-v1_5", key, new TextEncoder().encode(buildAlipaySignContent(params)));
  return btoa(String.fromCharCode(...new Uint8Array(signature)));
}

export async function alipayVerify(params: Record<string, unknown>, signature: string, publicKeyPem: string): Promise<boolean> {
  const key = await crypto.subtle.importKey(
    "spki",
    keyBytes(publicKeyPem, "PUBLIC KEY"),
    { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" },
    false,
    ["verify"],
  );
  return crypto.subtle.verify(
    "RSASSA-PKCS1-v1_5",
    key,
    Uint8Array.from(atob(signature), c => c.charCodeAt(0)),
    new TextEncoder().encode(buildAlipaySignContent(params)),
  );
}

export const ALIPAY_GATEWAY = "https://openapi.alipay.com/gateway.do";

/** 构造网关请求参数；具体 method/biz_content 由已确认的支付宝产品适配器提供。 */
export async function buildAlipayGatewayRequest(
  method: string,
  bizContent: Record<string, unknown>,
  options: { appId: string; privateKeyPem: string; notifyUrl?: string; timestamp?: string },
): Promise<Record<string, string>> {
  if (!method || !options.appId || !options.privateKeyPem) throw new Error("alipay_config_missing");
  const params: Record<string, unknown> = {
    app_id: options.appId,
    method,
    format: "JSON",
    charset: "utf-8",
    sign_type: "RSA2",
    timestamp: options.timestamp || new Date().toISOString().replace("T", " ").replace(/\.\d{3}Z$/, ""),
    version: "1.0",
    biz_content: JSON.stringify(bizContent),
  };
  if (options.notifyUrl) params.notify_url = options.notifyUrl;
  return { ...Object.fromEntries(Object.entries(params).map(([key, value]) => [key, String(value)])), sign: await alipaySign(params, options.privateKeyPem) };
}
