import { assert, assertStringIncludes } from "https://deno.land/std@0.224.0/assert/mod.ts";

const migration = await Deno.readTextFile(
  new URL("../migrations/202609200003_auth_lifecycle.sql", import.meta.url),
);
const readSource = async (path: string) => {
  try {
    return await Deno.readTextFile(new URL(path, import.meta.url));
  } catch {
    return "";
  }
};
const registerSource = await readSource("../functions/auth-register/index.ts");
const lifecycleSource = await readSource("../functions/_shared/auth-lifecycle.ts");
const cleanupSource = await readSource("../functions/auth-registration-cleanup/index.ts");
const enrollmentSource = await readSource("../functions/auth-device-enroll/index.ts");
const deviceRegistrationLimitMigration = await readSource("../migrations/202609200004_one_account_per_device.sql");

Deno.test("认证生命周期迁移定义十分钟注册与恢复期限", () => {
  assertStringIncludes(migration, "interval '10 minutes'");
  assertStringIncludes(migration, "auth_registration_states");
  assertStringIncludes(migration, "auth_recovery_locks");
  assertStringIncludes(migration, "device_security_audits");
});

Deno.test("认证生命周期只允许受控 RPC 变更状态", () => {
  assertStringIncludes(migration, "security definer");
  assertStringIncludes(migration, "alter table public.auth_registration_states enable row level security");
  assert(!migration.includes("grant all on public.auth_registration_states to anon"));
});

Deno.test("首次注册保留 create 分支而不误判为待确认重试", () => {
  assertStringIncludes(migration, "on conflict (email_normalized) do nothing");
  assertStringIncludes(migration, "select 'create'::text");
});

Deno.test("注册 Function 要求设备序列号和 MAC 且不回传原始设备信息", () => {
  assertStringIncludes(registerSource, "deviceSerial");
  assertStringIncludes(registerSource, "deviceMac");
  assertStringIncludes(lifecycleSource, "确认邮件已发送，请在 10 分钟内确认完成");
  assertStringIncludes(lifecycleSource, "encryptDeviceValue");
  assertStringIncludes(lifecycleSource, "username_taken");
  assertStringIncludes(lifecycleSource, "用户名已被使用");
});

Deno.test("注册清理 Function 删除十分钟过期且未确认的 Auth 用户", () => {
  assertStringIncludes(cleanupSource, "cleanupExpiredRegistrations");
  assertStringIncludes(lifecycleSource, "admin.auth.admin.deleteUser");
  assertStringIncludes(lifecycleSource, "registration_pending");
});

Deno.test("数据库每分钟清理过期注册并保留期限内确认的账户", () => {
  assertStringIncludes(migration, "cleanup_expired_auth_registrations");
  assertStringIncludes(migration, "user_row.email_confirmed_at <= state.expires_at");
  assertStringIncludes(migration, "create extension if not exists pg_cron");
  assertStringIncludes(migration, "'edward-auth-registration-cleanup'");
  assertStringIncludes(migration, "'* * * * *'");
});

Deno.test("旧账户只在成功登录后静默绑定首次设备", () => {
  assertStringIncludes(enrollmentSource, "admin.auth.getUser(token)");
  assertStringIncludes(enrollmentSource, "is(\"device_fingerprint\", null)");
  assertStringIncludes(enrollmentSource, "legacy_device_enrolled");
});

Deno.test("同一设备只能保留一个有效或待确认账户", () => {
  assertStringIncludes(deviceRegistrationLimitMigration, "device_already_registered");
  assertStringIncludes(deviceRegistrationLimitMigration, "device_fingerprint = p_device_fingerprint");
  assertStringIncludes(deviceRegistrationLimitMigration, "state = 'active'");
  assertStringIncludes(deviceRegistrationLimitMigration, "expires_at > now()");
  assertStringIncludes(lifecycleSource, "该设备已注册账号，请直接登录或使用其他设备。");
});
