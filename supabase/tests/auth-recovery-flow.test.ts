import { assert, assertStringIncludes } from "https://deno.land/std@0.224.0/assert/mod.ts";

const registerSource = await Deno.readTextFile(new URL("../functions/auth-register/index.ts", import.meta.url));
const recoverySource = await Deno.readTextFile(new URL("../functions/auth-recovery/index.ts", import.meta.url));
const localProxySource = await Deno.readTextFile(new URL("../../third_party/FableCut/server.js", import.meta.url));
const desktopAuthSource = await Deno.readTextFile(new URL("../../src/resources/src/supabase_auth_client.cpp", import.meta.url));
const vercelRouteSource = await Deno.readTextFile(new URL("../auth-recovery-site/vercel.json", import.meta.url));
const readSource = async (path: string) => {
  try {
    return await Deno.readTextFile(new URL(path, import.meta.url));
  } catch {
    return "";
  }
};
const vercelPageSource = await readSource("../auth-recovery-site/index.html");
const loginSource = await readSource("../functions/auth-login/index.ts");
const recoverSource = await readSource("../functions/auth-recover/index.ts");
const completeSource = await readSource("../functions/auth-recovery-complete/index.ts");
const confirmSource = await readSource("../functions/auth-registration-confirm/index.ts");
const lifecycleSource = await readSource("../functions/_shared/auth-lifecycle.ts");
const configSource = await readSource("../config.toml");
const confirmationTemplate = await readSource("../templates/confirmation.html");
const lifecycleMigration = await readSource("../migrations/202609200003_auth_lifecycle.sql");
const recoveryHardeningMigration = await readSource("../migrations/202609220001_auth_recovery_lock_hardening.sql");
const riskSource = await readSource("../functions/_shared/risk.ts");
const registerFunctionSource = await readSource("../functions/auth-register/index.ts");
const loginFunctionSource = await readSource("../functions/auth-login/index.ts");
const alipayCreateSource = await readSource("../functions/alipay-create-order/index.ts");
const entitlementSource = await readSource("../functions/auth-entitlement/index.ts");
const manualOrderSource = await readSource("../functions/subscription-create-order/index.ts");
const hardeningMigration = await readSource("../migrations/202609220002_auth_billing_preference_hardening.sql");
const preferenceSyncSource = await readSource("../functions/preference-sync/index.ts");

Deno.test("registration confirmation uses its own email return flow", () => {
  assertStringIncludes(lifecycleSource, "https://auth-recovery.vercel.app/?flow=signup");
  assert(!lifecycleSource.includes("emailRedirectTo: \"https://auth-recovery.vercel.app/\""));
});

Deno.test("registration landing page does not auto-confirm email links", () => {
  assertStringIncludes(recoverySource, "token_hash");
  assertStringIncludes(recoverySource, "verifyOtp");
  assertStringIncludes(recoverySource, "确认邮箱");
  assert(!recoverySource.includes('flow === "signup" && type === "signup"'));
});

Deno.test("confirmation email uses TokenHash and requires an explicit page action", () => {
  assertStringIncludes(configSource, "[auth.email.template.confirmation]");
  assertStringIncludes(confirmationTemplate, "{{ .TokenHash }}");
  assertStringIncludes(confirmationTemplate, "flow=signup");
  assert(!confirmationTemplate.includes("{{ .ConfirmationURL }}"));
});

Deno.test("password recovery uses its own email return flow", () => {
  assertStringIncludes(localProxySource, "https://auth-recovery.vercel.app/?flow=recovery");
  assertStringIncludes(desktopAuthSource, "https://auth-recovery.vercel.app/?flow=recovery");
  assert(!localProxySource.includes("body.redirect_to = \"https://auth-recovery.vercel.app/\""));
});

Deno.test("recovery landing page requires matching password confirmation", () => {
  assertStringIncludes(recoverySource, 'id="password-confirm"');
  assertStringIncludes(recoverySource, "两次输入的密码不一致");
  assertStringIncludes(recoverySource, "flow === \"recovery\"");
  assertStringIncludes(recoverySource, "flow === \"signup\"");
  assertStringIncludes(recoverySource, 'id="resend-email"');
  assertStringIncludes(recoverySource, 'errorCode === "otp_expired"');
  assertStringIncludes(recoverySource, "确认邮件已重新发送，请使用最新邮件中的链接");
});

Deno.test("email landing domain serves its own executable HTML page", () => {
  assertStringIncludes(vercelRouteSource, '"cleanUrls": true');
  assert(!vercelRouteSource.includes("functions/v1/auth-recovery"));
  assertStringIncludes(vercelPageSource, "<title>Orbit 账户</title>");
  assertStringIncludes(vercelPageSource, "verifyOtp");
  assertStringIncludes(vercelPageSource, "确认邮箱");
});

Deno.test("公开邮件落地页 Function 不要求 JWT", () => {
  assertStringIncludes(configSource, "[functions.auth-recovery]\nverify_jwt = false");
  assertStringIncludes(recoverySource, '"Content-Type", "text/html; charset=utf-8"');
  assertStringIncludes(recoverySource, 'new Response(html, { headers: htmlHeaders })');
});

Deno.test("登录先检查恢复锁和注册确认状态", () => {
  assertStringIncludes(loginSource, "auth_login_gate");
  assertStringIncludes(loginSource, "密码重置进行中，请通过邮件链接完成重设后再登录");
  assertStringIncludes(loginSource, "确认邮件已发送，请在 10 分钟内确认完成");
});

Deno.test("找回 Function 创建恢复锁并使用十分钟重置流程", () => {
  assertStringIncludes(recoverSource, "begin_password_recovery");
  assertStringIncludes(recoverSource, "flow=recovery");
  assertStringIncludes(completeSource, "complete_password_recovery");
  assertStringIncludes(confirmSource, "complete_registration_confirmation");
  assertStringIncludes(recoverySource, "auth-registration-confirm");
  assertStringIncludes(recoverySource, "auth-recovery-complete");
});

Deno.test("恢复锁会在十分钟后自动解除，且完成接口要求密码已更新", () => {
  assertStringIncludes(recoveryHardeningMigration, "expires_at > now()");
  assertStringIncludes(recoveryHardeningMigration, "user_updated_at");
  assertStringIncludes(completeSource, "if (!completed)");
});

Deno.test("限流使用平台可信来源并在风控存储失败时闭锁", () => {
  assertStringIncludes(registerFunctionSource, "cf-connecting-ip");
  assertStringIncludes(loginFunctionSource, "cf-connecting-ip");
  assertStringIncludes(riskSource, "if (countError) return { allowed: false, hash }");
  assertStringIncludes(riskSource, "if (insertError) return { allowed: false, hash }");
});

Deno.test("支付宝下单只接受当前生效方案", () => {
  assertStringIncludes(alipayCreateSource, ".lte(\"effective_from\", now)");
  assertStringIncludes(alipayCreateSource, "effective_to.is.null,effective_to.gt.");
});

Deno.test("授权和手动下单不使用已过期抵扣额度", () => {
  assertStringIncludes(entitlementSource, "expires_at.is.null,expires_at.gt.");
  assertStringIncludes(manualOrderSource, "expires_at.is.null,expires_at.gt.");
});

Deno.test("恢复锁在密码真正更新前强制检查十分钟期限并撤销会话", () => {
  assertStringIncludes(hardeningMigration, "delete from auth.sessions where user_id = p_user_id");
  assertStringIncludes(hardeningMigration, "before update of encrypted_password on auth.users");
  assertStringIncludes(hardeningMigration, "password_recovery_expired");
});

Deno.test("订单抵扣额度按下单冻结、支付确认消费、失败释放", () => {
  assertStringIncludes(hardeningMigration, "reserve_order_credits");
  assertStringIncludes(hardeningMigration, "state = 'frozen'");
  assertStringIncludes(hardeningMigration, "finalize_order_credits");
  assertStringIncludes(hardeningMigration, "release_order_credits");
  assertStringIncludes(hardeningMigration, "perform public.finalize_order_credits(p_order_id)");
});

Deno.test("偏好同步按账号范围隔离并使用复合冲突键", () => {
  assertStringIncludes(preferenceSyncSource, "account_scope");
  assertStringIncludes(preferenceSyncSource, 'onConflict: "user_id,account_scope,event_id"');
  assertStringIncludes(preferenceSyncSource, '.eq("account_scope", accountScope)');
  assertStringIncludes(preferenceSyncSource, 'count: "exact"');
  assertStringIncludes(preferenceSyncSource, "preference_too_large");
});
