# Edward 支付宝单次购买与手动续费设计

## 目标

在不依赖独立服务器、不启用自动扣款的前提下，为 Edward 提供支付宝单次购买和到期手动续费。支付宝只负责收款；Supabase Edge Function 负责创建订单、验签通知和发放权益；Edward 只读取服务端结果。

## 范围与边界

- 支持月付、年付等一次性订单，订单成功后授予对应有效期。
- 不把普通收款码跳转或前端成功页视为支付成功。
- 不在客户端保存支付宝私钥，不在 Vercel 静态页面处理支付密钥。
- 本阶段不实现协议支付、周期扣款、免密代扣。

## 业务流程

1. Edward 登录后从 `subscription-catalog` 获取有效套餐。
2. 客户端调用 `alipay-create-order`，携带 `planKey` 和幂等键。
3. Edge Function 校验用户、套餐和金额，生成服务端订单，并调用支付宝服务端下单接口。
4. 返回支付宝收银台跳转参数或订单号；客户端打开支付宝收银台。
5. 支付宝异步通知 `alipay-notify`。函数校验 RSA2 签名、商户号、应用号、订单号、金额和交易状态。
6. 仅在 `TRADE_SUCCESS`/官方等价成功状态且金额完全匹配时，以数据库事务幂等更新订单并创建或延长订阅。
7. Edward 通过 `auth-entitlement` 或订单查询刷新权益；同步跳转结果只用于提示“等待确认”，不直接开通。

## 数据合同

沿用 `subscription_plans`、`orders`、`subscriptions`，补充：

- `orders.provider = 'alipay'`
- `orders.provider_request_id`：商户订单号
- `orders.provider_order_id`：支付宝交易号
- `orders.provider_response`：脱敏后的通知/查询结果
- `orders.status`：`pending`、`paid`、`failed`、`refunded`

通知处理必须以 `provider_order_id` 或商户订单号建立唯一幂等约束；金额使用分为单位比较，禁止浮点比较。

## Supabase 配置

仅在 Supabase Secrets 保存：`ALIPAY_APP_ID`、`ALIPAY_PRIVATE_KEY`、`ALIPAY_PUBLIC_KEY`、`ALIPAY_NOTIFY_URL`、`ALIPAY_GATEWAY`。Vercel 只部署公开收银台/结果页面时，不放任何私钥。

## 无独立服务器可行性

可行。Supabase Edge Functions 负责支付宝 API 请求和异步通知，Supabase 数据库负责订单与权益状态；Vercel 不是必需的支付后端。必须确认支付宝商家账号已开通对应产品，并取得正式的服务端接口、回调地址白名单和 RSA2 密钥配置。

## 失败与安全规则

- 下单超时不得直接标记失败；客户端必须允许按订单号查询。
- 通知签名失败、金额不符、商户号不符、重复通知分别记录为诊断事件，不授予权益。
- 退款或关闭订单必须撤销/缩短对应权益，不能只改客户端显示。
- 未收到异步通知时，前端最多显示“支付处理中”。

## 实施顺序与验收

1. 增加支付宝 provider 适配和 RSA2 签名/验签单元测试。
2. 增加订单字段、唯一索引和通知幂等事务。
3. 实现 `alipay-create-order`、`alipay-notify`、`alipay-query-order`。
4. Edward 套餐页接入支付宝收银台，并显示处理中/成功/失败状态。
5. 使用支付宝沙箱或官方调试环境验证：重复通知只发放一次权益、金额不符拒绝、查询可恢复超时订单、退款后权益撤销。

完成标准：服务端异步通知验签通过且订单金额完全匹配后，用户权益状态可回读；任何只完成前端跳转但未收到可信通知的订单都不能成为有效订阅。
