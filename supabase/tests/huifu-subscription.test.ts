import { assertEquals } from "https://deno.land/std@0.224.0/assert/mod.ts";
import { recurringEnabled } from "../functions/huifu-recurring.ts";

Deno.test("连续订阅默认关闭，避免未确认产品直接调用生产接口", () => {
  const oldEnabled = Deno.env.get("HUIFU_RECURRING_ENABLED");
  const oldPath = Deno.env.get("HUIFU_RECURRING_AUTH_PATH");
  try {
    Deno.env.delete("HUIFU_RECURRING_ENABLED");
    Deno.env.delete("HUIFU_RECURRING_AUTH_PATH");
    assertEquals(recurringEnabled(), false);
    Deno.env.set("HUIFU_RECURRING_ENABLED", "true");
    assertEquals(recurringEnabled(), false);
    Deno.env.set("HUIFU_RECURRING_AUTH_PATH", "v3/quickbuckle/apply");
    assertEquals(recurringEnabled(), true);
  } finally {
    oldEnabled === undefined ? Deno.env.delete("HUIFU_RECURRING_ENABLED") : Deno.env.set("HUIFU_RECURRING_ENABLED", oldEnabled);
    oldPath === undefined ? Deno.env.delete("HUIFU_RECURRING_AUTH_PATH") : Deno.env.set("HUIFU_RECURRING_AUTH_PATH", oldPath);
  }
});
