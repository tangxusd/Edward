import test from "node:test";
import assert from "node:assert/strict";
import { createAuthProvider } from "../authProvider.js";

test("未登录管理员必须跳转登录", async () => {
  const result = await createAuthProvider({ apiBase: "https://admin.edward.uno", getSession: async () => null }).check();
  assert.equal(result.authenticated, false);
  assert.equal(result.redirectTo, "/login");
});
test("管理员会话必须通过服务端角色校验", async () => {
  const result = await createAuthProvider({ apiBase: "https://admin.edward.uno", getSession: async () => ({ access_token: "token", csrf: "csrf" }), fetchImpl: async () => new Response("{}", { status: 403 }) }).check();
  assert.equal(result.authenticated, false);
});
