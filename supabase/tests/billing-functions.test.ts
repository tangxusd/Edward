import { assertEquals } from "https://deno.land/std@0.224.0/assert/mod.ts";
import { calculateOrderAmounts } from "../functions/_shared/billing.ts";

Deno.test("折扣先于推荐抵扣计算", () => {
  assertEquals(calculateOrderAmounts(10000, 20, 3000, 2500), {
    original: 10000, discountAmount: 2000, creditAmount: 2500, paidAmount: 5500,
  });
});

Deno.test("抵扣不会超过折后金额且负数输入归零", () => {
  assertEquals(calculateOrderAmounts(1000, 120, 9999, 9999), {
    original: 1000, discountAmount: 1000, creditAmount: 0, paidAmount: 0,
  });
  assertEquals(calculateOrderAmounts(-1, 10, -20, 100), {
    original: 0, discountAmount: 0, creditAmount: 0, paidAmount: 0,
  });
});
