import { assertEquals } from "https://deno.land/std@0.224.0/assert/mod.ts";

Deno.test("邀请抵扣只计算可用且未过期账本", () => {
  const now = Date.now();
  const entries = [
    { amount: 100, state: "available", expires_at: new Date(now + 86400000).toISOString() },
    { amount: 50, state: "consumed", expires_at: new Date(now + 86400000).toISOString() },
    { amount: 20, state: "available", expires_at: new Date(now - 86400000).toISOString() },
  ];
  const available = entries.filter((entry) => entry.state === "available" && entry.expires_at > new Date(now).toISOString()).reduce((sum, entry) => sum + entry.amount, 0);
  assertEquals(available, 100);
});
