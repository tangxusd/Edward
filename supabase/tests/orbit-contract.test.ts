import { assert, assertEquals, assertThrows } from "https://deno.land/std@0.224.0/assert/mod.ts";
import { assertCatalogType, assertManifest, assertRequestId, assertSha256 } from "../functions/_shared/orbit_contract.ts";

Deno.test("Orbit 目录合同拒绝未知类型和非法哈希", () => {
  assertThrows(() => assertCatalogType("video"));
  assertThrows(() => assertSha256("short"));
  assertSha256("a".repeat(64));
});

Deno.test("Orbit requestId 必须是可审计的稳定标识", () => {
  assertThrows(() => assertRequestId("x"));
  assertRequestId("orbit-test-request-001");
});

Deno.test("Orbit manifest 保留增量包结构", () => {
  const manifest = assertManifest({ revision: "r1", fullHash: "a".repeat(64), delta: { items: [], removed: [] } });
  assertEquals(manifest.revision, "r1");
  assert(Array.isArray(manifest.delta.items));
  assert(Array.isArray(manifest.delta.removed));
});
