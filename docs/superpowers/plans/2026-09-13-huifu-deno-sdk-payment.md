# 汇付 Deno SDK 支付接入实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Supabase Edge Functions 中按汇付 Go SDK 合同实现微信/支付宝正扫和连续订阅所需支付能力。

**Architecture:** Edward 客户端只调用 Supabase；Deno 公共适配层负责 `{sys_id, product_id, sign, data}` 报文、RSA 签名/验签和诊断；Edge Functions 负责订单、通知和订阅状态，密钥只从 Supabase Secrets 读取。

**Tech Stack:** Deno Edge Functions、TypeScript、Supabase Postgres/RLS、Web Crypto RSA-SHA256、Supabase 定时任务。

## Global Constraints

- 不新增独立服务器，不在 Qt/网页端保存私钥或服务商密钥。
- 请求签名必须与 Go SDK 的 JSON 序列化和外层报文一致。
- 金额以分为单位存储，服务端生成金额和交易类型。
- 异步通知必须验签、幂等，并记录上游 HTTP 状态和原始响应摘要。
- 未经官方文档/调试接口确认的字段不得进入生产调用。

### Task 1: 建立 Deno SDK 核心与固定向量测试

**Files:**
- Modify: `supabase/functions/_shared/huifu.ts`
- Create: `supabase/tests/huifu-sdk.test.ts`

**Interfaces:**
- Produce `formatSignSrcText(value: Record<string, unknown>): string`。
- Produce `buildHuifuEnvelope(data: Record<string, unknown>): Promise<Record<string, unknown>>`。
- Produce `parseHuifuResponse(response: Response): Promise<HuifuResult>`。
- Preserve `huifuPost(path, data)` as the function endpoint callers use.

- [ ] Step 1: Add failing tests for Go SDK JSON escaping, `extend_infos` flattening, envelope shape, RSA signature verification, empty/non-JSON/HTTP-error responses.
- [ ] Step 2: Run `deno test --allow-env --allow-net supabase/tests/huifu-sdk.test.ts` and confirm failures identify the missing envelope behavior.
- [ ] Step 3: Implement JSON serialization matching Go `FormatSignSrcText`, including only `<`, `>` and `&` unescaping; flatten `extend_infos`; sign the serialized `data` object with RSA PKCS#1 v1.5 SHA-256; send `{sys_id, product_id, sign, data}`.
- [ ] Step 4: Parse `{sign,data}`, retain raw body/status, verify response signature when configured, and classify `http_error`, `empty_response`, `invalid_json`, `signature_invalid`, and `business_error`.
- [ ] Step 5: Run the focused test and compare one generated envelope against an equivalent Go SDK fixture.
- [ ] Step 6: Commit `feat: align Huifu Deno envelope with Go SDK`.

### Task 2: 正扫创建与订单诊断

**Files:**
- Modify: `supabase/functions/huifu-native-create/index.ts`
- Create: `supabase/functions/huifu-native-create/channel.ts`
- Modify: `supabase/tests/billing-functions.test.ts`

**Interfaces:**
- `createNativePayment(channel: "wechat" | "alipay", input): Promise<NativePaymentResult>` maps to `T_NATIVE` or `A_NATIVE`.
- Client input cannot override `trans_amt`, `notify_url`, `sys_id`, `product_id`, `acct_id`, or `trade_type`.

- [ ] Step 1: Add tests proving channel mapping, server-owned amount, missing QR field, and upstream HTTP 500 diagnostics.
- [ ] Step 2: Run focused tests and confirm current flat request fails envelope assertions.
- [ ] Step 3: Move channel mapping and official-field allowlist into `channel.ts`; use the corrected SDK core; read QR/link from unwrapped `data`.
- [ ] Step 4: Update order record with request ID, provider response summary, upstream status, and QR/link; return a stable client error containing `requestId` and diagnostic code.
- [ ] Step 5: Run focused tests and a Supabase local function invocation with a stubbed upstream response.
- [ ] Step 6: Commit `feat: support Huifu WeChat and Alipay native payments`.

### Task 3: 查询、关闭/退款和通知闭环

**Files:**
- Create: `supabase/functions/huifu-order-query/index.ts`
- Create: `supabase/functions/huifu-order-close/index.ts`
- Modify: `supabase/functions/huifu-native-notify/index.ts`
- Modify: relevant migration under `supabase/migrations/`
- Modify: `supabase/tests/billing-functions.test.ts`

- [ ] Step 1: Add tests for signed query, close/refund authorization, duplicate notification, invalid signature, and conflicting status transitions.
- [ ] Step 2: Implement query/close/refund using the same envelope core and service-owned order identifiers.
- [ ] Step 3: Make notification processing transactionally idempotent with a unique provider event/request key and monotonic status transitions.
- [ ] Step 4: Run SQL/RLS tests and function tests.
- [ ] Step 5: Commit `feat: close the Huifu order notification loop`.

### Task 4: 连续订阅数据与授权/代扣流程

**Files:**
- Create: migration `supabase/migrations/202609130003_huifu_recurring_subscription.sql`
- Create: `supabase/functions/huifu-subscription-authorize/index.ts`
- Create: `supabase/functions/huifu-subscription-charge/index.ts`
- Create: `supabase/functions/huifu-subscription-cancel/index.ts`
- Modify: `supabase/functions/huifu-native-notify/index.ts`
- Create: `supabase/tests/huifu-subscription.test.ts`

**Interfaces:**
- Authorization stores only provider binding identifiers and rule version.
- Charge accepts a subscription ID and derives amount/period from server-side plan data.

- [ ] Step 1: Add failing tests for authorization-before-charge, duplicate charge idempotency, next-cycle-only cancellation, retry cap, and expired authorization.
- [ ] Step 2: Add tables/constraints for subscription, provider binding, charge attempt, period, and audit records with RLS denying client writes.
- [ ] Step 3: Implement the officially confirmed authorization endpoint (quickbuckle/protocol) and store only opaque provider IDs.
- [ ] Step 4: Implement recurring charge through the officially confirmed withholding endpoint with new request IDs per period.
- [ ] Step 5: Implement cancel semantics and notification reconciliation.
- [ ] Step 6: Run migration, RLS, and function tests; block production if the merchant product is not enabled.
- [ ] Step 7: Commit `feat: add Huifu recurring subscription lifecycle`.

### Task 5: Supabase secrets、定时任务和生产验证

**Files:**
- Modify: `supabase/config.toml`
- Modify: project operation log under `docs/operation-log/`
- Modify: `.edward/acceptance/current.json`

- [ ] Step 1: Add a renewal worker entry point that can be invoked by Supabase Scheduler/pg_cron and refuses client authentication paths.
- [ ] Step 2: Verify required secrets exist without printing values; verify RLS and log redaction.
- [ ] Step 3: Run all relevant Deno/SQL tests and one non-charging signed request fixture.
- [ ] Step 4: Use the Huifu debug tool for a 0.01 yuan test only after the exact production fields and merchant capabilities are confirmed; capture HTTP code, body, request ID and business code.
- [ ] Step 5: Mark acceptance checks only when evidence is available; otherwise record the exact external blocker.
- [ ] Step 6: Commit `test: verify Huifu payment acceptance gates`.
