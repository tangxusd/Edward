# Vercel 邮箱验证回调部署

- 时间：2026-09-13
- 目的：为 Supabase 注册/找回密码邮件提供公开 HTTPS 回调页面。
- 变更：部署 `supabase/auth-recovery/index.html` 为 Vercel Project `auth-recovery`，生产地址为 `https://auth-recovery.vercel.app/`；更新 `auth-register` 的注册和重发确认邮件回调地址并重新部署 Edge Function。
- 验证：Vercel CLI 检查项目部署状态为 `READY`；Supabase CLI 报告 `auth-register` 已部署。
- 限制：Vercel 自动 Git 部署未连接，因当前 Vercel 集成账号尚未获得 GitHub 仓库 `tangxusd/Edward` 的管理/写入权限；当前生产部署为 CLI 直接部署。
- 支付接入：桌面端新增登录后“微信支付 0.01 元”入口，调用 Supabase `huifu-native-create`，服务端使用汇付密钥签名并返回微信原生扫码链接；客户端不接触私钥。`huifu-native-create` 已重新部署。
- 验证：`cmake --build build -j2` 成功构建 `Edward.app` 及相关测试目标。
- 支付入口：为避免入口被侧栏滚动区域遮挡，在工作区标题栏增加登录后可见的“微信支付 0.01 元”按钮；点击后生成扫码订单并在侧栏显示扫码链接。
- 支付修复：临时微信验证订单不绑定订阅套餐，新增迁移 `202609130002_allow_standalone_payment_orders.sql` 允许 `orders.plan_id` 为空；已通过 `supabase db push --linked --yes` 应用到生产数据库。
- 诊断修复：支付异常返回安全诊断文本供客户端显示；找回密码改用 Vercel 回调地址并在登录窗口显示“重置邮件已发送”状态。
- 生产诊断：汇付 HTTP 500 的响应正文现在会以截断后的安全诊断返回；回调页再次以 `--prod` 发布并保持别名 `https://auth-recovery.vercel.app/`。
- 生产配置：将 Supabase Secret `HUIFU_DEFAULT_ACCT_ID` 更新为已确认的 `F49973920`，并重新部署 `huifu-native-create`；找回密码客户端请求同时使用 Vercel 回调 URL 参数和请求体。
- 可见性修复：关闭 `auth-recovery` Vercel Project 的 SSO 部署保护，确保 Supabase 邮件跳转无需登录 Vercel；Qt 新增顶层微信支付结果窗口，回显汇付返回的扫码链接。
- 聚合正扫报文修正：`huifu-native-create` 按官方调试报文补充 `combinedpay_data`，使用当前商户号、默认账户和订单金额生成紧凑 JSON 字符串；未伪造风险/设备字段。函数已重新部署。
- 支付故障断言：新增 `HuifuHttpError`，支付函数返回上游 HTTP 状态码、响应正文摘要和请求路径，并在调用前校验生产密钥是否齐全；客户端显示可诊断错误。已重新部署并重新构建 Qt 应用。
- 报文补齐：聚合正扫请求增加 `term_div_coupon_type`、`remark`、`risk_check_data`、`terminal_device_data` 等官方调试报文字段；`acct_split_bunch` 仅接受调用方明确提供的值，避免伪造分账账户。函数已重新部署。
## 2026-09-13 汇付 Deno SDK 适配

- 目的：按官方 Go SDK 修正 Supabase Edge Function 的汇付请求封装，并增加微信/支付宝正扫与连续订阅服务端骨架。
- 变更：`supabase/functions/_shared/huifu.ts` 改为 Go SDK 的 `{sys_id, product_id, sign, data}` 外层合同；签名改为 `data` JSON 文本的 RSA PKCS#1 v1.5 + SHA-256；响应支持 `{sign,data}` 解包和诊断。
- 变更：正扫渠道映射为微信 `T_NATIVE`、支付宝 `A_NATIVE`；新增连续订阅绑定、代扣、下一周期退订函数和数据库表/RLS。
- 验证：Deno SDK、渠道和订阅闸门测试共 6 项通过；Supabase 迁移已推送；4 个函数已部署；未授权请求返回 HTTP 401。
- 外部状态：连续订阅默认关闭，必须先在汇付商户后台确认授权/代扣产品及字段，再设置 `HUIFU_RECURRING_ENABLED=true` 和授权路径；未确认前禁止生产扣款。
