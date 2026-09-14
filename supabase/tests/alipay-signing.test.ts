import { assert, assertEquals } from "https://deno.land/std@0.224.0/assert/mod.ts";
import { alipaySign, alipayVerify, buildAlipaySignContent } from "../functions/_shared/alipay.ts";

Deno.test("支付宝签名文本按键排序并排除 sign/sign_type/空值", () => {
  assertEquals(
    buildAlipaySignContent({ z: "last", app_id: "app", sign: "ignored", sign_type: "RSA2", empty: "", subject: "测试" }),
    "app_id=app&subject=测试&z=last",
  );
});

Deno.test("RSA2 签名可验签且修改字段后失败", async () => {
  const pair = await crypto.subtle.generateKey(
    { name: "RSASSA-PKCS1-v1_5", modulusLength: 2048, publicExponent: new Uint8Array([1, 0, 1]), hash: "SHA-256" },
    true,
    ["sign", "verify"],
  );
  const privateKey = btoa(String.fromCharCode(...new Uint8Array(await crypto.subtle.exportKey("pkcs8", pair.privateKey))));
  const publicKey = btoa(String.fromCharCode(...new Uint8Array(await crypto.subtle.exportKey("spki", pair.publicKey))));
  const toPem = (label: string, value: string) => `-----BEGIN ${label}-----\n${value}\n-----END ${label}-----`;
  const params = { app_id: "app", method: "alipay.trade.page.pay", biz_content: JSON.stringify({ out_trade_no: "o1", total_amount: "0.01" }) };
  const signature = await alipaySign(params, toPem("PRIVATE KEY", privateKey));
  assert(await alipayVerify(params, signature, toPem("PUBLIC KEY", publicKey)));
  assert(!(await alipayVerify({ ...params, app_id: "tampered" }, signature, toPem("PUBLIC KEY", publicKey))));
});
