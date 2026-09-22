# Edward 认证生命周期实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让注册、邮箱确认、登录、找回密码和本地会话受同一服务端状态机控制，并满足 10 分钟有效期、设备限制和全中文反馈。

**Architecture:** Supabase Auth 仅保存身份凭据和签发邮件令牌；新的 PostgreSQL 认证生命周期表与受限 RPC 保存 Edward 业务状态。认证 Edge Functions 是唯一可以变更这些状态的入口。原生端直接向注册/找回 Function 发送硬件序列号与 MAC，网页层不接触原始设备数据；网页层仍负责显示服务器返回的中文业务文案和已确认会话。

**Tech Stack:** C++20/Qt 6、Qt WebChannel、macOS IOKit 与 Unix 网络接口、Supabase PostgreSQL/RLS/Edge Functions/Deno、FableCut Node 本地代理、原生 `assert` 测试与 Deno 测试。

## Global Constraints

- 注册确认和找回邮件有效期严格为 600 秒，所有期限使用服务端时间。
- 原始硬件序列号和物理 MAC 只经 TLS 发往认证 Function；不进入网页 JavaScript、本地设置、普通日志和 API 返回体。
- 原始设备数据服务端加密保存；常规匹配仅使用版本化 HMAC 指纹。
- UI、原生客户端与 Edge Function 对用户均返回中文；禁止透传内部错误码。
- 已存在的会话持久化继续使用；只有服务端确认的 session 可驱动标题栏和订阅入口。
- 不改动支付、订阅时间线、导出、时间线或与认证无关的已有未提交文件。

---

### Task 1: 建立数据库认证生命周期合同

**Files:**
- Create: `supabase/migrations/202609200003_auth_lifecycle.sql`
- Create: `supabase/tests/auth-lifecycle-contract.test.ts`
- Modify: `.edward/acceptance/current.json`
- Modify: `docs/operation-log/auth-recovery.md`

**Interfaces:**
- Produces: `public.auth_registration_states`、`public.auth_recovery_locks`、`public.device_security_audits`。
- Produces: `public.reserve_registration_attempt(...)`、`public.complete_registration_confirmation(...)`、`public.begin_password_recovery(...)`、`public.complete_password_recovery(...)`、`public.auth_login_gate(...)` RPC。
- Consumes: `auth.users(id)` 与 `public.profiles(user_id)`。

- [ ] **Step 1: 写失败的合同测试**

```ts
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
```

- [ ] **Step 2: 运行测试确认失败**

Run: `deno test --allow-read supabase/tests/auth-lifecycle-contract.test.ts`

Expected: FAIL，因为迁移和测试对象尚不存在。

- [ ] **Step 3: 写最小迁移和 RPC 合同**

```sql
create table public.auth_registration_states (
  user_id uuid primary key references auth.users(id) on delete cascade,
  email_normalized text not null unique,
  device_fingerprint text not null,
  state text not null check (state in ('registration_pending', 'active', 'registration_expired')),
  expires_at timestamptz,
  created_at timestamptz not null default now(),
  confirmed_at timestamptz
);

create table public.auth_recovery_locks (
  user_id uuid primary key references auth.users(id) on delete cascade,
  state text not null check (state in ('recovery_locked', 'completed')),
  expires_at timestamptz not null,
  requested_at timestamptz not null default now(),
  completed_at timestamptz
);
```

补齐下列约束和函数：

- 仅 `registration_pending` 行可有未来 `expires_at`；`active` 行 `confirmed_at` 非空。
- `reserve_registration_attempt` 以 `email_normalized` 和设备指纹加锁，返回 `create`、`pending`、`active` 或 `expired_cleanup`，不允许并发创建第二条待确认记录。
- `auth_login_gate` 返回 `active`、`registration_pending`、`recovery_locked` 或 `unknown`，不返回邮箱、设备原值或 Auth 错误。
- `complete_registration_confirmation` 只在 Auth 用户 `email_confirmed_at` 非空时把状态切到 `active`。
- `begin_password_recovery` 创建/替换 10 分钟恢复锁；`complete_password_recovery` 只接受当前 authenticated 用户并关闭锁。
- `device_security_audits` 只存 `device_fingerprint`、两个密文、事件类型、时间和管理员审计元数据；启用 RLS，不给 `anon` 或 `authenticated` 任何策略。
- 所有普通业务表变更由 `security definer` RPC 执行，并固定 `search_path`。

- [ ] **Step 4: 运行合同测试确认通过**

Run: `deno test --allow-read supabase/tests/auth-lifecycle-contract.test.ts`

Expected: PASS。

- [ ] **Step 5: 更新验收与操作记录**

将 `auth-lifecycle-schema` 加入 `.edward/acceptance/current.json`，状态先标为 `pending`；在 `docs/operation-log/auth-recovery.md` 追加迁移目的、表/RPC 名称和未部署状态。

- [ ] **Step 6: Commit**

```bash
git add supabase/migrations/202609200003_auth_lifecycle.sql supabase/tests/auth-lifecycle-contract.test.ts .edward/acceptance/current.json docs/operation-log/auth-recovery.md
git commit -m "feat: add authentication lifecycle schema"
```

### Task 2: 实现服务端设备安全与注册状态机

**Files:**
- Create: `supabase/functions/_shared/auth-lifecycle.ts`
- Create: `supabase/functions/auth-registration-cleanup/index.ts`
- Modify: `supabase/functions/_shared/risk.ts`
- Modify: `supabase/functions/auth-register/index.ts`
- Modify: `supabase/tests/auth-lifecycle-contract.test.ts`

**Interfaces:**
- Consumes: `deviceSerial`、`deviceMac`，但只在 Function 内处理原始值。
- Produces: `registerAccount(input): Promise<{status: 'pending' | 'active'; message: string}>`。
- Produces: `cleanupExpiredRegistrations(now): Promise<number>`。

- [ ] **Step 1: 写失败的注册状态测试**

```ts
Deno.test("注册 Function 要求设备序列号和 MAC 且不回传原始设备信息", () => {
  assertStringIncludes(registerSource, "deviceSerial");
  assertStringIncludes(registerSource, "deviceMac");
  assertStringIncludes(registerSource, "确认邮件已发送，请在 10 分钟内确认完成");
  assert(!registerSource.includes("deviceSerial:") || registerSource.includes("encryptDeviceValue"));
});

Deno.test("注册清理 Function 删除十分钟过期且未确认的 Auth 用户", () => {
  assertStringIncludes(cleanupSource, "admin.auth.admin.deleteUser");
  assertStringIncludes(cleanupSource, "registration_pending");
});
```

- [ ] **Step 2: 运行失败测试**

Run: `deno test --allow-read supabase/tests/auth-lifecycle-contract.test.ts`

Expected: FAIL，因为共享生命周期模块、清理 Function 和设备字段尚未实现。

- [ ] **Step 3: 实现受控设备处理**

在 `_shared/auth-lifecycle.ts` 实现以下接口：

```ts
export type DeviceEvidence = { serial: string; mac: string };
export type LifecycleReply = { status: number; body: Record<string, unknown> };
export function deviceFingerprint(evidence: DeviceEvidence): string;
export function encryptDeviceValue(value: string): Promise<string>;
export function publicAuthMessage(code: string): string;
export async function registerAccount(input: RegisterInput): Promise<LifecycleReply>;
export async function cleanupExpiredRegistrations(now: Date): Promise<number>;
```

`deviceFingerprint` 使用 `DEVICE_FINGERPRINT_HMAC_KEY` 与稳定规范化的 `serial + "\\n" + mac` 计算 HMAC-SHA-256；`encryptDeviceValue` 使用 `DEVICE_AUDIT_ENCRYPTION_KEY` 的 AES-GCM，密文包含密钥版本和随机 nonce。缺少任一 Secret 时 Function 必须拒绝请求并记录安全配置错误，不能降级为 SHA-256 或明文保存。

`registerAccount` 必须按顺序：验证输入和设备格式 → 调用 `reserve_registration_attempt` → 对 `pending` 返回中文且不创建用户 → 对 `expired_cleanup` 先删除旧 Auth 用户并确认成功 → `auth.signUp` → 创建 profile → 绑定 user ID 和审计密文。任一后续步骤失败时仅清理由该请求创建的记录。

- [ ] **Step 4: 实现过期清理 Function**

`auth-registration-cleanup` 只使用 service-role client 查询服务端时间已过 `expires_at` 的 `registration_pending`；对每条记录先确认 `email_confirmed_at`，已确认则调用确认 RPC，否则删除 Auth 用户，最后标记/删除生命周期记录。重复运行必须幂等。

- [ ] **Step 5: 运行 Deno 类型检查与测试**

Run: `deno check supabase/functions/_shared/auth-lifecycle.ts && deno check supabase/functions/auth-register/index.ts && deno check supabase/functions/auth-registration-cleanup/index.ts && deno test --allow-read supabase/tests/auth-lifecycle-contract.test.ts`

Expected: 全部通过。

- [ ] **Step 6: Commit**

```bash
git add supabase/functions/_shared/auth-lifecycle.ts supabase/functions/_shared/risk.ts supabase/functions/auth-register/index.ts supabase/functions/auth-registration-cleanup/index.ts supabase/tests/auth-lifecycle-contract.test.ts
git commit -m "feat: enforce registration lifecycle and device evidence"
```

### Task 3: 实现登录门禁与找回密码恢复锁

**Files:**
- Create: `supabase/functions/auth-recover/index.ts`
- Create: `supabase/functions/auth-recovery-complete/index.ts`
- Create: `supabase/functions/auth-registration-confirm/index.ts`
- Modify: `supabase/functions/auth-login/index.ts`
- Modify: `third_party/FableCut/server.js`
- Modify: `supabase/functions/auth-recovery/index.ts`
- Modify: `supabase/tests/auth-recovery-flow.test.ts`
- Modify: `supabase/tests/auth-lifecycle-contract.test.ts`

**Interfaces:**
- `POST /functions/v1/auth-recover`: `{email, deviceSerial, deviceMac}` → generic Chinese response。
- `POST /functions/v1/auth-recovery-complete`: Bearer recovery session → closes the recovery lock。
- `POST /functions/v1/auth-registration-confirm`: Bearer signup confirmation session → activates the registration state。
- `auth-login` consumes `auth_login_gate` before calling `signInWithPassword`。

- [ ] **Step 1: 写失败测试**

```ts
Deno.test("登录先检查恢复锁和注册确认状态", () => {
  assertStringIncludes(loginSource, "auth_login_gate");
  assertStringIncludes(loginSource, "密码重置进行中，请通过邮件链接完成重设后再登录");
  assertStringIncludes(loginSource, "确认邮件已发送，请在 10 分钟内确认完成");
});

Deno.test("找回 Function 创建恢复锁并使用十分钟重置流程", () => {
  assertStringIncludes(recoverSource, "begin_password_recovery");
  assertStringIncludes(recoverSource, "flow=recovery");
  assertStringIncludes(recoveryPageSource, "auth-recovery-complete");
});
```

- [ ] **Step 2: 运行失败测试**

Run: `deno test --allow-read supabase/tests/auth-recovery-flow.test.ts supabase/tests/auth-lifecycle-contract.test.ts`

Expected: FAIL，因为恢复锁和完成端点尚未存在。

- [ ] **Step 3: 修改登录 Function**

在 `auth-login` 规范化 identifier 后先查 `auth_login_gate`：

```ts
if (gate === "registration_pending") return chinese("confirmation_pending", 403);
if (gate === "recovery_locked") return chinese("recovery_locked", 423);
if (gate !== "active") return chinese("invalid_credentials", 401);
```

只在 gate 为 `active` 时调用 `signInWithPassword`。Function 响应统一为 `{message, code}`，其中 `message` 已是中文；FableCut 和原生端不再拼接/显示 `code`。

- [ ] **Step 4: 实现找回与完成 Function**

`auth-recover` 接收原生端直接提交的设备证据，检查已确认账户的设备指纹后调用 `begin_password_recovery`，再调用 Supabase 恢复邮件接口。对不存在账户、设备不匹配和有效请求均返回相同中文成功文案，详细原因只写脱敏安全事件。

`auth-recovery-complete` 验证 Bearer token 对应的 recovery user，调用 `complete_password_recovery`。成功后返回 `{message: "密码已更新，请返回 Edward 登录"}`。`auth-registration-confirm` 验证 Bearer token 对应的用户已经具备 `email_confirmed_at`，再调用 `complete_registration_confirmation`；它不签发或保存 Edward 本地登录 session。

认证落地页成功 `updateUser` 后必须调用恢复完成 Function；失败时保持恢复锁，不得错误解锁。注册确认页必须先 `setSession`，再调用注册确认完成 Function，而不是仅显示静态成功文案。

- [ ] **Step 5: 调整本地代理与页面错误映射**

将 `third_party/FableCut/server.js` 的 `/api/auth/recover` 路由由 `/auth/v1/recover` 改为 `/functions/v1/auth-recover`，并保证只把已翻译的 `message` 转给界面。所有 `throw new Error(value.error...)` 改为读取 `value.message` 或固定中文兜底。

- [ ] **Step 6: 验证**

Run: `node --check third_party/FableCut/server.js && deno check supabase/functions/auth-login/index.ts && deno check supabase/functions/auth-recover/index.ts && deno check supabase/functions/auth-recovery-complete/index.ts && deno check supabase/functions/auth-registration-confirm/index.ts && deno test --allow-read supabase/tests/auth-recovery-flow.test.ts supabase/tests/auth-lifecycle-contract.test.ts`

Expected: 全部通过。

- [ ] **Step 7: Commit**

```bash
git add supabase/functions/auth-login/index.ts supabase/functions/auth-recover/index.ts supabase/functions/auth-recovery-complete/index.ts supabase/functions/auth-registration-confirm/index.ts supabase/functions/auth-recovery/index.ts third_party/FableCut/server.js supabase/tests
git commit -m "feat: gate login during password recovery"
```

### Task 4: 原生端安全采集设备信息并直连注册/找回

**Files:**
- Create: `src/resources/include/edward/resources/device_identity.hpp`
- Create: `src/resources/src/device_identity.cpp`
- Create: `src/resources/src/device_identity_macos.mm`
- Create: `tests/resources/test_device_identity.cpp`
- Modify: `src/resources/CMakeLists.txt`
- Modify: `src/resources/include/edward/resources/supabase_auth_client.hpp`
- Modify: `src/resources/src/supabase_auth_client.cpp`
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/src/main.cpp`
- Modify: `tests/resources/CMakeLists.txt`
- Modify: `tests/resources/test_supabase_auth_registration.cpp`

**Interfaces:**
- Produces: `DeviceIdentity DeviceIdentityCollector::collect()`，含 `serial`、`mac` 或中文错误。
- Produces: `WorkbenchRuntime::submitFablecutRegistration(email, password, username)` 与 `submitFablecutPasswordRecovery(email)` Q_INVOKABLE 方法。
- Produces: `authenticationOperationCompleted(operation, success, message)` Qt signal；不携带硬件值。

- [ ] **Step 1: 写失败测试**

```cpp
const auto normalized = edward::resources::normalizeDeviceIdentity(" SERIAL-1 ", "aa-bb-cc-dd-ee-ff");
assert(normalized.serial == "SERIAL-1");
assert(normalized.mac == "AA:BB:CC:DD:EE:FF");
assert(!edward::resources::normalizeDeviceIdentity("", "AA:BB:CC:DD:EE:FF").valid());
```

并在 `test_supabase_auth_registration.cpp` 断言注册/找回请求体包含 `deviceSerial`、`deviceMac`，且 C++ 信号只携带中文消息。

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake --build build-0.7.0 --target test_device_identity test_supabase_auth_registration -j4 && ctest --test-dir build-0.7.0 --output-on-failure -R 'resources.(device_identity|supabase_auth_registration)'`

Expected: FAIL，因为设备采集器和新接口尚不存在。

- [ ] **Step 3: 实现设备采集器**

```cpp
struct DeviceIdentity final {
  QString serial;
  QString mac;
  QString error;
  bool valid() const { return !serial.isEmpty() && !mac.isEmpty() && error.isEmpty(); }
};

class DeviceIdentityCollector final {
 public:
  static DeviceIdentity collect();
};
```

macOS 实现通过 IOKit 读取硬件序列号并通过系统网络接口读取非回环的物理 MAC；按规范化函数统一大小写和分隔符。采集失败直接返回错误，不实现随机 ID、IP 或网页指纹回退。

- [ ] **Step 4: 修改原生认证客户端与 WebChannel 桥接**

向 `SupabaseAuthClient` 添加：

```cpp
bool signUpWithDevice(const SupabaseAuthConfig&, const QString& email, const QString& password,
                      const QString& username, const DeviceIdentity&);
bool beginPasswordRecovery(const SupabaseAuthConfig&, const QString& email, const DeviceIdentity&);
```

这两个方法直接调用 Edge Function，不能将设备数据交给 `third_party/FableCut/app.js` 或 Node 代理。`WorkbenchRuntime` 保存仅包含项目 URL 和公开 anon key 的配置，暴露上述两项无设备参数的 Q_INVOKABLE；采集和 HTTP 提交全部在原生层完成。通过 `authenticationOperationCompleted` 仅把成功状态与中文文案发给网页。

- [ ] **Step 5: 更新构建配置和测试**

在 macOS 目标链接 IOKit 与 SystemConfiguration；只为 macOS 注册 Objective-C++ 源。测试只验证规范化、无效输入和请求 JSON，绝不在 CI 读取真实硬件值。

- [ ] **Step 6: 验证**

Run: `cmake --build build-0.7.0 --target edward_app test_device_identity test_supabase_auth_registration -j4 && ctest --test-dir build-0.7.0 --output-on-failure -R 'resources.(device_identity|supabase_auth_registration|supabase_auth_client)'`

Expected: 构建成功，相关测试通过。

- [ ] **Step 7: Commit**

```bash
git add src/resources src/desktop tests/resources
git commit -m "feat: collect device evidence in native auth flow"
```

### Task 5: 对接网页认证界面与会话权威状态

**Files:**
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/settings-client.js`
- Modify: `third_party/FableCut/test/auth-ui.test.js`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/src/native_titlebar_macos.mm`
- Modify: `tests/desktop/test_workbench_titlebar_actions.cpp`

**Interfaces:**
- Consumes: `window.edwardSettings.submitFablecutRegistration`、`submitFablecutPasswordRecovery`、`authenticationOperationCompleted`。
- Produces: `setAuthButton(confirmedSession)` 只接收服务器确认 session。

- [ ] **Step 1: 写失败的 UI 测试**

```js
test("注册只显示十分钟确认提示且不保存 session", async () => {
  assert.match(source, /确认邮件已发送，请在 10 分钟内确认完成/);
  assert.match(source, /submitFablecutRegistration/);
  assert.doesNotMatch(source, /email_already_registered/);
});

test("找回密码使用原生桥接且恢复锁反馈为中文", () => {
  assert.match(source, /submitFablecutPasswordRecovery/);
  assert.match(source, /密码重置进行中，请通过邮件链接完成重设后再登录/);
});
```

- [ ] **Step 2: 运行失败测试**

Run: `node --test third_party/FableCut/test/auth-ui.test.js`

Expected: FAIL，因为 UI 尚未调用原生桥接。

- [ ] **Step 3: 修改认证提交逻辑**

注册和找回按钮不再使用网页 `fetch` 传递设备相关请求：

```js
if (authMode === "register") {
  window.edwardSettings.submitFablecutRegistration(email, password, username);
  return;
}
window.edwardSettings.submitFablecutPasswordRecovery(email);
```

监听原生桥接完成信号，仅显示中文 `message`。注册待确认不写 `localStorage`、不调用 `saveAuthSession`、不切换标题栏；登录成功仍保存 session，随后调用服务器确认接口后才将标题栏切为青色“已登录”。

- [ ] **Step 4: 修正标题栏状态来源**

保持 `updateEdwardTitlebarAuthState` 的青色样式，但只由刷新成功或登录成功后的服务器确认事件调用。会话刷新失败、恢复锁、确认待完成或退出登录都显式调用 `setFablecutAuthState(false)`，防止本地旧 token 显示为已登录。

- [ ] **Step 5: 验证**

Run: `node --test third_party/FableCut/test/auth-ui.test.js && cmake --build build-0.7.0 --target edward_app test_workbench_titlebar_actions -j4 && ctest --test-dir build-0.7.0 --output-on-failure -R 'desktop.workbench_titlebar_actions'`

Expected: 全部通过。

- [ ] **Step 6: Commit**

```bash
git add third_party/FableCut/app.js third_party/FableCut/settings-client.js third_party/FableCut/test/auth-ui.test.js src/desktop/src/workbench_runtime.cpp src/desktop/src/native_titlebar_macos.mm tests/desktop/test_workbench_titlebar_actions.cpp
git commit -m "feat: render auth state from confirmed session"
```

### Task 6: 配置、部署与端到端验收

**Files:**
- Modify: `supabase/config.toml`
- Modify: `supabase/tests/auth-lifecycle-contract.test.ts`
- Modify: `docs/operation-log/auth-recovery.md`
- Modify: `.edward/acceptance/current.json`

**Interfaces:**
- Consumes: 生产 Supabase Auth Email Provider、SMTP、Function Secrets、定时任务。
- Produces: 已部署迁移和 Functions，以及可观察的认证闭环验收记录。

- [ ] **Step 1: 写配置失败测试**

```ts
Deno.test("本地 Supabase 合同声明十分钟邮箱确认", () => {
  assertStringIncludes(configSource, "enable_confirmations = true");
  assertStringIncludes(configSource, "otp_expiry = 600");
});
```

- [ ] **Step 2: 运行失败测试**

Run: `deno test --allow-read supabase/tests/auth-lifecycle-contract.test.ts`

Expected: FAIL，因为当前配置仍为 `enable_confirmations = false` 与 `otp_expiry = 3600`。

- [x] **Step 3: 更新本地合同与生产配置清单**

将 `supabase/config.toml` 的 `[auth.email]` 调整为：

```toml
enable_confirmations = true
otp_expiry = 600
```

在操作记录写明生产 Dashboard 必须同时完成：Confirm email 开启、Email OTP expiry 设为 600、生产 SMTP 可发送、Functions Secrets 已设置 `DEVICE_FINGERPRINT_HMAC_KEY` 和 `DEVICE_AUDIT_ENCRYPTION_KEY`。不得把密钥写入仓库或命令历史。

- [x] **Step 4: 部署前验证**

Run: `deno check supabase/functions/auth-register/index.ts && deno check supabase/functions/auth-login/index.ts && deno check supabase/functions/auth-recover/index.ts && deno check supabase/functions/auth-recovery-complete/index.ts && deno check supabase/functions/auth-registration-confirm/index.ts && deno check supabase/functions/auth-registration-cleanup/index.ts && deno test --allow-read supabase/tests/auth-recovery-flow.test.ts supabase/tests/auth-lifecycle-contract.test.ts && git diff --check`

Expected: 全部通过。

- [ ] **Step 5: 生产部署与配置验证**

部署迁移以及 `auth-register`、`auth-login`、`auth-recover`、`auth-recovery-complete`、`auth-registration-confirm`、`auth-recovery`、`auth-registration-cleanup`。配置每分钟触发 `auth-registration-cleanup`，并以一次测试账户完成：注册 → 10 分钟内确认 → 登录 → 发起找回 → 登录受阻 → 重设密码 → 登录恢复。生产测试账户完成后由管理员删除。

- [ ] **Step 6: 最终应用验证与验收**

Run: `cmake --build build-0.7.0 --target edward_app -j4 && ctest --test-dir build-0.7.0 --output-on-failure -R 'resources.(device_identity|supabase_auth_registration|supabase_auth_client)|desktop.workbench_titlebar_actions'`

启动 `Edward.app`，确认本地服务 HTTP 200，标题栏只有经确认会话才显示青色“已登录”。将 `auth-lifecycle-schema`、`auth-lifecycle-functions`、`auth-lifecycle-native-device`、`auth-lifecycle-e2e` 标记为 `passed`，并在操作记录写入部署版本与可观察结果。

- [ ] **Step 7: Commit**

```bash
git add supabase/config.toml supabase/tests/auth-lifecycle-contract.test.ts docs/operation-log/auth-recovery.md .edward/acceptance/current.json
git commit -m "feat: configure ten-minute authentication lifecycle"
```
