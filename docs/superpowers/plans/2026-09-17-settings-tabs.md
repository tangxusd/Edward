# Edward 设置页四 Tab 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 Edward 桌面版提供可持久化、可迁移且可验证的路径、大模型、偏好与调试设置。

**Architecture:** FableCut 只负责四 Tab 的交互，通过现有 Qt WebChannel 调用 `WorkbenchRuntime`；目录配置及迁移由桌面端负责，避免网页直接操作本机文件系统。云端偏好、公开供应商预设和反馈分别走受认证的 Supabase Edge Function，API Key 永不上传至 Supabase。

**Tech Stack:** Qt 6/C++20、Qt WebChannel、QSettings、Qt Network、FableCut 原生 JavaScript、Supabase Postgres/Edge Functions/Deno。

## Global Constraints

- 保持 macOS/Windows 原生窗口标题栏，页面内不得模拟标题栏。
- 不引入 DaVinci Resolve、Fusion、Component IR、Premiere 或 OpenShot 代码与依赖。
- 目录清理只能删除 Edward 派生文件：缓存、代理、预渲染、可重新下载的组件/素材包；不得删除项目、导出成品或用户安装的插件运行目录。
- API Key 仅保存在设备本地，读取接口不得向 WebChannel 回传明文，也不得上传到 Supabase。
- Supabase 生产迁移、Function 部署和真实反馈提交属于外部状态变更，必须在本地测试通过后取得确认。

---

### Task 1: 定义并测试本地设置合同

**Files:**
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `tests/desktop/test_preference_bridge.cpp`

**Interfaces:**
- Produces `QVariantMap fablecutSettings() const`，包含八个可迁移目录、当前模型的非敏感字段与偏好状态。
- Produces `bool saveFablecutSettings(const QVariantMap&)`、`QString chooseFablecutDirectory(const QString&, const QString&)`、`bool clearDerivedFablecutDirectory(const QString&)`。

- [ ] **Step 1: 写失败测试**
  - 使用 `QTemporaryDir` 设置 `EDWARD_SETTINGS_PATH`。
  - 断言八个目录均为绝对路径；保存后重建 `WorkbenchRuntime` 仍能读取；`apiKey` 不存在于返回映射。
  - 断言 `clearDerivedFablecutDirectory("cache")` 删除测试派生文件，但拒绝 `project`、`export`、`pluginRuntime`。
- [ ] **Step 2: 运行失败测试**
  - 运行 `cmake --build build-0.7.0 --target test_preference_bridge -j 4 && ctest --test-dir build-0.7.0 -R desktop.preference_bridge --output-on-failure`。
- [ ] **Step 3: 实现目录配置**
  - 在 `QSettings` 的 `paths/` 分组存储：`projectRoot`、`cacheRoot`、`exportRoot`、`componentDownloadRoot`、`mediaDownloadRoot`、`pluginDownloadRoot`、`pluginRuntimeRoot`、`proxyRoot`、`prerenderRoot`。
  - 默认值均基于 `QStandardPaths::AppLocalDataLocation`；仅在用户选择目录后创建目录。
  - 清理只允许 `cacheRoot`、`componentDownloadRoot`、`mediaDownloadRoot`、`proxyRoot`、`prerenderRoot`，并保留根目录本身。
- [ ] **Step 4: 运行通过测试**
  - 重复步骤 2，期望 `desktop.preference_bridge` 通过。

### Task 2: 实现路径 Tab

**Files:**
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/style.css`
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/settings-client.js`
- Test: `third_party/FableCut/test/auth-ui.test.js`

**Interfaces:**
- Consumes `window.edwardSettings.load/save/chooseDirectory/clearDerivedDirectory`。
- Produces `settingsTab="paths"` 的可访问 Tab 与九项目录控件。

- [ ] **Step 1: 写失败测试**
  - 断言设置弹窗包含四个 Tab、九个路径输入、可清理目录的清理按钮；项目、导出和插件运行目录不存在清理按钮。
- [ ] **Step 2: 运行失败测试**
  - 运行 `cd third_party/FableCut && node --test test/auth-ui.test.js`。
- [ ] **Step 3: 实现 Tab 状态与路径表单**
  - 使用 `role="tablist"`、`role="tab"`、`role="tabpanel"`；键盘左右方向键切换 Tab。
  - 每个目录显示路径、选择按钮和适用时的“清理派生文件”按钮；清理前显示目录种类与不可恢复提示。
  - 保存后重新读取桌面设置以确认规范化后的路径。
- [ ] **Step 4: 运行通过测试**
  - 重复步骤 2，期望通过。

### Task 3: 实现模型供应商与本地网络操作

**Files:**
- Modify: `src/desktop/CMakeLists.txt`
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `third_party/FableCut/settings-client.js`
- Modify: `third_party/FableCut/app.js`
- Test: `tests/resources/test_model_chat_client.cpp`
- Test: `tests/desktop/test_preference_bridge.cpp`

**Interfaces:**
- Produces `testAiProvider(providerId)` 和 `refreshAiProviderModels(providerId)`，返回 `{ok,message,models}`。
- API Key 仅作为测试/拉取模型请求的本地输入，不能出现在返回值、日志或云端载荷中。

- [ ] **Step 1: 写失败测试**
  - 为 HTTPS 端点、Bearer 认证和 `/models` 推导添加断言；为无密钥、HTTP 端点和空模型断言拒绝。
- [ ] **Step 2: 运行失败测试**
  - 运行 `ctest --test-dir build-0.7.0 -R 'resources.model_chat_client|desktop.preference_bridge' --output-on-failure`。
- [ ] **Step 3: 实现供应商列表**
  - 本地供应商记录包含 `id`、`name`、`endpoint`、`model`、`modelList`，密钥只保留设备侧。
  - 使用已有 `ModelChatClient` 发起测试与模型列表请求；失败信息脱敏并映射为用户可读状态。
- [ ] **Step 4: 运行通过测试**
  - 重复步骤 2，期望通过。

### Task 4: 增加供应商预设与反馈云端合同

**Files:**
- Create: `supabase/migrations/202609170002_settings_cloud.sql`
- Create: `supabase/functions/model-provider-presets/index.ts`
- Create: `supabase/functions/desktop-feedback/index.ts`
- Create: `supabase/tests/settings-cloud-contract.test.ts`

**Interfaces:**
- `GET /functions/v1/model-provider-presets` 返回公开、无密钥的预设 `{id,name,endpoint,default_model}`。
- `POST /functions/v1/desktop-feedback` 接收认证用户的 `{kind,message,diagnostics}`；`diagnostics` 仅允许版本、平台与脱敏错误码。

- [ ] **Step 1: 写失败测试**
  - 断言 SQL 启用 RLS；预设无写入策略；反馈仅允许用户读取/插入自身记录；Function 拒绝无认证、空消息和包含路径/API Key/项目内容的诊断。
- [ ] **Step 2: 运行失败测试**
  - 运行 `deno test --allow-read supabase/tests/settings-cloud-contract.test.ts`。
- [ ] **Step 3: 实现 SQL 与 Function**
  - `model_provider_presets` 仅存公共元数据；`desktop_feedback` 存用户 ID、种类、消息、允许诊断与时间戳。
  - Function 使用当前 Bearer Token 获取用户，不使用客户端传入的用户 ID。
- [ ] **Step 4: 运行通过测试**
  - 重复步骤 2，期望通过。

### Task 5: 实现模型、偏好和调试 Tab

**Files:**
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/settings-client.js`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Test: `third_party/FableCut/test/auth-ui.test.js`

**Interfaces:**
- 偏好 Tab 使用已有 `PreferenceStore::exportFacts/importFacts` 与 `PreferenceSyncClient::upload/download`。
- 调试 Tab 使用 `submitFeedback(kind,message)`，并显示提交结果。

- [ ] **Step 1: 写失败测试**
  - 断言模型 Tab 有供应商选择、测试、拉取模型、添加供应商与预设加载；偏好 Tab 有导入、导出、上传、下载；调试 Tab 有错误上报和意见反馈。
- [ ] **Step 2: 运行失败测试**
  - 运行 `cd third_party/FableCut && node --test test/auth-ui.test.js test/preferences.test.js`。
- [ ] **Step 3: 实现 UI 与桌面桥接**
  - 未登录时禁用上传、下载与反馈并说明需要登录。
  - 本地偏好导入/导出使用原有二进制格式；云端按钮调用现有同步客户端。
  - 预设加载只覆盖名称、端点、模型，不覆盖本地 API Key。
- [ ] **Step 4: 运行通过测试**
  - 重复步骤 2，期望通过。

### Task 6: 回归与部署门

**Files:**
- Modify: `.edward/acceptance/current.json`
- Modify: `docs/operation-log/settings.md`

- [ ] **Step 1: 验证本地完整性**
  - 运行 `cmake --build build-0.7.0 --target edward_app -j 4`。
  - 运行 `ctest --test-dir build-0.7.0 --output-on-failure`。
  - 运行 `cd third_party/FableCut && node --test`。
  - 运行 `deno test --allow-read supabase/tests/settings-cloud-contract.test.ts`。
- [ ] **Step 2: 更新验收与日志**
  - 只在所有本地验证通过后将 `desktop-settings-tabs` 标记为 `passed`，记录目录清理范围、云端数据最小化规则与未部署状态。
- [ ] **Step 3: 生产部署确认门**
  - 展示将执行的 Supabase migration 与两个 Function 名称，等待用户批准后才部署。

## 覆盖检查

- 路径迁移、九类目录、派生文件清理：Task 1-2。
- 供应商列表、测试、模型拉取、添加和 Supabase 预设：Task 3-5。
- 偏好导入/导出及云端上传/下载：Task 5。
- 错误上报、意见反馈及数据最小化：Task 4-5。
- 本地构建、测试、验收和生产变更确认：Task 6。
