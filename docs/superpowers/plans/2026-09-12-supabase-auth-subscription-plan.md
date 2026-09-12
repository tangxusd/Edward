# Supabase 认证、试用、订阅与推荐实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Edward 中实现由 Supabase Auth、Edge Functions 和数据库状态机驱动的安全注册、登录、试用、订阅、推荐奖励与抵扣体系。

**Architecture:** Supabase Auth 负责邮箱验证、密码和轮换会话；`profiles`、套餐、订单、订阅、试用、推荐和抵扣账本由 Postgres 保存。Edge Functions 是唯一业务入口，Qt 壳只保存短期会话并展示服务端返回的授权结果。

**Tech Stack:** Supabase Postgres/RLS、Supabase Edge Functions、Qt 6/QML、C++ QNetworkAccessManager、Node.js 测试。

## Global Constraints

- 密码只存储在 Supabase Auth，应用数据库不保存密码。
- 试用默认 3 天，可由后台调整为 0 天至任意天数；不绑定支付方式、不自动扣款。
- 每个账号只能获得一次试用，并限制设备/IP 重复试用。
- 推荐用户完成邮箱验证后，任意一次成功订阅付款触发一次奖励。
- 推荐奖励默认 12 个月有效，退款、拒付或风控命中时冻结/撤销。
- 套餐折扣先计算，推荐抵扣后计算；抵扣不可提现或转赠。
- 退订仅影响下一周期，当前周期继续有效且不退款。
- service role key 只能存在 CLI/CI Secret，不进入 Qt、网页或 Git。
- 价格、折扣、试用天数和奖励金额全部由 Supabase 版本化配置提供。

---

### Task 1: 身份资料、套餐规则与订阅账本迁移

**Files:**
- Create: `supabase/migrations/202609120003_auth_subscription.sql`
- Create: `supabase/tests/auth_subscription_rls.sql`
- Modify: `docs/operation-log/fablecut-navigation-tabs.md`

**Interfaces:**
- Produces `profiles`、`subscription_plans`、`subscription_rules`、`subscriptions`、`orders`、`referrals`、`credit_ledger`、`trial_grants`、`risk_events`。
- Exposes RLS predicates for authenticated self-read and service-role-only mutation.

- [ ] **Step 1: Write failing SQL assertions**

```sql
select has_table('public', 'profiles');
select has_table('public', 'subscription_plans');
select policy_allows('profiles', 'anon', 'select', false);
select policy_allows('credit_ledger', 'authenticated', 'insert', false);
```

- [ ] **Step 2: Run assertions and verify failure**

Run: `supabase db reset && psql "$SUPABASE_DB_URL" -f supabase/tests/auth_subscription_rls.sql`

Expected: missing-table or missing-policy failures.

- [ ] **Step 3: Add schema, constraints and indexes**

Implement normalized lowercase username uniqueness, integer currency amounts, plan version/effective windows, immutable credit ledger entries, one-time trial uniqueness per user, referral uniqueness, and risk-event retention fields.

- [ ] **Step 4: Add RLS and grants**

Authenticated users may read only their own profile/subscription/order/referral/ledger/trial/risk summary; client roles cannot insert or update prices, balances, trial state, referral state, order totals, or subscription status. Service role is required for mutations.

- [ ] **Step 5: Run assertions and push migration**

Run: `supabase db push --linked --yes && psql "$SUPABASE_DB_URL" -f supabase/tests/auth_subscription_rls.sql`

Expected: all assertions pass.

- [ ] **Step 6: Commit**

```bash
git add supabase/migrations/202609120003_auth_subscription.sql supabase/tests/auth_subscription_rls.sql docs/operation-log/fablecut-navigation-tabs.md
git commit -m "建立认证订阅与推荐账本模型"
```

### Task 2: 注册、登录、试用和反滥用 Edge Functions

**Files:**
- Create: `supabase/functions/auth-register/index.ts`
- Create: `supabase/functions/auth-login/index.ts`
- Create: `supabase/functions/auth-entitlement/index.ts`
- Create: `supabase/functions/_shared/risk.ts`
- Create: `supabase/tests/auth-functions.test.ts`

**Interfaces:**
- `POST auth-register({email,password,username,referralCode,deviceHash})` → `{userId,emailVerificationRequired}`。
- `POST auth-login({identifier,password,deviceHash})` → `{access_token,refresh_token,expires_in,user}`。
- `GET auth-entitlement` → `{trial,subscriptions,credits,serverTime}`。

- [ ] **Step 1: Write failing tests**

Cover duplicate normalized username, invalid password, unverified email, repeated trial, excessive IP/device failures, unknown identifier, and successful email login.

- [ ] **Step 2: Run `deno test supabase/tests/auth-functions.test.ts` and verify failure**

- [ ] **Step 3: Implement deterministic validation and risk checks**

Use bounded request bodies, generic login failure responses, per-IP and per-identifier sliding windows, exponential cooldown, device hash retention limits, and no password/token logging. Resolve username to Auth email only inside the function.

- [ ] **Step 4: Implement atomic registration transaction**

Create Auth user, profile, referral binding, and trial eligibility through one server-controlled flow; if risk or uniqueness fails, do not grant trial.

- [ ] **Step 5: Run tests and deploy**

Run: `deno test supabase/tests/auth-functions.test.ts`; deploy each function with `supabase functions deploy <name> --no-verify-jwt` and verify anonymous requests return the documented errors.

- [ ] **Step 6: Commit**

```bash
git add supabase/functions supabase/tests/auth-functions.test.ts
git commit -m "增加安全注册登录与试用授权函数"
```

### Task 3: 订阅、折扣、推荐奖励和抵扣函数

**Files:**
- Create: `supabase/functions/subscription-catalog/index.ts`
- Create: `supabase/functions/subscription-create-order/index.ts`
- Create: `supabase/functions/subscription-cancel/index.ts`
- Create: `supabase/functions/referral-credit/index.ts`
- Create: `supabase/functions/_shared/billing.ts`
- Create: `supabase/tests/billing-functions.test.ts`

**Interfaces:**
- `GET subscription-catalog` → active plans and effective prices.
- `POST subscription-create-order({planKey,continuous,creditAmount})` → immutable order price snapshot.
- `POST subscription-cancel` → marks `cancel_at_period_end=true` only.
- Internal `POST referral-credit({orderId})` → idempotent credit ledger entry.

- [ ] **Step 1: Write failing tests**

Assert discount-before-credit math, credit cap, expired credit rejection, any-paid-subscription referral trigger, duplicate callback idempotency, refund reversal, and next-period-only cancellation.

- [ ] **Step 2: Run tests and verify failure**

Run: `deno test supabase/tests/billing-functions.test.ts`

- [ ] **Step 3: Implement server-side price snapshots**

Read current plan/rule versions, calculate integer amounts, save original/discount/credit/paid fields, and reject all client-provided totals.

- [ ] **Step 4: Implement referral and credit ledger state machine**

Grant once after verified email plus any successful payment; freeze on refund/dispute/risk; expire after configured duration; consume ledger rows transactionally during renewal.

- [ ] **Step 5: Run tests and deploy functions**

Run tests, deploy functions, and replay the same order/callback twice to confirm one ledger mutation.

- [ ] **Step 6: Commit**

```bash
git add supabase/functions supabase/tests/billing-functions.test.ts
git commit -m "实现订阅折扣推荐奖励与抵扣"
```

### Task 4: Qt 登录、注册与订阅状态界面

**Files:**
- Modify: `src/resources/include/edward/resources/supabase_auth_client.hpp`
- Modify: `src/resources/src/supabase_auth_client.cpp`
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/qml/Workbench.qml`
- Create: `tests/resources/test_supabase_auth_registration.cpp`

**Interfaces:**
- `SupabaseAuthClient::signUpWithPassword(config,email,password)`。
- `WorkbenchRuntime::signUpWithSupabase(projectUrl,anonKey,email,password)`。
- QML dialog supports login/register mode, generic failure text, cooldown countdown, current plan and trial expiry.

- [ ] **Step 1: Add failing C++ request tests**

Assert HTTPS-only URL, anon-key requirement, non-empty credentials, and signup endpoint/body without logging password.

- [ ] **Step 2: Run targeted test and verify failure**

Run: `cmake --build build -j4 --target test_supabase_auth_client && build/tests/resources/test_supabase_auth_client`

- [ ] **Step 3: Implement signup and login mode UI**

Use the existing Edward dark dialog tokens; show “邮箱或账号名”, password, register link, verification message, and generic login errors that do not reveal whether an identifier exists.

- [ ] **Step 4: Add session refresh and entitlement refresh**

Persist only rotated session tokens in `AuthSessionStore`; refresh before expiry; request entitlement state from the server and disable resource actions when expired.

- [ ] **Step 5: Build and run tests**

Run: `cmake --build build -j4`; run the auth test target and launch `build/bin/Edward.app/Contents/MacOS/Edward`.

- [ ] **Step 6: Commit**

```bash
git add src/resources src/desktop tests/resources/test_supabase_auth_registration.cpp
git commit -m "接入 Edward 注册登录与订阅状态界面"
```

### Task 5: 资源访问授权与防绕过

**Files:**
- Modify: `supabase/functions/resource-catalog/index.ts`
- Modify: `supabase/functions/resource-detail/index.ts`
- Modify: `supabase/functions/resource-favorite/index.ts`
- Modify: `third_party/FableCut/server.js`
- Create: `supabase/tests/resource-entitlement.test.ts`

**Interfaces:**
- Every resource function accepts JWT and calls shared entitlement checks.
- Local proxy forwards only browser Authorization and never substitutes service credentials.

- [ ] **Step 1: Add failing entitlement tests**

Cover anonymous, trialing, active, expired, revoked, required-plan mismatch, signed URL expiry, and forged resource ID.

- [ ] **Step 2: Run tests and verify failure**

Run: `deno test supabase/tests/resource-entitlement.test.ts`

- [ ] **Step 3: Add shared entitlement guard and signed URL limits**

Reject expired access before querying private Storage; issue only short-lived URLs and return minimal metadata.

- [ ] **Step 4: Run deployment and security tests**

Deploy modified functions and rerun tests; verify direct anonymous REST/Storage access remains denied.

- [ ] **Step 5: Commit**

```bash
git add supabase/functions supabase/tests/resource-entitlement.test.ts third_party/FableCut/server.js
git commit -m "强化资源订阅授权与防绕过"
```

### Task 6: 全链路验收与文档

**Files:**
- Create: `supabase/tests/security-performance.test.ts`
- Modify: `docs/superpowers/specs/2026-09-12-supabase-auth-subscription-design.md`
- Modify: `docs/operation-log/fablecut-navigation-tabs.md`
- Create: `.edward/acceptance/auth-subscription-20260912.json`

- [ ] **Step 1: Run database, Edge Function and C++ tests**

Run: `supabase db push --linked --yes`; `deno test supabase/tests`; `cmake --build build -j4`; `node --test tools/resource-publisher/test/publish.test.mjs`.

- [ ] **Step 2: Run Qt smoke test**

Launch the Qt app, open login/register, verify generic errors, session state, trial/plan display, and resource denial after entitlement expiry.

- [ ] **Step 3: Run abuse scenarios**

Replay registration, login failure, trial grant, referral callback, refund and credit consumption requests; confirm rate limits, idempotency and no duplicate rewards.

- [ ] **Step 4: Record acceptance evidence**

Write command outputs, deployed function names, migration IDs and known external setup values; mark only passing checks as `passed`.

- [ ] **Step 5: Commit**

```bash
git add supabase/tests docs .edward/acceptance/auth-subscription-20260912.json
git commit -m "完成认证订阅安全验收记录"
```
