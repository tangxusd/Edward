import { assertEquals, assertRejects } from "https://deno.land/std@0.224.0/assert/mod.ts";
import { digestCode } from "../functions/redeem-code/index.ts";

Deno.test("兑换码摘要不区分大小写和首尾空格", async () => {
  assertEquals(await digestCode(" orbit-2026 "), await digestCode("ORBIT-2026"));
});

Deno.test("兑换码摘要产生 64 位十六进制", async () => {
  const digest = await digestCode("ORBIT-TEST");
  assertEquals(digest.length, 64);
  if (!/^[0-9a-f]+$/.test(digest)) throw new Error("invalid_digest");
});
