# Orbit 管理后台部署

后台入口固定为 `https://admin.edward.uno`，根域名不承载后台或 auth recovery。Cloudflare Pages 使用 `admin/wrangler.toml`，前端只持有 Supabase 用户会话，不包含 service role。所有 API 请求必须携带 `Authorization`、`X-Request-ID` 和 `X-CSRF-Token`，服务端校验 `admin_roles`、来源白名单和幂等号后再调用管理 RPC。

部署前检查：`npm --prefix admin test`、`npx wrangler pages deploy admin/public --project-name orbit-admin`。生产环境还需在 Cloudflare Pages 配置 Supabase URL、anon key 和 API 路由变量，并在 DNS 将 `admin.edward.uno` 指向 Pages。

2026-09-26 审查修复：前端 `dataProvider` 现拒绝包含路径分隔符、`.` 或查询语法的资源名，避免调用方通过资源参数跳出 `/api/` 路径边界。已新增未发起网络请求的回归测试，并通过 `npm --prefix admin test`。

2026-09-26 登录链路修复：发布顺序必须先部署 `auth-login` Function，再部署 `admin/public`；管理页使用该 Function 获取用户会话，并以 `admin-stats` 验证 `admin_roles`。发布后应从 `https://admin.edward.uno` 使用已配置管理员角色的账户完成一次真实登录验证。

2026-09-26 发布状态：`auth-login` 已部署并通过管理域名 CORS 回读。Pages 已发布 `admin/public` 到项目 `orbit-admin` 的 `main` 分支，发布地址为 `https://6dc82d5b.orbit-admin-3jg.pages.dev`；自定义域名已回读到新的登录页。

2026-09-26 资源分发方案确认：确定采用 Supabase 作为业务事实源、Cloudflare R2/边缘缓存作为字体、颜色、默认偏好公共版本和资源库静态包的分发层。第一阶段不迁移用户个性化偏好、订阅、兑换码、邀请、权限和审计事实。设计合同已写入 `docs/superpowers/specs/2026-09-26-orbit-home-and-admin-functional-design.md` 第 9 节；实现前需完成 R2 对象校验、版本化缓存、签名 URL、用量监控及 p50/p95 HTTP 性能验收。
