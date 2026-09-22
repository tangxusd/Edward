# 账户邮件流程操作记录

## 2026-09-17：区分注册确认与找回密码

- 目的：避免注册确认邮件被识别为密码重设邮件，并在密码重设时要求两次密码一致。
- 涉及文件：`supabase/functions/auth-register/index.ts`、`supabase/functions/auth-recovery/index.ts`、`third_party/FableCut/server.js`、`supabase/tests/auth-recovery-flow.test.ts`。
- 结果：注册确认邮件跳转到 `?flow=signup`；找回密码邮件跳转到 `?flow=recovery`。落地页仅在 recovery token 有效时显示两次新密码输入；注册确认仅显示确认完成提示。
- 验证：`deno test --allow-read supabase/tests/auth-recovery-flow.test.ts` 3/3 通过；两个认证函数 `deno check` 通过；`node --check third_party/FableCut/server.js` 通过。
- 外部状态：未发送邮件、未创建或修改远程账号、未部署 Supabase Function 或邮件跳转站点。

## 2026-09-17：生产部署

- 目的：将区分后的注册确认与密码找回流程发布到生产环境。
- 外部状态：已部署 Supabase 项目 `naybqwiqgviuzjtemerc` 的 `auth-register`、`auth-recovery` Function；Vercel `edison8/auth-recovery` 已部署并将生产别名切换到 `dpl_4xubjEw3ShGAWNkfnvn524XQqB3E`。
- 结果：Vercel 站点通过 rewrite 转发到 `auth-recovery` Function，不复制匿名公钥或账户页面实现；注册确认和密码找回依据 `flow` 参数进入不同流程。
- 验证：生产别名状态为 `READY`；生产 `auth-register` 对空请求返回 HTTP 400；生产落地页返回双密码确认字段与注册确认分支代码。
- 外部状态限制：未发送邮件、未创建或修改远程账号、未重设密码、未调用支付接口。

## 2026-09-19：持久登录与订阅续订入口

- 结果：注册按钮使用青色强调样式；找回密码紧贴密码输入框下方靠右；登录按钮在有效会话下显示青色并再次点击打开订阅/续订面板。
- 结果：新增订阅/续订面板，按 Supabase 订阅目录加载方案，预留 Supabase Banner，显示订阅状态和起止时间；方案按钮进入支付宝支付订单流程。关闭面板不清除会话，只有“退出登录”主动清除会话。
- 结果：增加订阅目录、Banner 和支付宝订单的本地代理路由；异常会话无法刷新时回到正常登录流程。
- 验证：FableCut 96 项测试、脚本语法检查、`git diff --check` 和 `edward_app` 构建通过；实际 Edward.app PID 91103 持续运行。未调用真实支付接口。

## 2026-09-19：订阅支付二维码与状态修复

- 修复：支付宝二维码返回值既支持二维码 URL，也支持 data/base64，不再把 URL 错误当作 base64；支付请求增加 30 秒超时，失败会在面板中明确显示。
- 修复：二维码与支付提示改为面板内水平居中、上下两行，提示使用橙色。
- 修复：订阅方案同时显示原价、折扣价、折扣比例与订阅周期。
- 修复：订阅状态接口改为调用 `auth-entitlement`，按真实 subscriptions/trial 数据显示“已订阅”和起止周期。
- 验证：FableCut 96 项测试、脚本语法检查、`git diff --check` 和 `edward_app` 构建通过；实际 Edward.app PID 93887 持续运行。未调用真实支付接口。

## 2026-09-19：订阅 Banner 与登录按钮状态

- 资源：将用户提供的 2048×1152 宣传图压缩为项目内 `third_party/FableCut/assets/subscription-banner.jpg`，输出 1200×675（16:9），订阅面板按固定比例自适应宽度显示。
- 修复：二维码内容若为支付 URL/二维码字符串，分别按图片地址或 QuickChart 二维码地址加载；支付超时提升至 60 秒并显示可读错误，不再显示原始 AbortError。
- 修复：有效登录会话通过 `setAuthButton` 持续应用青色 `authenticated` 样式，启动恢复和登录成功都走同一状态更新。
- 验证：认证界面测试通过，桌面包已重建，实际 Edward.app PID 543 持续运行。

## 2026-09-19：订阅面板溢出与二维码回退

- 修复：订阅面板限制为视口高度减边距并启用内部滚动，底部“退出登录/关闭”按钮保持在弹窗内部。
- 修复：支付平台返回的是二维码内容字符串时，不再把支付链接当作图片地址；优先使用二维码服务生成图片，并在失败时切换备用二维码服务。
- 修复：登录状态通过 CSS class 与 inline 样式双重应用，启动恢复和登录成功都会将标题栏按钮切换为青色。
- 验证：FableCut 96 项测试、脚本检查、`git diff --check` 和桌面包构建通过；实际 Edward.app PID 4179 持续运行。

## 2026-09-19：原生认证会话持久化与支付超时诊断

- 修复：登录会话同时写入 Edward 原生 `QSettings`，启动时优先从原生存储恢复，再同步到 WebView；退出登录同时清理两处会话，标题栏状态统一由恢复结果驱动。
- 修复：FableCut 服务对 Supabase 请求提供 30 秒上限；支付宝 Edge Function 对网关请求提供 15 秒上限并返回可定位的超时错误，避免界面无限等待。
- 验证：本机 `defaults read com.Edward.Edward` 已读到 `auth.accessToken`、`auth.refreshToken`、`auth.userJson`；实际调用本地支付代理使用真实会话后，目录接口返回 HTTP 200，支付宝订单请求返回明确的 Supabase 超时 HTTP 504（说明本地代理、会话转发和远端函数路由已接通，阻塞位于远端订单函数/支付宝网关响应）。未部署外部 Function，未重复创建真实订单。

## 2026-09-19：支付状态轮询与登录状态视觉同步

- 修复：支付提示改为“请使用支付宝扫码支付。”；创建订单后每 3 秒查询 `alipay-query-order`，订单变为 paid 后收起二维码，刷新订阅权益和订阅周期。
- 修复：标题栏登录按钮增加 `authenticated` 数据状态和强制青色样式，启动恢复、登录成功和退出登录均统一更新。
- 修复：支付宝查询函数对订单更新和 `apply_paid_order` 逐项检查错误，避免出现“支付成功但订阅周期不增长”的假成功状态。
- 验证：支付状态本地代理使用真实会话可到达远端查询函数（无效订单返回 HTTP 404）；认证与服务测试 20/20、Deno 类型检查、桌面构建通过；实际 Edward.app PID 8093 持续运行。真实支付成功后的远端支付宝回调/订单状态仍需在生产环境完成一次实付验证。

## 2026-09-19：支付宝 Function 生产部署

- 外部状态：已将 `alipay-create-order` 部署为版本 11、`alipay-query-order` 部署为版本 6 到 Supabase 项目 `naybqwiqgviuzjtemerc`，并恢复 `verify_jwt=true`。
- 验证：两个生产 Function 在无 Authorization 请求下均返回 HTTP 401；Edward.app PID 8093 和本地 7777 服务持续运行。

## 2026-09-19：支付异步通知优先

- 调整：移除二维码展示后的 3 秒高频轮询；支付结果以支付宝异步通知更新订单和订阅为准。
- 调整：二维码区域增加“我已完成支付”查询按钮；用户回到 Edward 窗口时只触发一次状态查询，成功后刷新订阅周期并收起支付提示。
- 验证：认证界面测试 4/4、脚本检查、桌面构建通过；实际 Edward.app PID 15430 持续运行。

## 2026-09-19：订单状态 SSE 实时通道

- 调整：支付宝订单创建后由本地服务保持订单事件流，服务端读取支付宝异步通知落库后的订单状态，并通过 SSE 推送 `paid`；前端不再进行 3 秒轮询。
- 调整：用户扫码后保持支付状态连接，支付宝通知到达后立即刷新订阅；`trade.query` 仍保留为服务端状态查询兜底接口。
- 验证：认证/服务测试 20/20、脚本检查、桌面构建通过；SSE 实测返回 `connected` 和心跳；实际 Edward.app PID 17377、本地服务 PID 17404 持续运行。

## 2026-09-19：Supabase 部署与登录状态重试

- 验证：生产 `alipay-create-order` 版本 11、`alipay-query-order` 版本 6 均为 ACTIVE 且 JWT 校验开启；使用真实会话请求无效方案返回 HTTP 404，证明 Function 路由、鉴权和数据库方案查询已接通。
- 诊断：有效方案的 `payment_gateway_timeout` 发生在已部署 Function 调用支付宝网关阶段，不是 Supabase Function 未部署；本地代理和远端 Function 均可达。
- 修复：登录按钮启动时先同步本地会话状态，并在 WebChannel 尚未就绪时于启动后重试认证恢复，确保 `authenticated` 青色样式不会因初始化竞态漏掉。
- 验证：认证测试 4/4、脚本检查、桌面构建通过；实际 Edward.app PID 18149、本地服务 PID 18176 持续运行。

## 2026-09-19：支付状态反馈与登录按钮持续同步

- 修复：点击“我已完成支付”后立即显示“正在查询支付状态，等待支付宝异步通知…”，SSE 连接结束或异常时恢复按钮，避免点击无反馈。
- 修复：登录按钮每 2 秒重新读取持久会话并同步青色状态，同时保留启动后 WebChannel 重试，避免状态初始化竞态。
- 诊断：真实会话请求 `/api/auth/entitlements` 返回 HTTP 200，当前订阅周期已读到 2027-01-14；Supabase Function 路由正常，`payment_gateway_timeout` 位于支付宝网关调用阶段。
- 验证：认证测试 4/4、脚本检查、桌面构建通过；实际 Edward.app PID 19107、本地服务 PID 19134 持续运行。
## 2026-09-19：支付宝扫码支付链路重设计

- 目的：移除本地 SSE 和服务端循环轮询，按支付宝官方 `precreate`、`notify_url`、`trade.query` 合同重建为“异步通知为事实源，查询为轻量兜底”的单链路。
- 研究：查阅支付宝开放平台 `alipay.trade.precreate`、`alipay.trade.query` 与开放平台异步通知文档；二维码创建请求只返回 `qr_code`，浏览器不会直接收到支付宝扫码事件，支付结果必须由商户通知接口验签后落库。
- 改动：新增 `confirm_paid_order` 数据库函数，通知与主动查询共用同一幂等入账入口；前端改为 5 秒起步、最多 15 秒的退避查询，用户点击“我已完成支付”时立即查询；删除 `/api/payment/alipay/events` SSE 路由及其 Supabase REST 循环。
- 涉及文件：`docs/plans/2026-09-19-alipay-payment-redesign.md`、`supabase/migrations/202609190001_alipay_payment_state.sql`、支付宝通知/查询函数、`third_party/FableCut/app.js`、`third_party/FableCut/server.js` 及认证 UI 测试。
- 外部状态：未部署 Supabase 迁移或 Function；部署属于外部生产变更，需在本地验证通过后单独执行。

## 2026-09-19：支付宝支付重设计生产部署

- 操作：按确认执行 `supabase db push --include-all`，应用 `202609190001_alipay_payment_state.sql`；部署 `alipay-notify` v9 与 `alipay-query-order` v7。
- 远端验证：迁移完成；通知 Function GET 返回 405（路由在线且方法受限），查询 Function 无令牌返回 401（鉴权在线）。
- 本地验证：Node 语法、FableCut 认证测试 4/4、支付宝签名测试 3/3、`git diff --check`、`edward_app` 构建均通过。
- 应用状态：重新打开 `build-0.7.0/bin/Edward.app`；Edward 进程与本地 FableCut 服务均可见，`http://127.0.0.1:7777/` 返回 HTTP 200。

## 2026-09-19：支付状态与原生登录按钮根因修复

- 根因：支付界面调用的查询 Function 同时访问支付宝网关，网关超时会阻塞本地订单状态读取；macOS 标题栏“登录”按钮是原生 NSButton，网页 `#btnAuth` 的样式更新不会同步到它。
- 改动：新增 `alipay-order-state` 只读订单状态 Function；二维码状态机改用该接口。新增 `setFablecutAuthState` WebChannel 方法和原生标题栏状态更新，网页登录、启动恢复、退出登录共用同一状态通知入口。
- 验证：订单 `bda86a79-e9e0-4868-b206-ef45c2dc4e1c` 通过新状态 Function 立即返回 `paid`；相关 Node 测试 4/4、构建和差异检查通过。
- 部署：`alipay-order-state` 已部署；未修改支付宝通知与查询 Function 的既有生产版本。

## 2026-09-19：主动核验兜底修正

- 证据：最新订单 `9e092855-6d41-4a18-8728-68ff96e83fa2` 在异步通知未到达时保持 `pending`；调用既有 `alipay-query-order` 后立即返回 `paid`，随后 `alipay-order-state` 立即返回 `paid`。
- 根因：上一版只读状态方案把主动核验完全移除了，导致支付宝通知丢失时“我已完成支付”没有改变事实状态。
- 改动：只读状态仍保持非阻塞；`pending` 时后台触发一次带 12 秒上限的主动核验，手动确认按钮直接触发主动核验并消费 `paid` 结果。

## 2026-09-19：顶部连接状态与支付宝通知地址核对

- 界面：顶部项目名后移除分隔点和 `connected` 文案，仅保留绿色状态点；断开时仍保留可点击的“未连接”恢复入口。
- 支付宝通知地址：创建订单审计记录确认当前 `notify_url` 为 `https://naybqwiqgviuzjtemerc.supabase.co/functions/v1/alipay-notify`。该地址必须在支付宝开放平台应用的服务器异步通知地址中配置，并保持公网 HTTPS；本地 `127.0.0.1` 不能作为通知地址。
- 验证：认证 UI 测试 5/5、构建、差异检查通过；Edward 已重新启动，本地服务 HTTP 200。

## 2026-09-19：认证与计费主链路收敛

- 范围：授权回调不纳入 Edward 当前支付与登录闭环；支付仍使用应用网关异步通知、RSA2 验签、幂等入账和查询兜底。
- 修复：统一订阅权益响应解析，登录成功后的资源状态与订阅面板都从 `subscriptions`/`trial` 对象读取，避免把对象误当数组导致登录后的权益状态静默丢失。
- 验证：FableCut 认证/导出相关测试 9/9、`node --check app.js` 通过。

## 2026-09-19：支付宝订单重试幂等性

- 修复：同一订阅方案的下单请求在网络超时或重复点击后复用同一个客户端幂等键；支付成功后才清除该键。
- 结果：一次用户购买意图只对应一个服务端订单和一张二维码，避免前端重试创建多笔待支付订单。
- 验证：FableCut 认证 UI 测试 5/5、`node --check app.js`、`git diff --check` 通过。

## 2026-09-19：支付宝折扣金额生产部署

- 部署：`alipay-create-order` 已部署至 Supabase 项目 `naybqwiqgviuzjtemerc`，生产版本为 13。
- 安全核验：Function 状态为 `ACTIVE`，`verify_jwt=true`；无令牌 POST 返回 HTTP 401。
- 结果：新创建的支付宝订单将使用服务端统一计算的原价、折扣额和实付金额，并将实付金额传给支付宝预创建订单。

## 2026-09-19：支付宝幂等订单恢复部署

- 修复：同一用户使用相同幂等键再次请求时，服务端将已生成的二维码按 API 合同返回为 `qrCode`；幂等键查询同时限定当前用户。
- 部署：`alipay-create-order` 已更新为生产版本 14，状态 `ACTIVE`，`verify_jwt=true`。
- 验证：Deno 类型检查、FableCut 认证 UI 测试 5/5、差异检查通过。

## 2026-09-19：支付宝失败重试边界

- 规则：网关超时和仍在创建中的订单保留幂等键，确保后续请求回到同一笔支付意图；服务端明确失败、过期或退款的订单允许前端释放幂等键并重新创建订单。
- 部署：`alipay-create-order` 已更新为生产版本 15，状态 `ACTIVE`，`verify_jwt=true`；无令牌请求返回 HTTP 401。
- 验证：Deno 类型检查、FableCut 认证 UI 测试 5/5、差异检查通过。

## 2026-09-19：支付宝交易关闭终态

- 修复：主动核验读到支付宝 `TRADE_CLOSED` 时，将本地待支付订单更新为 `expired`；客户端停止状态读取、收起二维码并释放该方案的幂等键。
- 部署：`alipay-query-order` 已更新为生产版本 8，状态 `ACTIVE`，`verify_jwt=true`；无令牌请求返回 HTTP 401。
- 验证：Deno 类型检查、FableCut 认证 UI 测试 5/5、差异检查通过。

## 2026-09-19：支付宝预创建订单有效期

- 修复：新建的支付宝预创建订单显式携带 `timeout_express=30m`，避免待支付二维码长期有效。
- 部署：`alipay-create-order` 已更新为生产版本 16，状态 `ACTIVE`，`verify_jwt=true`；无令牌请求返回 HTTP 401。
- 验证：Deno 类型检查、FableCut 认证 UI 测试 5/5、差异检查通过。

## 2026-09-19：支付宝待支付订单数据库保护

- 迁移：将 32 笔超过 30 分钟的历史支付宝待支付订单标记为 `expired`，并新增 `(user_id, plan_id)` 的支付宝 `pending` 部分唯一索引。
- 部署：`alipay-create-order` 已更新为生产版本 17，状态 `ACTIVE`，`verify_jwt=true`。
- 验证：远端当前支付宝 `pending` 订单为 0，重复待支付分组为 0；无令牌创建订单请求返回 HTTP 401；FableCut 测试 5/5、脚本检查和差异检查通过。

## 2026-09-19：无二维码订单恢复

- 修复：对超过 30 秒仍没有二维码的 `pending` 订单，下一次同方案下单请求会将其安全标为 `expired`，释放数据库唯一约束；仍在创建中的短暂订单继续返回 `payment_order_pending`，避免并发请求重复创建。
- 部署：`alipay-create-order` 已更新为生产版本 18，状态 `ACTIVE`，`verify_jwt=true`；无令牌请求返回 HTTP 401。
- 验证：Deno 类型检查、FableCut 认证 UI 测试 5/5、差异检查通过。

## 2026-09-19：支付错误提示本地化

- 修复：将 `payment_order_pending`、`payment_order_retryable`、方案不可用、金额无效和配置错误转换为中文可操作提示，并保留超时订单的幂等键。
- 验证：运行中的 Edward 页面已加载新提示；FableCut 认证 UI 测试 5/5、`node --check app.js`、差异检查通过。

## 2026-09-19：过期二维码不再复用

- 修复：创建订单时同时检查已有支付宝 `pending` 订单的创建时间；超过 30 分钟的订单无论是否已有二维码都会先标记为 `expired`，再创建新订单。
- 部署：`alipay-create-order` 已更新为生产版本 19，状态 `ACTIVE`，`verify_jwt=true`；无令牌请求返回 HTTP 401。
- 验证：Deno 类型检查、FableCut 认证 UI 测试 5/5、差异检查通过。

## 2026-09-19：支付状态读取上限

- 修复：二维码状态读取器增加 10 分钟总时限；超时后停止状态请求、释放本地幂等键并提示重新选择方案，避免通知丢失时无限重试。
- 验证：运行中的 Edward 页面已加载 `startedAt` 与超时分支；FableCut 认证 UI 测试 5/5、`node --check app.js`、差异检查通过。

## 2026-09-19：支付宝已支付订单与订阅权益一致性核验

- 只读核验：生产库共有 6 笔 `provider=alipay,status=paid` 订单；其中 5 笔存在对应 `subscription_periods`，另 1 笔为 2026-09-14 的历史订单。
- 历史例外：该订单自身没有周期行，但同一用户/方案的订阅为 `active`，当前周期已延展至 2027-02-14，且已有 4 个连续订阅周期（包括后续支付订单），不存在“已支付但无有效订阅”的记录。
- 结论：当前异步通知/主动查询共用的 `confirm_paid_order -> apply_paid_order` 入账链路未发现新增丢失权益；历史订单—周期关联不完整暂不直接改写生产数据，避免改变已生效的订阅时间线。

## 2026-09-19：历史已支付订单补偿边界

- 试验结论：不能对“已 paid 但缺少周期行”的历史订单直接调用当前 `apply_paid_order`，因为它会以当前订阅末尾为起点追加周期，可能重复延长订阅。
- 修复：生产数据库已恢复 `confirm_paid_order` 的安全幂等行为；只有 `pending -> paid` 才创建周期，已 `paid` 订单不自动重算历史权益。补偿迁移仅作为审计历史保留，随后已由恢复迁移覆盖。
- 数据处理：已撤销本次试验误追加的周期，订阅末日恢复为 2027-02-14；未改变用户既有订阅时间线。
- 后续边界：历史异常只能依据明确的原始支付周期证据人工核对，不能由当前末尾推断。

## 2026-09-20：Edward 应用重启核验

- 操作：结束旧实例后通过 `open -n build-0.7.0/bin/Edward.app` 重新启动，避免直接执行二进制导致应用包环境未初始化。
- 验证：Edward 主进程仍在运行，本地 FableCut 服务监听 `127.0.0.1:7777`，HTTP 返回 200。

## 2026-09-20：季付订单周期核验与界面文案修正

- 只读核验：最近季付订单使用方案 `74d6b9bc-ba55-453a-86f8-92a6bb92fc3c`，方案为 `billing_interval=month`、`billing_interval_count=3`；实际订阅周期为 2026-09-19 至 2026-12-19，后续同方案续费排到 2027-03-19，未发现按月计算的后端错误。
- 修复：订阅方案界面不再直接显示底层 `month · 3 个周期`，统一显示“季付”（月付、半年付、年付同样本地化），避免把三个月方案误认为月付。
- 验证：认证 UI 测试 5/5、`node --check app.js`、差异检查通过；Edward 已重启并由本地服务 HTTP 200 确认。

## 2026-09-20：过期幂等键导致下单不可用

- 根因：无二维码订单过期后，旧订单仍占用原幂等键；重试虽然释放了 `pending`，插入新订单时仍触发唯一约束，Function 返回 `order_unavailable`。
- 修复：当原幂等订单已过期时，服务端为新订单生成新的幂等键；未过期订单仍严格复用原幂等键，保持重复点击安全。
- 部署：`alipay-create-order` 已更新至生产版本 21，状态 `ACTIVE`，`verify_jwt=true`。
- 验证：无令牌请求返回 HTTP 401；Deno 检查、认证 UI 测试 5/5、差异检查通过。

## 2026-09-20：续费起算点与订阅时长规则修正

- 核验：最近年付订单实际周期为 2026-09-19 16:36:29 至 2027-09-19 16:36:29，未出现只增加一天；季付订单按 3 个月落账。
- 修复：`apply_paid_order` 现在读取同一用户/方案的最新订阅（包括 `expired`），未到期时从原 `current_period_end` 续接并保留首次付费的 `current_period_start`；已过期时从本次支付时间重新起算并重置起始时间。
- 时长：`day` 按天、`month × 1` 按月、`month × 3` 按季度、`month × 12` 按年，`year × 1` 按 12 个月计算。
- 部署：迁移 `202609200001_subscription_renewal_anchor.sql` 已同步生产；未改写现有用户订阅时间线。
- 验证：生产年付周期回读正确；认证 UI 测试 5/5、脚本检查和差异检查通过。

## 2026-09-20：统一用户权益时间线

- 根因：原有实现按 plan_id 分别维护月付、季付、年付订阅，导致同一用户存在并列权益时间线，续费没有从统一到期日叠加。
- 修复：apply_paid_order 已改为按用户锁定并选择到期最晚的权益记录；所有方案共享同一到期日。未到期时从统一到期日叠加并保留首次付费时间，已过期时才从本次支付时间重新起算。
- 当前账户校正：按已确认的历史统一到期点 2027-02-14，将年付周期调整为 2027-02-14 至 2028-02-14；唯一有效权益记录的首次付费时间为 2026-09-14，到期时间为 2028-02-14。月付与季付旧记录已标记为历史状态，不再参与权益判定。
- 部署与验证：迁移 202609200002_unified_user_entitlement_timeline.sql 已同步生产；生产回读为 1 条 active 权益记录；认证 UI 测试 5/5、脚本检查、差异检查通过；Edward 重启后本地服务 HTTP 200。

## 2026-09-20：按全部支付事实重建统一权益账期

- 纠正：先前将年付续期回填至 2028-02-14，漏算了两笔已支付季付。该日期不符合“所有支付都叠加到同一权益到期日”的原则。
- 支付事实：首次月付及后续月付共 5 个月；季付 2 笔共 6 个月；年付 1 笔共 12 个月，总计 23 个月。
- 重建：从首次付款 2026-09-14 开始，8 笔已支付订单已重排为无断档的连续周期；当前唯一 active 权益为 2026-09-14 至 2028-08-14。年付周期为 2027-08-14 至 2028-08-14。
- 验证：生产周期序列相邻边界全部相等，断档数为 0；月付、季付、年付旧并列记录均不再参与权益判定。

## 2026-09-20：注册确认邮件错误处理

- 根因：auth-register 把 Supabase signUp 返回的有效 session 误判为 email_confirmation_required，随后删除新用户；前端又直接显示英文错误码。
- 修复：注册 Function 不再删除有效用户；开启邮箱确认时返回确认邮件状态，未开启时保留 session 并直接完成登录；未确认用户重发邮件时检查 resend 错误。
- 界面：注册错误与确认状态统一显示中文；同时修正订阅周期本地化文案中的模板字面量问题。
- 部署与验证：auth-register 生产版本 18、ACTIVE；C++ 原生认证代码成功编译；认证 UI 测试 5/5、Deno 检查、脚本检查、差异检查通过；Edward 已重启并返回 HTTP 200。
- 配置边界：生产当前行为曾返回 session，证实 Supabase Auth 未启用邮箱确认时不会发送确认邮件；若要求注册必须验证邮箱，需要在 Supabase Auth 的 Email Provider 中开启 Confirm email，并确保 SMTP/邮件模板配置有效。

## 2026-09-20：失效注册确认链接恢复

- 根因：Supabase 在确认链接已使用、过期或被新的注册链接替换时，会在 URL hash 中返回 `error_code=otp_expired`；旧链接不能重新变为有效链接。
- 修复：认证落地页识别注册流程的 `otp_expired`，以中文说明失效原因，并提供邮箱输入和“重新发送确认邮件”操作；调用 Supabase `auth.resend` 仅重新签发注册链接，不改变用户登录状态。
- 部署与验证：auth-recovery 已部署生产；直连生产落地页已回读“重新发送确认邮件”；Deno 检查、认证落地页测试 4/4、差异检查通过。

## 2026-09-20：认证生命周期数据合同

- 新增迁移 `202609200003_auth_lifecycle.sql`：待确认注册、密码恢复锁和管理员设备安全审计表由受控 RPC 管理，期限使用数据库 `now()` 加 10 分钟。
- 生产部署：迁移已应用；`auth-register`、`auth-login`、`auth-recover`、`auth-recovery-complete`、`auth-registration-confirm`、`auth-registration-cleanup` 与 `auth-device-enroll` 均为 `ACTIVE`。已配置设备指纹 HMAC、审计加密和清理任务密钥，密钥未写入仓库。
- 过期清理：数据库 `pg_cron` 任务 `edward-auth-registration-cleanup` 每分钟调用 `cleanup_expired_auth_registrations()`；确认时间不晚于过期时刻的账户会转为 active，其余待确认 Auth 用户会删除并释放邮箱。
- 邮件配置：生产 Auth 的确认邮件已开启，`otp_expiry` 已从 3600 秒更新为 600 秒；SMTP、短信和连接池既有配置经 `config diff` 核实未被覆盖。
- 标题栏：登录状态不再由本地缓存决定。FableCut 在启动、点击登录和定期刷新时请求本地 `/api/auth/session`，该路由代理 Supabase `/auth/v1/user`；仅 HTTP 成功后才通知 macOS 原生标题栏切换青色“已登录”。
- 邮件落地页：`auth-recovery` 改为公开 Function，直连生产 URL 返回 HTTP 200 与 Edward 页面；避免 Vercel rewrite 因无法附带 JWT 被网关拦截。
- 旧账户：首次成功登录后，桌面端原生采集设备标识并调用 `auth-device-enroll` 绑定首次设备；已绑定账户不可被后续设备覆盖。
- 验证：Deno 合同测试 14/14、FableCut 认证 UI 测试 5/5、相关 CTest 4/4、Function 类型检查、生产 cron 回读、公开落地页 HTTP 200 均通过。最终邮件闭环仍需使用可访问的独立测试邮箱执行，避免锁定或改动现有用户账户。

## 2026-09-20：注册确认改为用户显式确认

- 根因：生产确认邮件仍使用 Supabase 默认 `{{ .ConfirmationURL }}`；邮件客户端或安全扫描访问链接会立即完成邮箱确认，无法证明是用户本人点击。
- 修复：`supabase/templates/confirmation.html` 改用 `{{ .TokenHash }}`，邮件只进入 Edward 落地页；页面首次打开不再调用 `setSession`，用户点击“确认邮箱”后才调用 `verifyOtp({ token_hash, type: "signup" })`，随后调用 `auth-registration-confirm` 激活 Edward 注册状态。
- 部署：`auth-recovery` 已重新部署；生产 Auth confirmation 模板的 subject/content 已通过 `supabase config push` 更新。
- 验证：生产落地页回读包含 `verifyOtp` 和“确认邮箱”；生产 `config diff` 不再报告 confirmation 模板更新；认证 Deno 测试 16/16 通过。

## 2026-09-20：同设备单账户注册限制

- 规则：同一设备指纹只允许一个 `active` 账户，或一条仍在 10 分钟期限内的 `registration_pending` 注册；已过期且未确认的记录不阻止重新注册。
- 实现：迁移 `202609200004_one_account_per_device.sql` 以设备和邮箱 advisory lock 串行化注册判定，避免并发绕过；`reserve_registration_attempt` 返回 `device_already_registered`，注册 Function 返回中文提示，不暴露设备原始信息。
- 部署与验证：迁移已应用，`auth-register` 生产版本 22、状态 `ACTIVE`；生产 RPC 使用两个不同邮箱和同一探针设备指纹验证，第二次返回 `device_already_registered`。探针记录已清除。

## 2026-09-20：确认页面由 Vercel 静态托管

- 根因：Supabase Edge Function 网关将公开 HTML 响应强制返回为 `Content-Type: text/plain`，并添加 `nosniff`；Vercel 反向代理会保留该响应，因此浏览器将确认页面源码作为文本显示。
- 修复：Vercel 项目改为直接托管 `supabase/auth-recovery-site/index.html`；该页面保留注册显式确认、确认邮件重发、找回密码和两次密码校验逻辑，不再将页面请求重写到 Supabase Function。
- 跨域：`auth-registration-confirm` 与 `auth-recovery-complete` 允许 `https://auth-recovery.vercel.app` 调用，预检已在生产回读 HTTP 200 与对应 `Access-Control-Allow-Origin`。
- 部署与验证：Vercel 生产部署 `dpl_8EKLsQRD3MCDc84pTCUYueGzQa7b` 为 `READY`，域名别名已切换；认证落地页 Deno 测试 9/9、Function 类型检查通过。
## 2026-09-22 认证与计费漏洞修复

- 目的：修复恢复锁超时永久阻塞、恢复完成未验证密码更新、限流失败放行和支付宝方案有效期校验问题。
- 变更：新增 `202609220001_auth_recovery_lock_hardening.sql`；恢复锁查询包含有效期，完成恢复要求 `auth.users.updated_at` 晚于恢复请求；限流查询/写入失败闭锁并优先使用平台可信来源；支付宝下单校验方案生效区间；旧 Qt 注册入口统一解析生命周期中文错误码。
- 计费边界：支付宝订单暂未接入 credit ledger 抵扣，因为当前没有原子扣减函数，避免出现重复抵扣。
- 验证：Supabase 认证、恢复、计费、支付宝签名测试 25/25 通过；桌面端完整构建通过；认证客户端二进制测试通过。
- 生产状态：已确认后部署；`auth-login` v15、`auth-recover` v2、`auth-recovery-complete` v3、`auth-register` v23、`alipay-create-order` v23 均为 ACTIVE；数据库迁移已应用且 dry-run 返回 up to date。

## 2026-09-22 授权额度有效期修复

- 目的：授权状态和手动订单不得展示或计算已过期抵扣额度，并让手动订单与支付宝订单统一检查方案生效区间。
- 变更：`auth-entitlement`、`subscription-create-order` 增加 `expires_at` 和方案有效期过滤；已部署为 v14。
- 验证：新增合同测试通过，相关 Deno 类型检查通过；线上状态为 ACTIVE，未授权请求返回 401。
