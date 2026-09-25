# Orbit 颜色、字体、偏好与管理后台实施记录

## 已完成并验证

- 目录合同、RLS、兑换码事务和管理员表迁移已写入 `supabase/migrations/202609250001_orbit_catalogs.sql`、`202609250002_orbit_rewards_admin.sql`。
- 本地目录缓存、项目会话快照、用户级字体安装校验、时间线 M 标记和循环动画已实现。
- 兑换码、邀请码摘要和桌面订阅面板入口已接入；客户端不接触 service role。
- 管理后台入口、Refine 适配器合同、Cloudflare Pages 配置和管理员 API 已建立；API 追加会话、来源、CSRF、幂等号和 `admin_roles` 服务端校验。
- 已通过：资源缓存/字体安装 2 项、风格快照 1 项、标记/动画 2 项、兑换/邀请 3 项、管理员安全 3 项、端到端快照/离线 2 项、`Edward` 可执行目标构建、`npm --prefix admin test`。

## 未完成的外部验证

- 本机 Supabase PostgreSQL 未启动，`supabase db lint --local` 无法连接 `54322`，因此迁移需要在已启动的 Supabase 环境中再执行一次。
- Cloudflare Pages、`admin.edward.uno` DNS 和 Supabase 生产变量尚未从本地部署命令执行；部署前必须配置生产 secrets，并验证管理员账号 `tangxu8@icloud.com` 的 `admin_roles` 记录。
- 全量 `deno test supabase/tests` 会扫描 macOS `._*` 资源文件并触发 Deno 语法错误；本次使用显式 Orbit 测试文件完成验证，未删除这些既有文件。

## 变更门

后续目录、字体、奖励和管理员能力只能通过新的迁移、Edge Function 合同和操作记录追加，不能直接改写已发布 revision 或绕过 RPC/RLS。
