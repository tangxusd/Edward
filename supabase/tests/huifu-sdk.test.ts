import { assertEquals, assertMatch } from "https://deno.land/std@0.224.0/assert/mod.ts";
import {
  buildHuifuEnvelope,
  formatSignSrcText,
  flattenHuifuData,
} from "../functions/_shared/huifu.ts";

Deno.test("Go SDK 兼容的 JSON 文本按键排序并展平 extend_infos", () => {
  const data = flattenHuifuData({
    z: "last",
    extend_infos: { b: "two", a: "one" },
    empty: "",
    nil: null,
  });
  assertEquals(data, { a: "one", b: "two", z: "last" });
  assertEquals(formatSignSrcText(data), '{"a":"one","b":"two","z":"last"}');
});

Deno.test("请求使用 Go SDK 的 sign/data 外层合同", async () => {
  const old = {
    sys: Deno.env.get("HUIFU_SYS_ID"),
    product: Deno.env.get("HUIFU_PRODUCT_ID"),
  };
  const pair = await crypto.subtle.generateKey(
    { name: "RSASSA-PKCS1-v1_5", modulusLength: 2048, publicExponent: new Uint8Array([1, 0, 1]), hash: "SHA-256" },
    true,
    ["sign", "verify"],
  );
  const privateKey = btoa(String.fromCharCode(...new Uint8Array(await crypto.subtle.exportKey("pkcs8", pair.privateKey))));
  const oldPrivate = Deno.env.get("HUIFU_RSA_PRIVATE_KEY");
  Deno.env.set("HUIFU_SYS_ID", "sys-test");
  Deno.env.set("HUIFU_PRODUCT_ID", "product-test");
  Deno.env.set("HUIFU_RSA_PRIVATE_KEY", privateKey);
  try {
    const envelope = await buildHuifuEnvelope({ req_seq_id: "r1", extend_infos: { channel: "wx" } });
    assertEquals(Object.keys(envelope).sort(), ["data", "product_id", "sign", "sys_id"]);
    assertEquals(envelope.data, { channel: "wx", req_seq_id: "r1" });
    assertMatch(String(envelope.sign), /^[A-Za-z0-9+/]+=*$/);
  } finally {
    old.sys === undefined ? Deno.env.delete("HUIFU_SYS_ID") : Deno.env.set("HUIFU_SYS_ID", old.sys);
    old.product === undefined ? Deno.env.delete("HUIFU_PRODUCT_ID") : Deno.env.set("HUIFU_PRODUCT_ID", old.product);
    oldPrivate === undefined ? Deno.env.delete("HUIFU_RSA_PRIVATE_KEY") : Deno.env.set("HUIFU_RSA_PRIVATE_KEY", oldPrivate);
  }
});
