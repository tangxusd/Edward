export type NativeChannel = "wechat" | "alipay";

export type NativeChannelConfig = {
  channel: NativeChannel;
  tradeType: "T_NATIVE" | "A_NATIVE";
};

export function nativeChannelConfig(value: unknown): NativeChannelConfig {
  const channel = String(value || "wechat").toLowerCase();
  if (channel === "wechat" || channel === "wx" || channel === "weixin") {
    return { channel: "wechat", tradeType: "T_NATIVE" };
  }
  if (channel === "alipay" || channel === "ali" || channel === "支付宝") {
    return { channel: "alipay", tradeType: "A_NATIVE" };
  }
  throw new Error("unsupported_payment_channel");
}
