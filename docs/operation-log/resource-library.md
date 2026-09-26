# 资源库操作记录

## 2026-09-21 组件库生命周期实现

- 目的：实现公共组件的版本化发布、跨设备收藏、分类查询、签名下载、本地完整包缓存、低分辨率 MP4 预览、时间线版本锁定与原生运行时复现。
- 涉及范围：`supabase/migrations/202609210001_component_library_lifecycle.sql`、三个资源 Edge Function、`tools/resource-publisher/`、`third_party/FableCut/` 的缓存、目录、时间线和原生运行时。
- 本地缓存：仅由详情接口返回的短期签名地址下载；缓存身份为 `componentId@version#contentHash`；服务端校验包哈希、清单、运行时入口和登记资产后原子写入。日志只记录组件 ID、版本、哈希与结果，不记录令牌、签名 URL、项目内容或绝对路径。
- 时间线实例：公共组件保存 `resourceId/componentId/version/contentHash/target/cacheKey/source`；固定标题和字幕保存本地来源与中心坐标。公共组件静态属性和关键帧轨道由锁定运行时清单白名单验证。
- 目录逻辑：除“导入”外的 Tab 默认“我的收藏”；固定筛选可与业务分类组合；“标注”不再绕过公共资源目录。卡片只显示名称和收藏数量，本地固定组件显示“本地”。
- 已执行验证：
  - `node --test third_party/FableCut/test/component-versioning.test.js third_party/FableCut/test/native-annotation-runtime.test.js third_party/FableCut/test/resource-cache.test.js third_party/FableCut/test/resource-browser.test.js third_party/FableCut/test/resource-timeline.test.js third_party/FableCut/test/user-components.test.js`
  - `node --test tools/resource-publisher/test/publish.test.mjs`
  - `deno test --allow-read supabase/tests/component-library-contract.test.ts`
  - `node --check third_party/FableCut/server.js && node --check third_party/FableCut/app.js && node --check third_party/FableCut/component-runtime.js && node --check third_party/FableCut/component-versioning.js`
  - 临时本机服务在独立数据目录启动，`/api/project`、`component-versioning.js` 与 `index.html` 均返回 200；验证后已关闭并清理临时目录。
- 外部状态：初始实现完成后已按下文记录部署三个资源 Edge Function；生产 migration 仍未推送。生产端到端收藏、签名下载与 UI 冒烟仍需在数据库迁移历史确认后执行。

## 2026-09-21 生产 Function 部署与迁移前检查

- 已部署：`resource-catalog` v20、`resource-detail` v17、`resource-favorite` v16，均为 ACTIVE。未携带用户凭证的健康请求均返回 401，符合这些接口要求认证的预期。
- 本地固定组件预览：新增 320×180、30 fps、3 秒的 H.264 MP4，分别用于“标题”和“字幕”卡片及预览面板。
- 迁移检查：远端 migration history 包含本地不存在的 `202609200005`。经只读查询，名称为 `fix_resource_version_semver`，且远端 `resource_versions_version_check` 实际定义为三段语义版本正则；已据此恢复同名本地迁移。未执行 `migration repair`，也未修改既有远端迁移历史。
- 生产迁移：`202609210001_component_library_lifecycle.sql` 已成功应用。迁移清单回读确认本地与远端版本全部一致；`resource_favorites_favorite_count`、已发布资源/版本删除保护触发器、`list_favorite_resources` 和 `recount_resource_favorite_counts` 均已存在。
- 数据核对：现有 `resources.favorite_count` 与 `resource_favorites` 聚合的差异资源数为 0，未执行计数重算，也未修改任何用户收藏记录。
- 本机交付验证：以隔离数据目录启动当前工作树的 FableCut 服务，`/assets/resource-previews/title.mp4`、`/assets/resource-previews/subtitle.mp4`、`/component-versioning.js` 和 `/api/project` 均返回 200；两个 MP4 均返回 `video/mp4` 与 Range 支持。验证服务已关闭，隔离数据目录已清理。
- 桌面构建验证：`cmake --build build-0.7.0 --parallel 4` 和 `ctest --test-dir build-0.7.0 --output-on-failure` 均通过（35/35）。桌面运行时从当前工作树的 `third_party/FableCut` 直接提供资源，新增 MP4 不依赖遗漏的打包副本。
- 会话链路审计：前端资源请求统一通过 `fetchResourceApi` 从持久会话取得 access token；本地代理将 Bearer Token 原样转发给 `resource-catalog`、`resource-detail` 和 `resource-favorite`；三个 Function 均以该 JWT 调用 `auth.getUser()` 并在 RLS 上下文中执行，未发现独立或脱节的资源登录状态。

## 2026-09-21 资源库代码审查修复

- 已修复缓存预览持久性：公共组件缓存成功后记录本地缓存预览身份；下次资源目录加载会优先读取本地 `/api/resources/cache-preview`，不再依赖可能过期的远端签名 MP4 URL。
- 已修复缓存完整性绕过：缓存索引记录每个落盘文件哈希；缓存复用、预览读取和运行时入口读取均先校验所有文件、包内容哈希、发布清单、运行时合同、资产哈希和 MP4 文件头。任一不一致即只删除可再生的本地缓存，并从受控详情接口重新获取。
- 已修复收藏边界：收藏 Function 仅允许已发布且 `public/unlisted` 的资源进入收藏表，草稿与私有资源不会被已知 UUID 的请求写入或计数。
- 已修复目录输入与反馈：分类 UUID、页码和页大小在 Function 中严格校验；前端将资源库内部错误码统一映射为中文用户提示。
- 已执行验证：资源缓存、资源浏览器、轨道放置、原生运行时、用户组件测试 28/28 通过；Supabase 资源合同测试 6/6 通过；Node/Deno 语法与 `git diff --check` 通过。
- 外部状态：经用户确认后，已部署新的 `resource-catalog` v21 和 `resource-favorite` v17，均为 ACTIVE；未授权请求分别返回 401 `authentication_required`，未写入用户数据。
- 日志审计：资源目录加载、收藏/取消收藏、缓存请求/失效/失败和本地服务状态现在写入统一 FableCut 诊断通道；缓存仍同时保留 `resource-cache.log` 作为本地辅助日志。事件只包含资源/组件 ID、版本、内容哈希、分类、状态码、数量和耗时，不记录 Token、签名 URL、项目内容或本地绝对路径。
- 向后兼容：`resource-catalog` v20 在远端尚无 `list_favorite_resources` RPC 时，回退读取现有 `resource_favorites` 并保持收藏顺序，避免“我的收藏”直接失败；它不会伪造或更新收藏计数。
- 当前边界：Function 级收藏、目录、详情和缓存仍需要带真实登录会话的桌面端到端验证；生产数据库和 Function 均未用测试账户或模拟收藏写入验证，避免污染用户数据。

## 2026-09-21 组件库代码审查问题修复

- 目的：闭合缓存性能、发布原子性、插入并发、预览回退、实例身份和日志可追踪性缺口。
- 缓存：缓存建立或文件元数据变化时执行完整哈希/运行时/MP4 校验；正常预览 Range 请求只走元数据快速路径；旧索引会在首次读取时完整校验并补写元数据。
- 发布：发布器在上传前解包并验证 `edward-runtime.json`、入口、资产字节数与 SHA-256、MP4 文件头；数据库写入改为受保护的 `publish_resource_version` 事务；数据库失败会清理本次已上传对象。
- 时间线：公共组件缓存完成后重新读取当前项目轨道再计算位置；资源插入请求按队列串行化，避免缓存等待期间的轨道重叠。
- 版本身份：缓存接口向运行时清单注入组件 ID、版本和内容哈希；实例验证同时检查 `resourceRef`、`cacheKey` 与清单三者一致。
- 预览与日志：本地 MP4 失效时清理本地标记并回退签名远端 MP4；详情、缓存、收藏日志增加请求 ID，错误原因限制为脱敏错误码，不再写入文件路径。

## 2026-09-21 资源库复审加固

- 目的：修复跨账户本地缓存暴露、发布运行时合同未绑定、目录返回不完整版本、低分辨率预览未强制验证和资源版本元数据直接读取边界。
- 本地缓存：目录缓存和资源元数据按当前用户隔离；退出或切换账户时清除可再生缓存；已登录会话会获得仅本机、15 分钟有效的 HttpOnly 缓存访问票据，预览和运行时入口缺票据即拒绝。
- 发布和缓存：`edward-runtime.json` 必须与外层清单的作者帧率、总帧数、可编辑属性、关键帧轨道一致；所有允许编辑的属性或轨道根属性必须存在于 `additionalProperties: false` 的 schema 中。
- 预览：发布器通过 `ffprobe` 强制 H.264、偶数尺寸、长边不超过 854、时长不超过 15 秒，且文件不超过 20 MiB；本地标题预览实测为 H.264 320×180、3 秒。
- 目录和数据库：目录只返回同时具有版本、内容哈希和签名 MP4 的资源；新增资源版本 RLS 迁移，使版本元数据只能随已发布且可见资源读取。
- 验证：资源库 Node 测试 42/42、Supabase 合同测试 8/8、`node --check`、`deno check` 和 diff 格式检查通过。
- 待部署：`resource-catalog` Function 与 `202609210003_resource_version_visibility.sql` 尚未部署生产，等待明确部署确认。
- 验证：FableCut 资源相关测试 21/21、资源发布器测试 5/5、Supabase 组件合同测试 7/7；Node/Deno 语法检查通过。新增 Supabase migration 尚未部署，真实登录会话和生产发布事务仍需外部环境验证。

## 2026-09-21 组件库修复生产部署

- 目的：将组件库审查修复应用到生产 Supabase 项目 `naybqwiqgviuzjtemerc`。
- 数据库：执行 `supabase db push --linked`，成功应用 `202609210002_atomic_resource_publish.sql`；迁移列表回读确认本地与远端一致。
- Function：部署 `resource-catalog` v22、`resource-favorite` v18，状态均为 ACTIVE；未修改其他 Function。
- 远端验证：两个 Function 的 OPTIONS 请求均返回 200；未携带凭证的请求均返回 401 `authentication_required`，未写入用户或收藏数据。
- 结果：原子发布 RPC、收藏分类查询和错误分类已上线；真实登录用户的收藏、下载和发布器实际调用仍需使用真实授权会话验证。
## 2026-09-21 缓存目录统一

- 目的：让设置中的缓存位置、资源包缓存、分析缓存和 WebEngine 缓存使用同一根目录，并让“清理缓存”覆盖资源服务缓存。
- 变更：`cacheRoot` 作为唯一缓存根目录，下设 `analysis`、`resource-packages`、`web-profile`；资源服务通过 `FABLECUT_CACHE_ROOT` 派生路径；启动时兼容迁移旧版 `data/resource-cache`。
- 清理：设置页清理 `cacheRoot` 时先请求资源服务清理资源包及内存索引，再清理本地缓存根目录；资源服务不可用时明确提示本地已清理、重启后服务端完成清理。
- 验证：Node 语法检查及资源缓存/资源浏览器测试通过。

## 2026-09-21 生产资源目录部署

- 目的：完成已确认的资源目录 Function 与资源版本可见性迁移部署。
- 外部状态：`resource-catalog` 部署为 v23 并保持 ACTIVE；生产数据库成功应用 `202609210003_resource_version_visibility.sql`。
- 验证：再次执行数据库 dry-run 返回 up to date；未写入用户收藏或测试数据。
