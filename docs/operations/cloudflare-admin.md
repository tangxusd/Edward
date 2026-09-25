# Orbit 管理后台部署

后台入口固定为 `https://admin.edward.uno`，根域名不承载后台或 auth recovery。Cloudflare Pages 使用 `admin/wrangler.toml`，前端只持有 Supabase 用户会话，不包含 service role。所有 API 请求必须携带 `Authorization`、`X-Request-ID` 和 `X-CSRF-Token`，服务端校验 `admin_roles`、来源白名单和幂等号后再调用管理 RPC。

部署前检查：`npm --prefix admin test`、`npx wrangler pages deploy admin/public --project-name orbit-admin`。生产环境还需在 Cloudflare Pages 配置 Supabase URL、anon key 和 API 路由变量，并在 DNS 将 `admin.edward.uno` 指向 Pages。
