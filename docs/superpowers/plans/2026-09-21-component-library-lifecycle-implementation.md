# Edward 组件库生命周期实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 交付可跨设备收藏、可按分类查询、可安全缓存并复现复杂动画组件的 Edward 三层组件库。

**Architecture:** Supabase 保存公共资源的稳定身份、不可变版本、分类与用户收藏；私有对象桶保存发布包和预览 MP4。FableCut 本地服务负责将经授权下载的完整资源版本原子缓存到数据目录；时间线实例锁定资源 ID、版本与哈希，原生运行时直接执行锁定版本的 React、HTML/CSS、GSAP 或 SVG 入口。

**Tech Stack:** PostgreSQL/Supabase Storage/Edge Functions、Deno、Node.js 标准库、浏览器 Fetch、FableCut 原生 Web Runtime、Node test、Deno test。

## Global Constraints

- 不使用 Component IR 或等价转换层；预览和导出直接执行相同的原生运行时参数。
- 所有位置使用 Edward 画布中心 `(0, 0)`，右/上为正；新增位置字段必须有中心、正负方向和预览/导出一致性测试。
- `导入` Tab 只显示当前项目用户导入素材，绝不请求公共资源目录。
- “我的收藏、最新、热门”是固定筛选；Supabase 业务分类是第二层；`local → cached → remote-preview` 是实际使用来源优先级。
- 内置标题、字幕不上传、不从 Supabase 更新、不写用户收藏；公共组件必须有 `target = web.runtime`、完整版本哈希与低分辨率 MP4。
- 资源卡片只显示名称与收藏数；内置资源显示“本地”。
- 不新增第三方运行时依赖；远端代码只能执行已校验、已登记、哈希固定的包。

---

## 文件结构

- `docs/contracts/component-package.schema.json`：公共组件发布包的机器可验证清单。
- `docs/contracts/edward-runtime-host.md`：原生运行时、动画帧、实例与升级合同。
- `supabase/migrations/202609210001_component_library_lifecycle.sql`：资源版本、收藏计数、归档状态和查询索引。
- `supabase/functions/resource-catalog/index.ts`：固定筛选、二级分类、`is_favorite`、签名预览 URL。
- `supabase/functions/resource-detail/index.ts`：取得锁定版本与包/预览签名 URL。
- `supabase/functions/resource-favorite/index.ts`：只变更用户收藏，不直接维护计数。
- `supabase/tests/component-library-contract.test.ts`：迁移、函数和版本合同测试。
- `tools/resource-publisher/publish.mjs`：验证清单、上传 `preview.mp4`、写入正确版本字段。
- `third_party/FableCut/resource-cache.js`：资源来源选择与缓存索引纯函数。
- `third_party/FableCut/zip-extract.js`：仅接受安全相对路径的 ZIP 解包器，供本机缓存运行入口使用。
- `third_party/FableCut/resource-timeline.js`：通用三秒资源插入轨道计算。
- `third_party/FableCut/server.js`：完整包/MP4 缓存、缓存预览文件服务与脱敏日志。
- `third_party/FableCut/app.js`：资源浏览器、统一动作按钮、插入实例、缓存调用和预览窗口。
- `third_party/FableCut/index.html`、`third_party/FableCut/style.css`：预览窗口与统一动作样式。
- `third_party/FableCut/test/resource-cache.test.js`、`resource-timeline.test.js`、`resource-browser.test.js`、`component-versioning.test.js`：行为测试。
- `.edward/acceptance/current.json`、`docs/operation-log/resource-library.md`：验收状态与操作记录。

### Task 1: 固化组件发布、动画和实例合同

**Files:**
- Modify: `docs/contracts/component-package.schema.json`
- Modify: `docs/contracts/edward-runtime-host.md`
- Modify: `tools/resource-publisher/publish.mjs`
- Modify: `tools/resource-publisher/test/publish.test.mjs`

**Interfaces:**
- Produces `validateManifest(manifest): string[]`，要求 `runtime`、`target`、`capabilities`、`timeline`、`assets`、`editableProperties` 和 `editableTracks`。
- Produces对象路径 `component_id/version/{manifest.json,package.zip,preview.mp4}`。

- [ ] **Step 1: 写失败的发布器测试**

    test("发布清单要求原生运行时、帧合同和完整 MP4 预览", () => {
      const valid = { ...base, runtime: "react", timeline: { authoringFps: 30, durationFrames: 90, frameRounding: "nearest" }, capabilities: { preview: true, export: true, editable: true, audio: false, transparent: true }, editableProperties: ["color"], editableTracks: ["x"] };
      assert.deepEqual(validateManifest(valid), []);
      assert.match(validateManifest({ ...valid, runtime: "canvas" }).join(" "), /runtime/);
      assert.match(validateManifest({ ...valid, preview: "preview.png" }).join(" "), /preview/);
    });

- [ ] **Step 2: 运行失败测试**

Run: `node --test tools/resource-publisher/test/publish.test.mjs`

Expected: FAIL，缺少新清单字段验证。

- [ ] **Step 3: 扩展 Schema 与发布器验证**

添加 `react|html-css|gsap|svg` 运行时枚举、`timeline.authoringFps/durationFrames/frameRounding`、能力声明以及每个资产的 `path/mimeType/bytes/sha256`。属性 Schema 必须设置 `additionalProperties: false`；实例只允许清单列出的静态属性和关键帧轨道。

- [ ] **Step 4: 修正 MP4 发布路径**

发布器必须将 preview.mp4 以 video/mp4 上传到 `component_id/version/preview.mp4`，并写入 `resource_versions.preview_video_path`，使 `preview_image_path` 保持 null。拒绝非 MP4 预览、缺失导出能力和未登记资产。

- [ ] **Step 5: 验证**

Run: `node --test tools/resource-publisher/test/publish.test.mjs && node --check tools/resource-publisher/publish.mjs`

Expected: PASS。

- [ ] **Step 6: 提交**

    git add docs/contracts/component-package.schema.json docs/contracts/edward-runtime-host.md tools/resource-publisher/publish.mjs tools/resource-publisher/test/publish.test.mjs
    git commit -m "feat: define versioned component package contract"

### Task 2: 建立 Supabase 收藏、分类、版本与归档合同

**Files:**
- Create: `supabase/migrations/202609210001_component_library_lifecycle.sql`
- Create: `supabase/tests/component-library-contract.test.ts`
- Modify: `supabase/functions/resource-catalog/index.ts`
- Modify: `supabase/functions/resource-detail/index.ts`
- Modify: `supabase/functions/resource-favorite/index.ts`

**Interfaces:**
- Directory parameters: `tabKey`, `filter` (`favorites|latest|popular`), `categoryId`, `limit`, `offset`。
- Directory item: `{id, component_id, target, name, favorite_count, is_favorite, preview_url, version, content_hash}`。
- Detail result: `{resource, version, packageUrl, previewUrl}`，两个 URL 均为短期签名 URL。

- [ ] **Step 1: 写失败的 Supabase 合同测试**

    Deno.test("资源合同以唯一用户收藏维护热门并支持筛选与分类组合", async () => {
      const migration = await Deno.readTextFile(new URL("../migrations/202609210001_component_library_lifecycle.sql", import.meta.url));
      const catalog = await Deno.readTextFile(new URL("../functions/resource-catalog/index.ts", import.meta.url));
      assertMatch(migration, /favorite_count = favorite_count \+ 1/i);
      assertMatch(catalog, /filter === "favorites"/);
      assertMatch(catalog, /is_favorite/);
      assertMatch(catalog, /preview_video_path/);
    });

- [ ] **Step 2: 运行失败测试**

Run: `deno test --allow-read supabase/tests/component-library-contract.test.ts`

Expected: FAIL，迁移和目录合同尚不存在。

- [ ] **Step 3: 编写迁移**

扩展现有 `resources.status` 约束为 `draft|published|archived|withdrawn`，不得新增并行生命周期字段；创建覆盖 `tab_key, category_id, favorite_count desc, published_at desc, id desc` 的热门索引。为 `resource_favorites` 的插入和删除创建触发器，原子递增/递减 `resources.favorite_count`。创建仅 service-role 能调用的计数重算函数。已发布资源与版本不得物理删除。

- [ ] **Step 4: 修改函数**

目录函数的 `favorites` 分支以当前用户 `resource_favorites` 内连接并按 `created_at desc` 排序；`popular` 按收藏数、发布时间、ID 稳定排序；所有固定筛选可叠加 `categoryId`。目录返回 `is_favorite` 与最新发布版本的签名 MP4 URL。详情函数返回包与 MP4 的签名 URL；收藏函数只 upsert/delete，返回触发器后的 `favorite` 与 `favoriteCount`。

- [ ] **Step 5: 验证**

Run: `deno test --allow-read supabase/tests/component-library-contract.test.ts`

Expected: PASS。

- [ ] **Step 6: 提交**

    git add supabase/migrations/202609210001_component_library_lifecycle.sql supabase/tests/component-library-contract.test.ts supabase/functions/resource-catalog/index.ts supabase/functions/resource-detail/index.ts supabase/functions/resource-favorite/index.ts
    git commit -m "feat: add resource favorites and version lifecycle"

### Task 3: 实现本地完整资源缓存与缓存预览服务

**Files:**
- Create: `third_party/FableCut/resource-cache.js`
- Create: `third_party/FableCut/zip-extract.js`
- Modify: `third_party/FableCut/server.js`
- Create: `third_party/FableCut/test/resource-cache.test.js`

**Interfaces:**
- `resourceCacheKey({componentId, version, contentHash}): string`
- `validateCacheIndex(index): {ok:boolean, reason?:string}`
- `POST /api/resources/cache` 输入 `{resourceId, componentId, version, contentHash}`，输出 `{state:"ready", previewUrl, manifest}`。
- `GET /api/resources/cache-preview?key=...` 仅服务已校验本机 MP4。
- `GET /api/resources/cache-entry?key=...&path=...` 仅服务已校验包中登记的原生运行文件。

- [ ] **Step 1: 写失败的缓存状态机测试**

    test("缓存索引要求完整包、清单、MP4 与同一内容哈希", () => {
      assert.deepEqual(validateCacheIndex({ componentId: "text.title.fade", version: "1.0.0", contentHash: "a".repeat(64), files: ["manifest.json", "package.zip", "preview.mp4"] }), { ok: true });
      assert.equal(validateCacheIndex({ componentId: "text.title.fade", version: "1.0.0", contentHash: "a".repeat(64), files: ["manifest.json", "preview.mp4"] }).ok, false);
    });

- [ ] **Step 2: 运行失败测试**

Run: `node --test third_party/FableCut/test/resource-cache.test.js`

Expected: FAIL，模块不存在。

- [ ] **Step 3: 实现缓存索引和安全解包纯函数**

缓存键固定为 `componentId@version#contentHash`。浏览器只保存元数据和来源，绝不把短期签名 URL 作为缓存身份。`zip-extract.js` 只接受 stored/deflate ZIP 条目；拒绝绝对路径、`..`、符号链接、重复路径和超过清单声明大小的文件。

- [ ] **Step 4: 实现本机原子缓存**

缓存目录固定为 `DATA_DIR/resource-cache/{encoded-component}/{version-hash}/`。服务通过详情接口取得受控签名 URL，下载 `manifest.json/package.zip/preview.mp4` 到临时目录；校验资源 ID、版本、哈希、资产清单和三项必需文件后，安全解包到 `runtime/` 并原子重命名。`cache-entry` 只能读取清单登记的 `previewEntry/renderEntry` 或资产路径。失败删除临时目录并记录脱敏原因；禁止浏览器传入任意 URL 或文件路径。

- [ ] **Step 5: 验证**

Run: `node --test third_party/FableCut/test/resource-cache.test.js && node --check third_party/FableCut/server.js && node --check third_party/FableCut/resource-cache.js && node --check third_party/FableCut/zip-extract.js`

Expected: PASS。

- [ ] **Step 6: 提交**

    git add third_party/FableCut/resource-cache.js third_party/FableCut/zip-extract.js third_party/FableCut/server.js third_party/FableCut/test/resource-cache.test.js
    git commit -m "feat: cache verified component packages locally"

### Task 4: 改造资源目录筛选、卡片、预览与收藏

**Files:**
- Modify: `third_party/FableCut/app.js:1768-2365`
- Modify: `third_party/FableCut/index.html:64-72`
- Modify: `third_party/FableCut/style.css:683-690`
- Create: `third_party/FableCut/test/resource-browser.test.js`

**Interfaces:**
- `resourceBrowserState.filter` 默认 `favorites`，不再用 `sort` 表达收藏。
- `createResourceActionButton(resource, action)`，其中 action 为 `add|favorite`。
- `toggleResourceFavorite(resource): Promise<void>`。
- `openResourcePreview(resource): void`。

- [ ] **Step 1: 写失败的浏览器合同测试**

    test("非导入 Tab 默认我的收藏且卡片只显示名称与统计", () => {
      const app = read("app.js");
      assert.match(app, /filter: "favorites"/);
      assert.match(app, /function createResourceActionButton\(resource, action\)/);
      assert.doesNotMatch(app, /resource-card-summary.*textContent = resource\.summary/);
    });

- [ ] **Step 2: 运行失败测试**

Run: `node --test third_party/FableCut/test/resource-browser.test.js`

Expected: FAIL，固定筛选和统一动作接口尚未存在。

- [ ] **Step 3: 重构浏览器状态与查询**

`loadResourceBrowser(tab)` 设置 `filter = "favorites"`、`categoryId = null`。固定筛选只修改 filter，二级分类只修改 categoryId。标注 Tab 删除绕过目录的专用分支，与媒体、文本、音频和卡片一样进入 `loadResourceBrowser("annotation")`；本地缓存键包含 tab/filter/categoryId；导入页保留 renderBin，不触发资源目录请求。

- [ ] **Step 4: 统一卡片和预览动作**

卡片仅渲染名称与 `resource.localFixed ? "本地" : "收藏 " + resource.favorite_count`。公共卡片按 is_favorite 显示收藏状态；本地卡片没有远端收藏请求。卡片和预览窗口都使用同一 createResourceActionButton；预览来源按已校验本地 MP4 优先、远端签名 MP4 回退。

- [ ] **Step 5: 验证**

Run: `node --test third_party/FableCut/test/resource-browser.test.js third_party/FableCut/test/resource-timeline.test.js`

Expected: PASS。

- [ ] **Step 6: 提交**

    git add third_party/FableCut/app.js third_party/FableCut/index.html third_party/FableCut/style.css third_party/FableCut/test/resource-browser.test.js
    git commit -m "feat: unify resource filters previews and actions"

### Task 5: 实现本地标题/字幕和通用三秒资源插入

**Files:**
- Modify: `third_party/FableCut/resource-timeline.js`
- Modify: `third_party/FableCut/app.js:2191-2241`
- Modify: `third_party/FableCut/test/resource-timeline.test.js`

**Interfaces:**
- `resolveResourceInsertionTrack(tracks, clips, start, duration, maxTracks)`
- `insertResourceClip(resource): Promise<void>`
- 固定资源包含 `id/target/runtime/version/contentHash/previewUrl/props`。

- [ ] **Step 1: 写失败的通用插入测试**

    test("任意资源在播放头插入三秒片段并选择最低无冲突视频轨", () => {
      const result = resolveResourceInsertionTrack([{ id: "V2", kind: "video" }, { id: "V1", kind: "video" }], [{ track: "V1", start: 5, duration: 3 }], 5, 3, 16);
      assert.deepEqual(result, { trackId: "V2", createdTrack: null });
    });

- [ ] **Step 2: 运行失败测试**

Run: `node --test third_party/FableCut/test/resource-timeline.test.js`

Expected: FAIL，通用函数尚不存在。

- [ ] **Step 3: 实现通用插入和实例锁定**

公共资源插入前调用本地缓存接口。片段写入 `resourceRef = {resourceId, componentId, version, contentHash, target}`；持续三秒，按最底层无冲突轨道、向上、创建新轨道的顺序落位。标题使用 `x:0,y:0`；字幕使用 `x:0,y:subtitleSafeAreaY(project.height)`，全部采用中心坐标。

- [ ] **Step 4: 记录脱敏资源操作**

插入、缓存、预览、收藏和升级日志包含资源 ID、版本、哈希、Target、来源、轨道、开始时间、结果；不得包含令牌、签名 URL、项目内容或绝对路径。

- [ ] **Step 5: 验证**

Run: `node --test third_party/FableCut/test/resource-timeline.test.js third_party/FableCut/test/resource-cache.test.js`

Expected: PASS。

- [ ] **Step 6: 提交**

    git add third_party/FableCut/resource-timeline.js third_party/FableCut/app.js third_party/FableCut/test/resource-timeline.test.js
    git commit -m "feat: insert version-locked resource clips"

### Task 6: 校验原生运行时、帧映射和显式升级

**Files:**
- Modify: `third_party/FableCut/component-runtime.js`
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/test/native-annotation-runtime.test.js`
- Create: `third_party/FableCut/test/component-versioning.test.js`

**Interfaces:**
- `validateComponentInstance(manifest, instance): {ok:boolean, reason?:string}`
- `componentFrameAt(projectFrame, clip, manifest): number`
- `upgradeComponentInstance(instance, fromManifest, toManifest): Promise<{props, keyframeOverrides}>`

- [ ] **Step 1: 写失败的版本和动画合同测试**

    test("实例拒绝未知属性和未授权关键帧，并锁定内容哈希", () => {
      const result = validateComponentInstance(manifest, { resourceRef, props: { color: "#fff", unknown: 1 }, keyframeOverrides: { x: [] } });
      assert.equal(result.ok, false);
      assert.match(result.reason, /未知属性/);
    });
    test("项目帧映射为作者整数帧", () => { assert.equal(componentFrameAt(45, clip, manifest), 45); });

- [ ] **Step 2: 运行失败测试**

Run: `node --test third_party/FableCut/test/component-versioning.test.js`

Expected: FAIL，验证和帧映射函数尚不存在。

- [ ] **Step 3: 实现验证与帧映射**

原生运行时仅读取实例锁定版本的入口和资产。静态属性、实例属性和关键帧轨道均按清单白名单及范围验证；缺失导出能力、版本资产或哈希不一致时阻止插入和导出并显示中文错误。帧映射使用整数 nearest，拉伸/循环/反放只在清单明确允许时生效。

- [ ] **Step 4: 实现显式升级**

仅当 `toManifest.upgrade.compatibleFrom` 匹配、migrationEntry 存在且 Target 相同时加载原生迁移入口。迁移前深拷贝实例为撤销快照；迁移结果必须通过新 Schema 与首帧、中间帧、末帧渲染检查，失败时保留旧实例。

- [ ] **Step 5: 验证**

Run: `node --test third_party/FableCut/test/component-versioning.test.js third_party/FableCut/test/native-annotation-runtime.test.js && node --check third_party/FableCut/component-runtime.js && node --check third_party/FableCut/app.js`

Expected: PASS。

- [ ] **Step 6: 提交**

    git add third_party/FableCut/component-runtime.js third_party/FableCut/app.js third_party/FableCut/test/native-annotation-runtime.test.js third_party/FableCut/test/component-versioning.test.js
    git commit -m "feat: validate and upgrade native component instances"

### Task 7: 完成验收、文档和本机应用验证

**Files:**
- Modify: `.edward/acceptance/current.json`
- Modify: `.edward/acceptance/resource-library-20260912.json`
- Create: `docs/operation-log/resource-library.md`
- Modify: `third_party/FableCut/CLAUDE.md`
- Modify: `third_party/FableCut/README.md`

**Interfaces:**
- Adds acceptance IDs: `resource-lifecycle-schema`, `resource-cache-integrity`, `resource-filters`, `resource-instance-version-lock`, `resource-preview-mp4`, `resource-ui-smoke`。

- [ ] **Step 1: 初始化验收门**

在 `.edward/acceptance/current.json` 添加六项 pending 检查及本任务中的精确验证命令；保留所有既有验收项。

- [ ] **Step 2: 运行完整自动验证**

Run: `node --test third_party/FableCut/test/resource-cache.test.js third_party/FableCut/test/resource-browser.test.js third_party/FableCut/test/resource-timeline.test.js third_party/FableCut/test/component-versioning.test.js third_party/FableCut/test/native-annotation-runtime.test.js && node --test tools/resource-publisher/test/publish.test.mjs && deno test --allow-read supabase/tests/component-library-contract.test.ts && node --check third_party/FableCut/server.js && node --check third_party/FableCut/app.js && node --check third_party/FableCut/mcp-server.js`

Expected: PASS。

- [ ] **Step 3: 本机 UI 冒烟验证**

启动当前工作树 FableCut 服务和 Edward.app，验证：导入页无公共资源请求；文本页默认我的收藏且标题/字幕置顶；公共卡片仅显示名称/收藏数；收藏状态刷新；已缓存组件播放本地 MP4；加号按轨道规则插入；缺失 Target 或缓存的资源不能插入。

- [ ] **Step 4: 写操作记录和更新文档**

记录实际时间、迁移/Function/缓存目录、测试命令、UI 结果和脱敏日志字段。更新 CLAUDE.md 与 README.md，说明资源包、缓存、固定筛选和版本锁定规则。

- [ ] **Step 5: 标记验收并提交**

只有自动测试和 UI 冒烟均通过，才将六项验收状态改为 passed。

    git add .edward/acceptance/current.json .edward/acceptance/resource-library-20260912.json docs/operation-log/resource-library.md third_party/FableCut/CLAUDE.md third_party/FableCut/README.md
    git commit -m "docs: verify component library lifecycle"
