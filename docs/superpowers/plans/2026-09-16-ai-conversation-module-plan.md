# Edward 0.6.0 AI 对话模块实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个只修改当前项目、支持官方 Web 运行时组件、资源库匹配和时间线编排的 AI 对话模块，并保证所有修改可验证、可原子撤销。

**Architecture:** AI 模型只产生受 schema 约束的意图和 ActionPlan；Edward 负责规则加载、项目快照校验、资源确定性匹配、时间线约束和事务执行。React、HTML/CSS、SVG、GSAP 组件通过同一个官方 Web Runtime Host 原生预览和渲染，不经过 Component IR、理解层或转换层。

**Tech Stack:** C++20、Qt、Qt WebEngine、现有 ModelChatClient、现有 Timeline、Node.js/Chromium 渲染运行时、JSON Schema、CTest/QtTest、FableCut Node test。

## Global Constraints

- Edward 0.6.0 新 AI 路线禁止 Component IR、理解层、转换层和等价中间表示。
- AI 只能修改当前项目；模型、skill、附件和规则文件都是不可信输入。
- 所有项目修改必须通过 ActionPlan 和原子事务；事务失败全部回滚，保留最近 5 个 AI 事务。
- `AGENTS.override.md` > `AGENTS.md` > `CLAUDE.md`；系统安全边界高于用户、规则和记忆。
- global/project/session 记忆必须隔离，属性必须使用命名空间。
- React、HTML/CSS、SVG、GSAP 使用原生运行时；预览和导出使用同一入口和 props。
- 导出只在用户明确请求时执行，导出前必须检查文件名冲突，不得覆盖已有文件。
- 不使用 OpenShot、DaVinci Resolve、Premiere 及其 API、插件或脚本作为 Edward 运行依赖。

---

### Task 1: 定义官方 Web Runtime Host 合同

**Files:**
- Create: `docs/contracts/edward-runtime-host.md`
- Create: `docs/contracts/edward-runtime-host.schema.json`
- Create: `src/runtime/include/edward/runtime/runtime_manifest.hpp`
- Create: `src/runtime/src/runtime_manifest.cpp`
- Create: `tests/runtime/test_runtime_manifest.cpp`
- Create: `src/runtime/CMakeLists.txt`, `tests/runtime/CMakeLists.txt`
- Modify: `CMakeLists.txt`（按现有子目录模式注册 `edward_runtime` 与 runtime tests）

**Interfaces:**
- `RuntimeManifest::parse(const QJsonObject&, QString*) -> std::optional<RuntimeManifest>`
- `RuntimeManifest::validate(QString*) const -> bool`
- `RuntimeManifest` 字段：`protocol`, `runtime`, `width`, `height`, `fps`, `durationInFrames`, `previewEntry`, `renderEntry`, `propsSchema`, `editableProperties`。

- [ ] **Step 1: Write the failing test**

```cpp
void test_acceptsSupportedRuntimeManifest();
void test_rejectsMissingEntryAndNonPositiveMetadata();
void test_rejectsComponentIrAndArbitraryPathFields();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset macos-debug -R test_runtime_manifest --output-on-failure`
Expected: FAIL because the runtime manifest type and target do not exist.

- [ ] **Step 3: Write minimal implementation**

实现严格 schema 校验、相对入口路径校验、运行时白名单校验（`react`、`html-css`、`svg`、`gsap`），拒绝绝对路径、路径穿越、任意命令字段和 Component IR 字段。

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset macos-debug -R test_runtime_manifest --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add docs/contracts/edward-runtime-host.md docs/contracts/edward-runtime-host.schema.json src/runtime tests/runtime CMakeLists.txt
git commit -m "feat: define Edward web runtime host contract"
```

### Task 2: 实现原生宿主预览与渲染调度

**Files:**
- Create: `src/runtime/include/edward/runtime/web_runtime_host.hpp`
- Create: `src/runtime/src/web_runtime_host.cpp`
- Create: `src/runtime/include/edward/runtime/render_request.hpp`
- Create: `src/runtime/src/render_request.cpp`
- Create: `tests/runtime/test_web_runtime_host.cpp`
- Modify: `src/desktop/qml/EdwardPreview.qml`, `src/desktop/qml/Workbench.qml`
- Create: `src/runtime/CMakeLists.txt`, `tests/runtime/CMakeLists.txt`
- Modify: `CMakeLists.txt`, `src/desktop/CMakeLists.txt`

**Interfaces:**
- `WebRuntimeHost::mount(const RuntimeManifest&, const QString& packageRoot, const QJsonObject& props) -> HostResult`
- `WebRuntimeHost::setFrame(qint64 frame) -> HostResult`
- `WebRuntimeHost::setProps(const QJsonObject&) -> HostResult`
- `WebRuntimeHost::renderFrame(qint64 frame, const QString& outputPath) -> HostResult`
- `WebRuntimeHost::unmount() -> void`

- [ ] **Step 1: Write the failing test**

```cpp
void test_mountsEachSupportedRuntimeThroughNativeEntry();
void test_previewAndRenderUseIdenticalPropsAndFrame();
void test_rejectsHostPackageOutsideApprovedRoot();
void test_reportsRuntimeFailureWithoutFallbackOrFieldDeletion();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset macos-debug -R test_web_runtime_host --output-on-failure`
Expected: FAIL because the host does not exist.

- [ ] **Step 3: Write minimal implementation**

使用 Qt WebEngine 加载 `previewEntry`，通过固定消息协议发送 `mount`、`setProps`、`setFrame`、`renderFrame`。渲染任务使用同一组件包、同一 props 和同一帧号；入口失败只返回结构化错误，禁止静默替换为旧 IR 或素材层。

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset macos-debug -R test_web_runtime_host --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/runtime src/desktop/qml/EdwardPreview.qml src/desktop/qml/Workbench.qml CMakeLists.txt src/desktop/CMakeLists.txt tests/runtime
git commit -m "feat: add native web runtime preview and render host"
```

### Task 3: ActionPlan、确定性校验和原子事务

**Files:**
- Create: `src/ai/include/edward/ai/action_plan.hpp`
- Create: `src/ai/src/action_plan.cpp`
- Create: `src/ai/include/edward/ai/ai_transaction.hpp`
- Create: `src/ai/src/ai_transaction.cpp`
- Create: `tests/ai/test_action_plan.cpp`
- Create: `tests/ai/test_ai_transaction.cpp`
- Create: `src/ai/CMakeLists.txt`, `tests/ai/CMakeLists.txt`
- Modify: `CMakeLists.txt`, `src/desktop/include/edward/desktop/workbench_runtime.hpp`, `src/desktop/src/workbench_runtime.cpp`

**Interfaces:**
- `ActionPlan::parse(const QJsonObject&, QString*) -> std::optional<ActionPlan>`
- `ActionPlan::validate(const ProjectSnapshot&, QString*) const -> bool`
- `AiTransaction::apply(const ActionPlan&, ProjectState&) -> TransactionResult`
- `AiTransaction::undoLast(ProjectState&) -> TransactionResult`
- `AiTransaction::undoDepth() const -> int`（最大 5）

- [ ] **Step 1: Write the failing test**

```cpp
void test_rejectsUnknownOperationAndComponentIrFields();
void test_rejectsStaleBaseProjectRevision();
void test_rollsBackAllOperationsWhenOneOperationFails();
void test_keepsOnlyFiveUndoableAiTransactions();
void test_exportRequiresExplicitRequestAndCollisionCheck();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset macos-debug -R 'test_(action_plan|ai_transaction)' --output-on-failure`
Expected: FAIL because the new plan and transaction APIs do not exist.

- [ ] **Step 3: Write minimal implementation**

加入 `baseProjectRevision`、目标存在性/类型前提、资源 ID 白名单、导出文件名检查和完整回滚。AI 事务只保存项目快照差异；撤销不修改记忆和规则文件。

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset macos-debug -R 'test_(action_plan|ai_transaction)' --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/ai CMakeLists.txt src/desktop/include/edward/desktop/workbench_runtime.hpp src/desktop/src/workbench_runtime.cpp tests/ai
git commit -m "feat: add validated AI action plans and transactions"
```

### Task 4: 规则文件发现与安全上下文

**Files:**
- Create: `src/ai/include/edward/ai/rule_file_loader.hpp`
- Create: `src/ai/src/rule_file_loader.cpp`
- Create: `tests/ai/test_rule_file_loader.cpp`
- Modify: `src/ai/CMakeLists.txt`, `tests/ai/CMakeLists.txt`
- Modify: `src/desktop/src/workbench_runtime.cpp`, `src/desktop/include/edward/desktop/workbench_runtime.hpp`

**Interfaces:**
- `RuleFileLoader::load(const std::filesystem::path& projectRoot, const std::filesystem::path& target) -> RuleSnapshot`
- `RuleSnapshot::effectiveRules() const -> QString`
- `RuleSnapshot::entries() const -> QVector<RuleEntry>`（含路径、哈希、层级、生效范围）

- [ ] **Step 1: Write the failing test**

```cpp
void test_mergesNestedRulesWithNearestDirectoryPrecedence();
void test_prefersOverrideThenAgentsThenClaudeInOneDirectory();
void test_excludesAttachmentsCommentsAndSkillOutputFromRules();
void test_recordsHashAndRejectsOversizedRuleFile();
void test_rulesCannotGrantShellNetworkOrSandboxEscape();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset macos-debug -R test_rule_file_loader --output-on-failure`
Expected: FAIL because the loader does not exist.

- [ ] **Step 3: Write minimal implementation**

按系统安全边界 > 用户当前指令 > 规则文件 > 项目事实 > global 记忆 > session 推断的顺序生成上下文；规则仅读取、不写回、不授予额外权限。

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset macos-debug -R test_rule_file_loader --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/ai src/desktop/src/workbench_runtime.cpp src/desktop/include/edward/desktop/workbench_runtime.hpp tests/ai
git commit -m "feat: load scoped project agent rules safely"
```

### Task 5: 三层记忆与按需召回

**Files:**
- Create: `src/ai/include/edward/ai/memory_store.hpp`
- Create: `src/ai/src/memory_store.cpp`
- Create: `src/ai/include/edward/ai/memory_retriever.hpp`
- Create: `src/ai/src/memory_retriever.cpp`
- Create: `tests/ai/test_memory_store.cpp`
- Create: `tests/ai/test_memory_retriever.cpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`

**Interfaces:**
- `MemoryStore::remember(const MemoryEntry&, bool userConfirmed) -> MemoryResult`
- `MemoryStore::forget(const MemoryKey&) -> MemoryResult`
- `MemoryStore::query(const MemoryQuery&) -> QVector<MemoryEntry>`
- `MemoryStore::flush(FlushReason) -> MemoryResult`
- `MemoryRetriever::recall(const QString& target, const QString& namespace, MemoryScope) -> QVector<MemoryEntry>`

- [ ] **Step 1: Write the failing test**

```cpp
void test_separatesGlobalProjectAndSessionScopes();
void test_doesNotPersistUnconfirmedBehaviorInference();
void test_preventsTextColorAndBorderColorNamespacePollution();
void test_projectValueBeatsGlobalValueWithoutLastWriteWins();
void test_flushesOnlyOnIdleOpenOrExport();
void test_tombstoneHidesDeletedOlderValue();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset macos-debug -R 'test_memory_(store|retriever)' --output-on-failure`
Expected: FAIL because the new memory APIs do not exist.

- [ ] **Step 3: Write minimal implementation**

默认使用设备本地存储；只保存用户确认的偏好、纠正、工作流或项目事实。闲置阈值固定为 300 秒；打开项目、导出项目和闲置批处理触发 flush。完整聊天原文、代码、文案和素材内容不得写入 global 记忆。

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset macos-debug -R 'test_memory_(store|retriever)' --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/ai src/desktop/src/workbench_runtime.cpp tests/ai
git commit -m "feat: add scoped AI memory and namespace-safe recall"
```

### Task 6: 文案 target 与资源库确定性匹配

**Files:**
- Create: `src/ai/include/edward/ai/resource_matcher.hpp`
- Create: `src/ai/src/resource_matcher.cpp`
- Create: `tests/ai/test_resource_matcher.cpp`
- Create: `src/runtime/include/edward/runtime/verified_resource_catalog.hpp`
- Create: `src/runtime/src/verified_resource_catalog.cpp`
- Modify: `src/runtime/CMakeLists.txt`
- Create: `docs/contracts/resource-match.schema.json`

**Interfaces:**
- `ResourceMatcher::rank(const TargetIntent&, const VerifiedResourceCatalog&, const TimelineSnapshot&) -> QVector<ResourceCandidate>`
- `ResourceMatcher::choose(const QVector<ResourceCandidate>&) -> MatchDecision`

- [ ] **Step 1: Write the failing test**

```cpp
void test_ranksTargetSceneActionRuntimeAndTimelineConstraintsDeterministically();
void test_autoChoosesOnlyWhenTopScoreIsAtLeast070AndMarginIsAtLeast015();
void test_requestsClarificationForCloseCandidatesOrMissingTarget();
void test_acceptsResourceIdOnlyAndRejectsPathsAndUnverifiedUrls();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset macos-debug -R test_resource_matcher --output-on-failure`
Expected: FAIL because matcher and contract do not exist.

- [ ] **Step 3: Write minimal implementation**

实现固定版本的排序权重、权限过滤、运行时支持过滤、时间线冲突过滤，并记录候选、分数、排序依据、最终选择和事务 ID。模型只提供 target 意图，不提供资源路径。

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset macos-debug -R test_resource_matcher --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/ai src/runtime/include/edward/runtime/verified_resource_catalog.hpp src/runtime/src/verified_resource_catalog.cpp src/runtime/CMakeLists.txt docs/contracts/resource-match.schema.json tests/ai
git commit -m "feat: match resource components by deterministic target ranking"
```

### Task 7: AI 模型路由与 Workbench 集成

**Files:**
- Create: `src/ai/include/edward/ai/ai_orchestrator.hpp`
- Create: `src/ai/src/ai_orchestrator.cpp`
- Create: `tests/ai/test_ai_orchestrator.cpp`
- Modify: `src/resources/include/edward/resources/model_chat_client.hpp`, `src/resources/src/model_chat_client.cpp`
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`, `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/qml/Workbench.qml`

**Interfaces:**
- `AiOrchestrator::handle(const UserRequest&, const ProjectContext&) -> AiResult`
- `AiResult` 只允许 `conversation`、`clarification`、`action_plan`、`unsupported`。

- [ ] **Step 1: Write the failing test**

```cpp
void test_routesConversationClarificationActionAndUnsupportedDeterministically();
void test_explanationPlusEditChoosesSmallestUnambiguousAction();
void test_neverCallsLegacyComponentIrOrFusionConversionPath();
void test_rejectsModelOutputWithUnknownOperationsOrDirectFileWrites();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset macos-debug -R test_ai_orchestrator --output-on-failure`
Expected: FAIL because orchestration API does not exist.

- [ ] **Step 3: Write minimal implementation**

替换现有 prompt 中要求 Component IR/Fusion 转写的分支；模型只返回 ActionPlan 或普通文本，Edward 先校验再执行。请求上下文包含规则快照、相关记忆索引、项目修订号、选中对象和资源目录摘要，不发送 API key 到工程或日志。

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset macos-debug -R test_ai_orchestrator --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/ai src/resources/include/edward/resources/model_chat_client.hpp src/resources/src/model_chat_client.cpp src/desktop/include/edward/desktop/workbench_runtime.hpp src/desktop/src/workbench_runtime.cpp src/desktop/qml/Workbench.qml tests/ai
git commit -m "feat: route AI conversation through Edward orchestration"
```

### Task 8: AI 助理界面与端到端验收

**Files:**
- Modify: `third_party/FableCut/index.html`, `third_party/FableCut/style.css`, `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/test/visual-layout.test.js`
- Create: `tests/e2e/test_ai_conversation_host.cpp`
- Modify: `tests/e2e/CMakeLists.txt`
- Modify: `docs/operation-log/` 现有 AI 操作记录（追加结果，不新建同类记录）

- [ ] **Step 1: Write the failing test**

```js
test("AI inspector uses the three selection/conversation layout states", () => {
  // assert title, no-selection, ai-idle and conversation-active selectors
});
```

```cpp
void test_skillComponentCanPreviewEditRenderAndExportThroughOneHost();
void test_resourceSelectionTimelineInsertionAndUndoAreAtomic();
void test_exportCollisionIsRejectedBeforeRendering();
```

- [ ] **Step 2: Run test to verify it fails**

Run: `npm test` in `third_party/FableCut` and `ctest --preset macos-debug -R test_ai_conversation_host --output-on-failure`.
Expected: the new end-to-end assertions fail before integration is complete.

- [ ] **Step 3: Write minimal implementation**

完成三态 UI：未选中时 AI 占满检查器；选中且无用户消息时压缩；已有用户消息时与属性检查器分栏。端到端链路验证 React、HTML/CSS、SVG、GSAP 组件的挂载、属性更新、逐帧预览、渲染、导出、冲突拒绝和撤销。

- [ ] **Step 4: Run test to verify it passes**

Run: `npm test` in `third_party/FableCut`; `cmake --build build -j2`; `ctest --preset macos-debug --output-on-failure`。
Expected: FableCut tests、AI 专项测试和现有 CTest 全部通过；失败组件只显示明确错误，不走旧 IR/Fusion 回退。

- [ ] **Step 5: Commit**

```bash
git add third_party/FableCut src/desktop/qml tests/e2e docs/operation-log
git commit -m "test: verify AI conversation host end to end"
```

## 完成门

只有以下条件全部满足才可报告完成：

- 新 AI 路线没有调用 Component IR、理解层、转换层或旧 Fusion 转写；
- 四种官方 Web 运行时均通过预览、属性更新、逐帧渲染和导出验收；
- ActionPlan 版本、项目修订号、目标前提和原子回滚测试通过；
- 最近 5 个 AI 事务可撤销，失败事务不留下部分修改；
- global/project/session 记忆隔离、命名空间隔离和 300 秒 flush 测试通过；
- AGENTS 类文件层级、优先级、哈希和安全边界测试通过；
- 资源库匹配阈值和候选澄清行为测试通过；
- 导出文件名冲突在渲染前被拒绝；
- `npm test`、`cmake --build build -j2` 和相关 `ctest` 全部通过。
