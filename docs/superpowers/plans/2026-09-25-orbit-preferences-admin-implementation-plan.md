# Orbit 颜色、字体、偏好与管理后台实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将已收敛的颜色、字体、偏好、兑换码、邀请码、时间线动画和 Cloudflare 管理后台方案实现为可测试、可回滚、可发布的 Orbit 功能。

**Architecture:** Supabase 负责目录、偏好事实、订阅权益、兑换码、邀请账本和管理员事实；Edge Functions/RPC 是所有跨表业务写入的唯一入口。桌面端在进入项目时建立一次颜色/字体会话快照，后续只读本地缓存；预览、渲染和导出消费同一份快照与同一求值器。Refine 只负责 `admin.edward.uno` 的 UI，不能绕过 RLS 或 Edge Function。

**Tech Stack:** C++20、Qt 6/QML、SQLite、Supabase PostgreSQL/RLS/Edge Functions、TypeScript/Deno、Refine、Cloudflare Pages/Workers、现有 CMake/Ninja 测试体系。

## Global Constraints

- 不引入 Component IR、组件转换层或第二套渲染真源；原生运行时直接负责预览和输出。
- 颜色/字体目录只在项目进入时读取本地缓存并最多执行一次版本检查；项目会话内的选择不访问 Supabase。
- 历史项目保存具体颜色值、色彩空间、透明度、字体内容哈希和资源版本，不受未来目录更新影响。
- 字体仅在用户明确选择后下载，校验授权、格式、大小和 SHA-256，并只安装到用户级目录，不申请管理员权限。
- 默认偏好只在首次安装且没有用户事实/方案时应用一次，不写入学习事实。
- 跨表权益、兑换、订阅周期和奖励只能由事务 RPC 完成；所有写入携带 `requestId` 并保持幂等。
- 预览、渲染、导出使用同一项目快照、同一字体解析结果和同一动画求值器；导出前再次校验合同。
- 管理后台不持有 service role；管理员权限由 `admin_roles`、RLS 和服务端会话校验共同决定。
- 不记录 access token、refresh token、兑换明文、字体签名 URL 或本地绝对路径。
- 所有回复、界面文案和新增操作记录使用中文；不新增 Emoji 图标。

---

## 任务 1：锁定数据合同、迁移和事务边界

**Files:**
- Create: `supabase/migrations/202609250001_orbit_catalogs.sql`
- Create: `supabase/migrations/202609250002_orbit_rewards_admin.sql`
- Create: `supabase/functions/_shared/orbit_contract.ts`
- Create: `supabase/functions/_shared/orbit_transaction.ts`
- Modify: `supabase/migrations/202609230001_preference_facts_append_only.sql`（只补充兼容索引，不改变已有事实语义）
- Test: `supabase/tests/orbit-contract.test.ts`
- Test: `supabase/tests/orbit-transaction.test.ts`

**Interfaces:**
- `getCatalogManifest(catalogType: 'color'|'font', knownRevision: string|null): Promise<{revision:string; fullHash:string; delta:CatalogDelta; expiresAt:string}>`
- `getFontAsset(fontId: string, resourceVersion: string, purpose: 'preview'|'install'): Promise<{signedUrl:string; contentHash:string; licenseId:string; expiresAt:string}>`
- `redeemCode(requestId: string, codeDigest: string, userId: string): Promise<RedeemResult>`
- `postReferralReward(requestId: string, inviteeUserId: string, orderId: string): Promise<RewardResult>`
- `adminPublishCatalog(requestId: string, catalogType: string, revision: string): Promise<PublishResult>`

- [ ] **Step 1: 写迁移失败测试。** 在 `supabase/tests/orbit-contract.test.ts` 固定字段名、唯一键、外键和枚举；断言重复 `revision`、重复 `content_hash`、无效 `parent_revision` 均被拒绝。
- [ ] **Step 2: 运行迁移测试确认失败。** 运行 `supabase test db --file supabase/tests/orbit-contract.test.ts`；预期新增表不存在导致失败。
- [ ] **Step 3: 创建目录与奖励表。** 建立 `color_catalog_revisions/items`、`font_catalog_revisions/items`、`font_catalog_assets`、`redeem_code_batches/codes/redemptions`、`referral_clicks`、`credit_ledger`、`admin_roles`、`admin_audit_events`；每个发布版本保存父版本、完整哈希、状态和变更摘要。
- [ ] **Step 4: 建立 RLS。** 用户只能读取已发布目录和自己的偏好/兑换/奖励视图；管理表只允许管理员 Edge Function 使用 service role 在服务端访问；客户端不得直接写管理表。
- [ ] **Step 5: 建立事务 RPC。** 实现 `redeem_code_once`、`apply_referral_reward`、`grant_subscription_entitlement`、`publish_catalog_revision`；以 `request_id` 唯一键先查幂等结果，再锁定兑换码/订单，最后原子写入权益、账本和审计。
- [ ] **Step 6: 建立合同测试。** 测试并发兑换、重复请求、权益创建失败、退款冻结、邀请重复绑定和目录回滚；确认任何失败都没有半成功行。
- [ ] **Step 7: 提交。** `git add supabase/migrations supabase/functions/_shared supabase/tests && git commit -m "feat: add Orbit catalog and reward contracts"`

## 任务 2：实现目录缓存、字体预览和用户级安装

**Files:**
- Create: `src/resources/include/edward/resources/catalog_cache.hpp`
- Create: `src/resources/src/catalog_cache.cpp`
- Create: `src/resources/include/edward/resources/font_installer.hpp`
- Create: `src/resources/src/font_installer.cpp`
- Modify: `src/resources/include/edward/resources/preference_sync_client.hpp`
- Modify: `src/resources/src/preference_sync_client.cpp`
- Modify: `src/resources/CMakeLists.txt`
- Test: `tests/resources/test_catalog_cache.cpp`
- Test: `tests/resources/test_font_installer.cpp`

**Interfaces:**
- `CatalogCache::openSession(const QString& accountId, const QString& projectId, CatalogType type) -> CatalogSession`
- `CatalogCache::checkRevisionOnce(CatalogSession&, const ManifestFetcher&) -> SyncResult`
- `CatalogCache::readColor(const CatalogSession&, const QString& semanticPath) -> std::optional<ColorValue>`
- `CatalogCache::readFont(const CatalogSession&, const QString& fontId) -> std::optional<FontRecord>`
- `FontInstaller::install(const FontAsset&, const QByteArray& expectedSha256) -> InstallResult`
- `FontInstaller::loadForPreview(const FontAsset&) -> RuntimeFontHandle`
- `FontInstaller::removeIfUnreferenced(const QString& contentHash) -> bool`

- [ ] **Step 1: 写缓存失败测试。** 覆盖首次无缓存、版本相同、增量父版本错误、哈希错误、网络失败、容量清理和被项目引用保护。
- [ ] **Step 2: 写字体安装失败测试。** 覆盖路径越界、非法扩展名、授权缺失、大小超限、SHA-256 不匹配、用户目录权限失败和重复安装。
- [ ] **Step 3: 实现 SQLite 缓存。** 缓存键固定为 `accountId/installationId/catalogType/catalogRevision`；保存 `lastSyncAttempt`、`lastSyncSuccess`、`fullHash` 和删除列表；增量校验失败时隔离包而不污染当前版本。
- [ ] **Step 4: 实现字体适配器。** macOS 使用 `QStandardPaths::AppDataLocation/fonts` 的用户目录，Windows/Linux 使用对应用户字体目录；禁止写入系统目录和公共合同中的绝对路径。
- [ ] **Step 5: 实现运行时加载。** 浏览器预览通过 `@font-face` 和 `document.fonts.load` 加载；QImage/原生渲染与导出字体集合使用同一 `FontRuntimeSnapshot`；加载失败返回明确错误，不静默换字体。
- [ ] **Step 6: 接入一次性会话快照。** 在项目打开流程创建 `CatalogSession`，只调用一次 manifest 检查；选择颜色/字体只读缓存并更新本地快照。
- [ ] **Step 7: 运行测试并提交。** `cmake --build --preset macos-debug --target test_catalog_cache test_font_installer -j2 && ctest --preset macos-debug -R 'resources\.(catalog_cache|font_installer)' --output-on-failure`；通过后提交 `feat: add Orbit catalog cache and font installer`。

## 任务 3：实现颜色、字体和偏好事实桥接

**Files:**
- Create: `src/desktop/include/edward/desktop/style_snapshot.hpp`
- Create: `src/desktop/src/style_snapshot.cpp`
- Modify: `src/desktop/include/edward/desktop/preference_store.hpp`
- Modify: `src/desktop/src/preference_store.cpp`
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/qml/Workbench.qml`
- Modify: `src/desktop/qml/EdwardPreview.qml`
- Test: `tests/desktop/test_style_snapshot.cpp`
- Test: `tests/desktop/test_preference_store.cpp`

**Interfaces:**
- `StyleSnapshot::color(const QString& semanticPath) -> ColorValue`
- `StyleSnapshot::font(const QString& fontId) -> FontRecord`
- `StyleSnapshot::freeze(const CatalogSession&, const TimelineSnapshot&) -> StyleSnapshot`
- `PreferenceStore::recordConfirmedPropertyChange(const QVariantMap&) -> bool`
- `PreferenceStore::recordAiConfirmedFact(const QVariantMap&) -> bool`
- `WorkbenchRuntime::styleSnapshot() -> QJsonObject`

- [ ] **Step 1: 写颜色和字体快照测试。** 断言文字、线条、边框、背景、阴影、SVG、组件颜色都能按稳定 `semanticPath` 取值；快照创建后目录更新不改变已冻结值。
- [ ] **Step 2: 写偏好事实测试。** 断言只有用户明确确认或 AI 已确认的最终值进入事实；默认偏好不进入事实；事实保存 `valueType`、`catalogRevision`、`resourceVersion`、`contentHash` 和 `source`。
- [ ] **Step 3: 实现 `StyleSnapshot`。** 规范化颜色为色彩空间、RGBA 和透明度；字体保存 fontId、resourceVersion、contentHash、安装状态和预览句柄。
- [ ] **Step 4: 接入 `PreferenceStore`。** 保留现有 append-only 事实和 A/B/C 编译流程，只让新模块写入统一事实表；本地队列离线保存，项目会话内不直接上传。
- [ ] **Step 5: 接入属性检查器与预览。** 将现有颜色字段和字体下拉绑定到 `StyleSnapshot`；下拉项用对应预览资源和字体自身样式显示；确认后刷新预览、渲染和导出使用的快照。
- [ ] **Step 6: 实现首次默认偏好。** 仅当安装标记不存在且无用户事实/方案时应用内置默认；写入安装完成标记后不重复应用。
- [ ] **Step 7: 运行测试并提交。** `cmake --build --preset macos-debug --target test_style_snapshot test_preference_store -j2 && ctest --preset macos-debug -R 'desktop\.(style_snapshot|preference_store)' --output-on-failure`；通过后提交 `feat: bridge Orbit style snapshots and preferences`。

## 任务 4：实现时间线播放头、标记和六类循环动画

**Files:**
- Modify: `src/core/include/edward/core/timeline.hpp`
- Modify: `src/core/src/timeline.cpp`
- Modify: `src/core/include/edward/core/timeline_commands.hpp`
- Modify: `src/core/src/timeline_commands.cpp`
- Create: `src/media/include/edward/media/loop_animation_evaluator.hpp`
- Create: `src/media/src/loop_animation_evaluator.cpp`
- Modify: `src/desktop/include/edward/desktop/timeline_controller.hpp`
- Modify: `src/desktop/src/timeline_controller.cpp`
- Modify: `src/desktop/qml/EdwardTimeline.qml`
- Modify: `src/desktop/qml/EdwardPreview.qml`
- Test: `tests/core/test_timeline_markers.cpp`
- Test: `tests/media/test_loop_animation_evaluator.cpp`
- Test: `tests/desktop/test_timeline_interaction.cpp`

**Interfaces:**
- `Timeline::addMarker(Frame frame, MarkerScope scope, ClipId clipId, MarkerColor color) -> MarkerId`
- `Timeline::removeMarker(MarkerId id) -> bool`
- `Timeline::snapFrame(Frame candidate, SnapContext context) -> Frame`
- `LoopAnimationEvaluator::evaluate(const LoopAnimation&, Frame frame, int fps) -> AnimationValue`
- `TimelineController::seekFrame(Frame frame)` and `TimelineController::dragPlayhead(Frame frame)`

- [ ] **Step 1: 写标记测试。** 覆盖整条时间线标记、选中素材按 M 产生的素材标记、右键删除/五种系统色、附近鼠标吸附和撤销恢复。
- [ ] **Step 2: 写播放头交互测试。** 覆盖颜色 `#6CFF37`、1px 分割线、空白单击定位、Pointer Capture 拖拽、缩放/滚动和左右键逐帧；片段、标记、轨道控件点击不得被空白定位抢占。
- [ ] **Step 3: 实现核心标记模型。** 把 marker 放入 `TimelineSnapshot`，区分 `Timeline` 与 `Clip` scope；删除、着色和新增均生成可回滚命令。
- [ ] **Step 4: 实现吸附与整数帧。** 在当前缩放下将鼠标位置转换为候选帧，按最近片段边界、标记和轨道边界排序，超过阈值才吸附；所有写入保持整数帧。
- [ ] **Step 5: 实现六类动画。** 支持持续右移、左移、上移、下移、放大、缩小，固定画布中心坐标、repeat/ping-pong、起止帧和缩放基准；预览和导出调用同一求值器。
- [ ] **Step 6: 实现导出前校验。** 对方向、坐标、循环边界、关键帧数量和字体/颜色快照重新校验；不一致时阻断导出并返回字段差异。
- [ ] **Step 7: 运行测试并提交。** `cmake --build --preset macos-debug --target test_timeline_markers test_loop_animation_evaluator test_timeline_interaction -j2 && ctest --preset macos-debug -R '(core\.timeline_markers|media\.loop_animation_evaluator|desktop\.timeline_interaction)' --output-on-failure`；通过后提交 `feat: add Orbit markers playhead and loop animation`。

## 任务 5：实现兑换码、邀请码和订阅抵扣

**Files:**
- Create: `supabase/functions/redeem-code/index.ts`
- Create: `supabase/functions/referral-summary/index.ts`
- Modify: `supabase/functions/auth-register/index.ts`
- Modify: `supabase/functions/alipay-create-order/index.ts`
- Modify: `supabase/functions/huifu-create-order/index.ts`
- Modify: `src/resources/include/edward/resources/supabase_auth_client.hpp`
- Modify: `src/resources/src/supabase_auth_client.cpp`
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/qml/Workbench.qml`
- Test: `supabase/tests/redeem-code.test.ts`
- Test: `supabase/tests/referral-reward.test.ts`

**Interfaces:**
- `POST /functions/v1/redeem-code {requestId, code}` → `{redemptionId, entitlement, expiresAt}`
- `POST /functions/v1/referral-summary` → `{code, inviteUrl, availableCredit, ledgerEntries}`
- `WorkbenchRuntime::redeemSubscriptionCode(const QString& code)`
- `WorkbenchRuntime::referralSummary()`

- [ ] **Step 1: 写兑换失败测试。** 覆盖过期、已兑换、并发、错误用户、批次权益注入和权益写入失败回滚。
- [ ] **Step 2: 写邀请码测试。** 覆盖注册绑定一次、成功订阅唯一奖励、退款冻结、后台金额规则、未订阅邀请开关和不可提现余额。
- [ ] **Step 3: 实现 Edge Functions。** 只接收用户会话和 requestId，调用任务 1 的 RPC；永不信任客户端传入价格、时长、奖励金额或用户 ID。
- [ ] **Step 4: 接入订阅订单抵扣。** 创建订单时从服务端读取可用 credit，保存价格规则快照和抵扣金额；支付回调仍以服务端订单为准。
- [ ] **Step 5: 接入桌面端面板。** 在订阅/续订面板右上角增加“兑换码”和“邀请码”小字按钮；显示兑换结果、邀请链接和不可提现抵扣余额，错误可重试且不重复提交。
- [ ] **Step 6: 运行测试并提交。** `deno test --allow-env --allow-net supabase/tests/redeem-code.test.ts supabase/tests/referral-reward.test.ts`；通过后提交 `feat: add Orbit redemption and referral flows`。

## 任务 6：实现管理后台和 Cloudflare 部署

**Files:**
- Create: `admin/`（Refine 应用，包含 `src/authProvider.ts`、`src/dataProvider.ts`、`src/resources/`、`src/pages/`）
- Create: `admin/wrangler.toml`
- Create: `admin/package.json`
- Create: `supabase/functions/admin-catalog/index.ts`
- Create: `supabase/functions/admin-redemption-batch/index.ts`
- Create: `supabase/functions/admin-preferences/index.ts`
- Create: `supabase/functions/admin-users/index.ts`
- Create: `supabase/functions/admin-stats/index.ts`
- Test: `admin/src/__tests__/authProvider.test.ts`
- Test: `supabase/tests/admin-security.test.ts`
- Modify: `docs/operations/cloudflare-admin.md`

**Interfaces:**
- Refine `authProvider.check()` → Supabase Auth 会话 + `admin_roles` 服务端验证
- Refine `dataProvider.getList/getOne/create/update/delete` → `https://admin.edward.uno/api/*`
- Admin API 请求头：`Authorization`、`X-Request-ID`、CSRF token；来源只允许 `https://admin.edward.uno`

- [ ] **Step 1: 写管理员安全测试。** 覆盖未登录、登录但无角色、角色撤销、过期会话、错误来源、缺失 CSRF、重复 requestId 和敏感日志扫描。
- [ ] **Step 2: 实现服务端认证。** 通过 Supabase Auth 获取用户 UUID，再查询 `admin_roles`；客户端只得到短期会话，不得到 service role。
- [ ] **Step 3: 实现统计资源。** 使用聚合视图或统计 Function 提供用户数、增长、订阅、反馈、问题日志和邀请码记录；禁止直接返回明细敏感字段。
- [ ] **Step 4: 实现管理资源。** 完成偏好管理、大模型模板、兑换码批次、用户状态、目录发布/撤回/回滚、邀请规则和审计日志；高风险操作必须二次确认并支持预览/取消。
- [ ] **Step 5: 部署 Cloudflare。** 配置 `admin.edward.uno`、CORS 白名单、HTTPS、缓存策略和错误页；根域名不承载后台或 auth recovery。
- [ ] **Step 6: 运行安全测试并提交。** `npm --prefix admin test` 与 `deno test --allow-env --allow-net supabase/tests/admin-security.test.ts`；通过后提交 `feat: add Orbit Cloudflare admin console`。

## 任务 7：端到端集成、验收和发布门

**Files:**
- Modify: `src/desktop/CMakeLists.txt`
- Modify: `tests/desktop/test_workbench_plugins.cpp`
- Create: `tests/e2e/test_orbit_style_render_export.cpp`
- Create: `tests/e2e/test_orbit_catalog_offline.cpp`
- Create: `docs/operations/orbit-preferences-admin.md`
- Modify: `.edward/acceptance/current.json`

- [ ] **Step 1: 写端到端失败测试。** 固定流程：打开项目 → 建立目录快照 → 选择颜色/字体 → 预览 → 渲染 → 导出 → 重开项目；断言三者使用相同颜色值、字体哈希、动画帧结果。
- [ ] **Step 2: 写离线和回滚测试。** 断网时继续编辑已有项目；目录增量校验失败不污染缓存；撤销时间线操作恢复 marker、clip 和属性。
- [ ] **Step 3: 接入验收清单。** `.edward/acceptance/current.json` 列出目录缓存、字体安装、偏好、兑换、邀请、播放头、动画、后台安全和导出一致性检查，每项都要有命令或可观察结果。
- [ ] **Step 4: 执行范围检查。** `cmake --build --preset macos-debug -j2`；`ctest --preset macos-debug --output-on-failure`；`deno test --allow-env --allow-net supabase/tests`；`npm --prefix admin test`；`git diff --check`。
- [ ] **Step 5: 执行 macOS Bundle 检查。** 运行 `cmake --build --preset macos-debug --target edward_app -j2`，验证 `Info.plist`、资源、字体适配器和签名输入；Finder/Dock 只验证图标可读，不修改 Dock 排列。
- [ ] **Step 6: 更新操作记录并提交。** 记录命令、版本、外部服务状态、失败重试和结果；提交 `chore: close Orbit preferences and admin acceptance`。

## 任务 8：归档和后续变更门

**Files:**
- Modify: `docs/superpowers/specs/2026-09-25-color-font-preferences-admin-design.md`
- Modify: `docs/superpowers/plans/2026-09-25-orbit-preferences-admin-implementation-plan.md`
- Create: `docs/operations/orbit-design-change-log.md`

- [ ] **Step 1: 对照验收标准。** 逐条核对设计文档第 12、13 节，记录测试命令和结果，不以“代码存在”代替用户可见结果。
- [ ] **Step 2: 固化变更流程。** 后续新增能力必须记录原因、影响字段、迁移策略、兼容范围和回滚方式；禁止直接修改已归档设计中的行为。
- [ ] **Step 3: 标记归档。** 只有任务 1–7 的完成门全部通过后，才把设计状态改为“已实现并归档”，并附最终 commit、迁移版本和部署版本。

## 自审结果

- **规格覆盖：** 任务 1 覆盖目录、RLS、RPC、兑换、邀请和后台事实；任务 2 覆盖缓存、字体文件、预览、安装和跨平台路径；任务 3 覆盖颜色/字体偏好与预览桥接；任务 4 覆盖播放头、M 标记、吸附、撤销和六类动画；任务 5 覆盖兑换/邀请 UI 与订阅抵扣；任务 6 覆盖 Refine、Cloudflare、管理员登录、统计和审计；任务 7 覆盖预览/渲染/导出一致性与离线兼容；任务 8 覆盖归档门和变更控制。
- **完整性扫描：** 每个任务均列出实际文件、接口、测试命令和提交点，没有空缺步骤或未定义的后续动作。
- **接口一致性：** `CatalogCache`、`FontInstaller`、`StyleSnapshot`、`LoopAnimationEvaluator`、兑换/邀请 Edge Function 和 Refine API 的名称在后续任务中保持一致；跨任务依赖通过接口块明确传递。
- **风险边界：** 不修改用户现有未提交改动；不把 service role、支付凭证或签名 URL 写入客户端；不使用根域名承载后台或 auth recovery；不改变 Dock 系统排列。
