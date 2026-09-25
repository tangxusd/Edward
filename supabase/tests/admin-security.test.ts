import { assertEquals } from "https://deno.land/std@0.224.0/assert/mod.ts";
import { validateAdminRequest } from "../functions/_shared/admin_guard.ts";

const valid = { authorization: "Bearer session", origin: "https://admin.edward.uno", csrf: "csrf", requestId: "req_123456" };
Deno.test("管理员请求必须来自白名单来源", () => assertEquals(validateAdminRequest({ ...valid, origin: "https://edward.uno" }, "https://admin.edward.uno"), "origin_not_allowed"));
Deno.test("管理员请求必须带会话和 CSRF", () => {
  assertEquals(validateAdminRequest({ ...valid, authorization: "" }, "https://admin.edward.uno"), "missing_session");
  assertEquals(validateAdminRequest({ ...valid, csrf: "" }, "https://admin.edward.uno"), "missing_csrf");
});
Deno.test("管理员请求必须使用幂等请求号", () => assertEquals(validateAdminRequest({ ...valid, requestId: "x" }, "https://admin.edward.uno"), "invalid_request_id"));
