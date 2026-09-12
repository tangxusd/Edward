# Supabase 资源库与组件属性契约 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 为 Edward/FableCut 建立安全、缓存友好的 Supabase 三级资源库，并让组件属性契约驱动属性检查器。

**Architecture:** Supabase 私有 Storage + RLS 数据库保存分类、资源、版本、收藏和权限；Edge Function 作为唯一资源访问入口。本地 FableCut 服务代理资源 API，并在浏览器 IndexedDB/Cache Storage 缓存元数据与预览；组件包 manifest 提供确定性的属性契约。

**Tech Stack:** Supabase Postgres、RLS、Storage、Edge Functions、Node.js 标准库、FableCut 原生 HTML/CSS/JavaScript、Qt WebEngine。

## Global Constraints

- 一级分类固定为现有非导入 Tab：media、text、audio、cards、chart、background、annotation、number。
- Supabase 原始资源与预览均使用私有 Bucket，禁止公开 URL。
- service role key 只能存在 Codex 录入 CLI 或 CI Secret，不能进入 Qt、网页、Git。
- 客户端不直接修改收藏数、浏览数或订阅状态。
- 订阅到期禁止新授权；本地缓存必须带版本、哈希和最近授权时间。
- 所有输入解析、属性校验和权限判断使用确定性程序规则，不调用模型。

---

### Task 1: Supabase 数据库迁移与 RLS

**Files:**
- Create: `supabase/migrations/20260912_resource_library.sql`
- Create: `supabase/tests/resource_library_rls.sql`
- Modify: `docs/superpowers/specs/2026-09-12-supabase-resource-library-design.md`

**Interfaces:**
- Produces tables `resource_categories`, `resources`, `resource_versions`, `resource_favorites`, `resource_views`, `entitlements` and indexes used by Tasks 2–4.

- [ ] **Step 1: Write failing SQL assertions**

```sql
select has_table('public', 'resources');
select policy_allows('resources', 'anon', 'select', false);
select policy_allows('resource_favorites', 'authenticated', 'insert', true);
```

- [ ] **Step 2: Run assertions and verify they fail**

Run: `supabase db reset && psql "$SUPABASE_DB_URL" -f supabase/tests/resource_library_rls.sql`

Expected: missing-table/policy failures.

- [ ] **Step 3: Create schema, constraints, indexes and RLS**

Implement the exact tables and fields in the approved spec. Add `check (tab_key in (...))`, unique `(user_id, resource_id)`, parent/child foreign keys, private default policies, and authenticated-user policies that expose only published resources and matching entitlements.

- [ ] **Step 4: Run assertions and inspect query plans**

Run the same SQL test plus `EXPLAIN` for Tab/category/favorite/latest queries. Expected: all assertions pass and category/time/favorite indexes are used.

- [ ] **Step 5: Commit**

```bash
git add supabase/migrations supabase/tests
git commit -m "建立 Supabase 资源库数据模型与 RLS"
```

### Task 2: 安全资源 API 与签名访问

**Files:**
- Create: `supabase/functions/resource-catalog/index.ts`
- Create: `supabase/functions/resource-detail/index.ts`
- Create: `supabase/functions/resource-favorite/index.ts`
- Create: `supabase/functions/resource-entitlement/index.ts`
- Create: `supabase/tests/resource-functions.test.ts`

**Interfaces:**
- `GET resource-catalog({tabKey, categoryId, sort, cursor, limit})` → paginated metadata and signed preview URLs.
- `GET resource-detail({resourceId, version})` → authorized detail plus short-lived source URL.
- `POST resource-favorite({resourceId, favorite})` → idempotent result and server-computed count.

- [ ] **Step 1: Write tests for anonymous denial, entitlement denial, pagination and idempotent favorite**
- [ ] **Step 2: Run `deno test supabase/tests/resource-functions.test.ts` and verify failures**
- [ ] **Step 3: Implement JWT validation, allow-listed sort keys, row limits, rate limits, entitlement checks and 60-second signed URLs**
- [ ] **Step 4: Re-run tests and verify no service-role data leaks**
- [ ] **Step 5: Commit**

```bash
git add supabase/functions supabase/tests
git commit -m "增加受限资源目录与签名访问函数"
```

### Task 3: Codex 专用资源录入管道

**Files:**
- Create: `tools/resource-publisher/package.json`
- Create: `tools/resource-publisher/publish.mjs`
- Create: `tools/resource-publisher/manifest-schema.json`
- Create: `tools/resource-publisher/test/publish.test.mjs`

**Interfaces:**
- `node tools/resource-publisher/publish.mjs --manifest <path> --package <path> --preview <path>` validates, hashes and publishes one version using `SUPABASE_URL` and `SUPABASE_SERVICE_ROLE_KEY` environment variables.

- [ ] **Step 1: Write failing tests for invalid manifest, duplicate hash and successful version publish**
- [ ] **Step 2: Run `node --test tools/resource-publisher/test/publish.test.mjs` and verify failures**
- [ ] **Step 3: Implement manifest validation, SHA-256 hashing, private Storage upload, metadata insert and rollback on partial failure**
- [ ] **Step 4: Run tests with a fixture package and verify no key is logged**
- [ ] **Step 5: Commit**

### Task 4: FableCut catalog proxy and local cache

**Files:**
- Modify: `third_party/FableCut/server.js`
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/style.css`
- Create: `third_party/FableCut/test/resource-catalog.test.js`

**Interfaces:**
- Local endpoints `/api/resources/catalog`, `/api/resources/detail`, `/api/resources/favorite` proxy Edge Functions without exposing service credentials.
- Browser cache keys use `component_id/version/content_hash`; metadata uses stale-while-revalidate.

- [ ] **Step 1: Add failing endpoint and cache tests**
- [ ] **Step 2: Run `node --test third_party/FableCut/test/resource-catalog.test.js` and verify failures**
- [ ] **Step 3: Implement paginated proxy, IndexedDB metadata cache, visible-card preview batching and offline entitlement grace handling**
- [ ] **Step 4: Run tests and verify repeated Tab visits do not issue duplicate catalog requests**
- [ ] **Step 5: Commit**

### Task 5: 非导入 Tab 两栏三级浏览器

**Files:**
- Modify: `src/desktop/qml/Workbench.qml`
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/style.css`
- Modify: `third_party/FableCut/app.js`
- Create: `tests/desktop/test_resource_catalog_ui.cpp`

**Interfaces:**
- `tab_key` drives一级 Tab；二级分类列表位于左栏；三级分类和资源卡片位于右栏；顶部固定收藏/热门/最新。

- [ ] **Step 1: Add UI tests for Tab mapping, fixed sort entries and category selection**
- [ ] **Step 2: Run targeted UI tests and verify failures**
- [ ] **Step 3: Implement the two-column layout, loading/error/empty states, pagination and favorite actions**
- [ ] **Step 4: Run Qt build and browser smoke test**
- [ ] **Step 5: Commit**

### Task 6: 组件属性契约与属性检查器

**Files:**
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/style.css`
- Create: `third_party/FableCut/test/component-manifest.test.js`
- Create: `docs/contracts/component-inspector-manifest.schema.json`

**Interfaces:**
- `validateComponentManifest(manifest)` returns deterministic diagnostics.
- `buildInspectorFromManifest(manifest, instance)` creates controls from `inspector.groups/properties`.
- Instance writes use `{component_id, version, overrides, keyframes}` and reject undeclared fields.

- [ ] **Step 1: Write failing tests for type/unit/range/enum/keyframe validation and undeclared-field rejection**
- [ ] **Step 2: Run `node --test third_party/FableCut/test/component-manifest.test.js` and verify failures**
- [ ] **Step 3: Implement schema validation, normalized values, original/parsed/computed/used value retention and inspector rendering**
- [ ] **Step 4: Run tests and verify a sample component round-trips through browser and Fusion mapping metadata**
- [ ] **Step 5: Commit**

### Task 7: 全链路安全与性能验收

**Files:**
- Create: `supabase/tests/security-performance.test.ts`
- Modify: `docs/operation-log/fablecut-navigation-tabs.md`
- Modify: `.edward/acceptance/current.json`

- [ ] **Step 1: Test anonymous direct-table access, forged resource IDs, expired entitlement, replayed signed URLs and rate limits**
- [ ] **Step 2: Test cache hit ratio and request count for Tab/category/detail workflows**
- [ ] **Step 3: Run `node --check third_party/FableCut/app.js`, `node --test third_party/FableCut/test`, `cmake --build build -j4`, and Supabase tests**
- [ ] **Step 4: Record receipts, mark acceptance checks passed, and document any external Supabase setup values required**
- [ ] **Step 5: Commit**

