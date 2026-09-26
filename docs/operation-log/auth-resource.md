# 认证与资源库操作记录

## 2026-09-17：本地会话到资源库代理验证

- 目的：验证登录后 FableCut 资源库使用同一份 Supabase access token，且本地服务不丢失认证头。
- 涉及文件：`third_party/FableCut/app.js`、`third_party/FableCut/server.js`、`third_party/FableCut/test/auth-ui.test.js`、`third_party/FableCut/test/rest-api.test.js`。
- 结果：新增受控本地 Supabase 替身测试，确认 `Authorization: Bearer local-session-token` 与匿名公钥被转发至 `/functions/v1/resource-catalog`，响应内容透传给编辑器。
- 验证：`cd third_party/FableCut && node --test`，89 项通过；`node --check app.js` 与 `node --check server.js` 通过。
- 外部状态：未创建远程账号、未发送重置邮件、未调用支付接口、未修改远程 Supabase。

## 2026-09-20：文本收藏固定卡与资源插入轨道

- 目的：在“文本 → 我的收藏”提供永久本地的“标题”“字幕”卡片，并让所有资源卡片的“＋”真实插入时间线。
- 涉及文件：`third_party/FableCut/app.js`、`third_party/FableCut/resource-timeline.js`、`third_party/FableCut/index.html`、`third_party/FableCut/test/resource-timeline.test.js`。
- 结果：固定卡只在渲染层前置，未写入 `resourceBrowserState.items`、本地资源缓存或 Supabase 请求；标题使用画布中心 `(0, 0)`，字幕使用竖屏底部 34% 安全区外线的中心坐标 `y = -0.16 × 画布高度`。所有“＋”均在播放头插入 3 秒文本，按 V1、V2…寻找完整空隙，遇到冲突时逐层上移并补建缺失轨道。
- 验证：`cd third_party/FableCut && npm test` 104 项通过；`node --check server.js app.js mcp-server.js resource-timeline.js` 通过；运行中的 Edward 本地服务已返回新的 `resource-timeline.js` 和页面脚本引用。
- 外部状态：未上传资源、未修改 Supabase 数据或远端资源目录。

## 2026-09-22：认证、计费与偏好隔离缺口修复

- 修复：恢复密码开始时撤销既有 Supabase 会话；数据库触发器在密码真正写入前校验十分钟恢复锁，过期恢复不能先改密再完成。
- 修复：支付宝/手动订单的抵扣额度按订单冻结，支付确认后消费，网关超时、失败或过期时释放；支付确认保持幂等。
- 修复：支付审计只保留订单号、交易号、状态、金额和应用标识，不保存原始回调载荷。
- 修复：注册确认邮件重发错误不再被静默忽略；偏好同步按账号范围隔离，并拒绝超过 5000 条的无提示截断。
- 修复：桌面偏好事件的事件键和身份键按账号隔离；会话刷新失败时同时清理本地 Web 与原生持久会话。
- 验证：`cmake --build build-0.7.0 -j2`、`./build-0.7.0/tests/desktop/test_preference_store`、`deno test --no-check --allow-read supabase/tests/auth-recovery-flow.test.ts`、相关 Function `deno check`、`node --check third_party/FableCut/app.js` 和 `git diff --check` 通过。
- 外部状态：未部署 Supabase 迁移或 Function，未修改生产数据。

## 2026-09-22：生产 Supabase 部署与线上回读

- 结果：已将 `202609220002_auth_billing_preference_hardening` 推送到项目 `naybqwiqgviuzjtemerc`；认证、支付宝支付、订阅下单、偏好同步、反馈及订单状态 Function 均部署为 ACTIVE。
- 回读：`supabase migration list --linked` 显示本地与远端 `202609220002` 一致；`auth-recovery` 公开端点 HTTP 200，正文为 Edward 账户页面。
- 说明：Supabase Edge 网关对该公开 HTML Function 的响应头仍回读为 `text/plain`，但确认邮件实际跳转地址为 Vercel 页面，不依赖该 Function 作为浏览器 HTML 页面；未执行订单创建、邮件发送或用户数据修改。

## 2026-09-22：邮件确认页 Vercel 生产同步

- 结果：将 `supabase/auth-recovery-site` 发布到 Vercel 项目 `auth-recovery`，生产部署 ID 为 `dpl_F5pPZeBf9AntDUDGp47i4AKUyVez`，状态 `READY`，别名 `https://auth-recovery.vercel.app` 已指向新部署。
- 验证：`vercel inspect auth-recovery.vercel.app` 回读生产目标为 `production` 且状态为 `Ready`；CLI 页面内容回读受网络响应超时影响，未修改 Supabase 用户或订单数据。
