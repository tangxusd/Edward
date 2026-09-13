import { assertEquals, assertThrows } from "https://deno.land/std@0.224.0/assert/mod.ts";
import { nativeChannelConfig } from "../functions/huifu-native-create/channel.ts";

Deno.test("微信正扫映射为 T_NATIVE", () => {
  assertEquals(nativeChannelConfig("wechat"), { channel: "wechat", tradeType: "T_NATIVE" });
});

Deno.test("支付宝正扫映射为 A_NATIVE", () => {
  assertEquals(nativeChannelConfig("alipay"), { channel: "alipay", tradeType: "A_NATIVE" });
});

Deno.test("不允许未知支付渠道", () => {
  assertThrows(() => nativeChannelConfig("unknown"), Error, "unsupported_payment_channel");
});
