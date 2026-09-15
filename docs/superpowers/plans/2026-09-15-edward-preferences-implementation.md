# Edward 0.6.0 偏好系统 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Edward 主进程中实现设备级 SQLite 偏好存储、确定性属性隔离、仅作用于新增组件的三套默认方案，以及可选的 Supabase 事实同步。

**Architecture:** Qt `PreferenceStore` 是唯一事实写入方，数据库位于 Edward 应用数据目录；FableCut 通过 Qt WebChannel 窄接口读取新增默认值并提交用户确认的事实。偏好不写入 `project.json`，不参与 React、HTML/CSS、SVG、GSAP 的渲染或转换；三套方案是由事实记录派生的本地缓存。

**Tech Stack:** C++20、Qt 6 Core/Sql/WebChannel/WebEngineQuick、SQLite WAL、FableCut 原生 JavaScript、现有 Supabase 客户端/Edge Function、CTest。

## Global Constraints

- `project.json` 是项目唯一事实源；偏好不得写入项目文件、素材库或项目导出包。
- 偏好只用于新增素材/新建组件；已有片段、已有素材和已有组件不得被自动改写或主动建议替换。
- 属性身份必须由 `componentId + componentFamily + componentVersion + manifestHash + semanticPath + propertyPath + valueType` 完整确定。
- 只记录用户确认的最终值；不记录默认应用、动画中间值、拖动中间值、导入项目已有值或恢复原值的过程。
- 闲置阈值固定为 300 秒；批量写入触发于闲置、项目切换/关闭、Edward 正常退出和导出开始。
- 方案收敛只发生于闲置批次后、打开项目、导出开始和设置中的立即更新。
- 默认方案至少需要三次明确确认或两次不同的新建会话；最多保留 A/B/C 三套方案。
- SQLite 使用 WAL 和后台批处理；未写入内存事实在崩溃或强制退出时允许丢失。
- Supabase 上传/下载必须由用户主动触发，采用事实合并，不直接覆盖本地方案。
- 严禁 Component IR、理解层、转换层，以及 OpenShot、DaVinci Resolve、Premiere 运行依赖。
- 预览和导出继续使用 FableCut 统一浏览器场景；偏好服务不参与渲染。

---

### Task 1: 建立 PreferenceStore 本地事实库与方案收敛器

**Files:**
- Create: `src/desktop/include/edward/desktop/preference_store.hpp`
- Create: `src/desktop/src/preference_store.cpp`
- Modify: `src/desktop/CMakeLists.txt`
- Create: `tests/desktop/test_preference_store.cpp`
- Modify: `tests/desktop/CMakeLists.txt`

**Interfaces:**
- Consumes: `QStandardPaths::AppLocalDataLocation`、manifest 身份和值校验结果。
- Produces: `edward::desktop::PreferenceStore`，供 Task 2 的 Qt WebChannel 和生命周期钩子调用。

建议公开接口保持窄而稳定：

```cpp
class PreferenceStore final : public QObject {
  Q_OBJECT
public:
  explicit PreferenceStore(QObject* parent = nullptr);
  QVariantMap creationPreferences(const QVariantMap& identity) const;
  bool recordConfirmedPropertyChange(const QVariantMap& observation);
  bool flushPendingPreferences();
  bool compilePreferences();
  QVariantMap status() const;
  void setDatabasePathForTests(const QString& path);
};
```

- [ ] **Step 1: Write the failing database/schema tests**

在 `test_preference_store.cpp` 中使用 `QTemporaryDir`，覆盖：数据库首次创建、WAL 开启、事实表唯一 `eventId`、完整身份键区分文字颜色和边框颜色、项目字段不会出现在持久化 JSON 中。

```cpp
PreferenceStore store;
store.setDatabasePathForTests(directory.filePath("preferences.sqlite"));
assert(store.recordConfirmedPropertyChange(observation("text", "color", "#0000ff")));
assert(store.recordConfirmedPropertyChange(observation("border", "color", "#ff0000")));
assert(store.flushPendingPreferences());
assert(store.creationPreferences(identity("text")).value("color") !=
       store.creationPreferences(identity("border")).value("color"));
```

- [ ] **Step 2: Run the focused test and verify it fails**

运行：

```bash
cmake --build build --target test_preference_store -j2
ctest --test-dir build -R desktop.preference_store --output-on-failure
```

预期：因 `PreferenceStore` 和测试注册尚不存在而失败。

- [ ] **Step 3: Implement schema and transaction boundaries**

在 `preference_store.cpp` 中创建三张表：

```sql
CREATE TABLE IF NOT EXISTS preference_events (
  event_id TEXT PRIMARY KEY,
  installation_id TEXT NOT NULL,
  identity_key TEXT NOT NULL,
  component_id TEXT NOT NULL,
  component_family TEXT NOT NULL,
  component_version TEXT NOT NULL,
  manifest_hash TEXT NOT NULL,
  semantic_path TEXT NOT NULL,
  property_path TEXT NOT NULL,
  value_type TEXT NOT NULL,
  value_json TEXT NOT NULL,
  creation_session_id TEXT NOT NULL,
  source TEXT NOT NULL,
  created_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS preference_profiles (
  identity_key TEXT PRIMARY KEY,
  profile_json TEXT NOT NULL,
  revision INTEGER NOT NULL,
  updated_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS preference_meta (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);
```

在打开数据库时执行 `PRAGMA journal_mode=WAL`，所有待提交事实使用一个事务；重复 `event_id` 必须幂等，不得产生第二条事实。

- [ ] **Step 4: Implement identity validation and three-profile compilation**

实现以下内部函数，并在头文件中保留可测试的最小声明：

```cpp
QString makeIdentityKey(const QVariantMap& identity);
bool isValidObservation(const QVariantMap& observation, QString* error);
QVariantList compileTopProfiles(const QList<QVariantMap>& events);
```

组合候选只能来自同一个 `creationSessionId`；单属性统计不得跨完整身份键。方案 A/B/C 只有达到稳定性门槛且值满足类型/范围校验时才写入 `preference_profiles`。

- [ ] **Step 5: Add failure fallback and bounded queue behavior**

数据库打不开、schema 版本不兼容或 JSON 无法解析时，返回系统默认值并保留上一版有效方案；不抛出会阻塞编辑器的异常。为内存队列和单批次设置明确上限，超限时保留最新确认值并丢弃旧的未提交中间事实。

- [ ] **Step 6: Run the focused test and commit**

运行：

```bash
cmake --build build --target test_preference_store -j2
ctest --test-dir build -R desktop.preference_store --output-on-failure
```

提交：

```bash
git add src/desktop/include/edward/desktop/preference_store.hpp \
  src/desktop/src/preference_store.cpp src/desktop/CMakeLists.txt \
  tests/desktop/test_preference_store.cpp tests/desktop/CMakeLists.txt
git commit -m "feat: add local preference store"
```

### Task 2: 接入 Edward 生命周期与 Qt WebChannel

**Files:**
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/src/main.cpp`
- Modify: `src/desktop/qml/Workbench.qml`
- Modify: `src/desktop/CMakeLists.txt`
- Create: `tests/desktop/test_preference_bridge.cpp`
- Modify: `tests/desktop/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的 `PreferenceStore`。
- Produces: 页面侧 `window.edwardPreferences` 异步对象，以及项目/导出生命周期通知。

- [ ] **Step 1: Write bridge and lifecycle failing tests**

测试必须验证：

```cpp
assert(runtime.flushPreferencesForProjectClose());
assert(runtime.flushPreferencesForExport());
assert(runtime.preferenceStoreStatus().value("pending").toInt() == 0);
```

另测 WebChannel 注册对象名称固定为 `preferenceStore`，避免页面脚本和宿主名称漂移。

- [ ] **Step 2: Run the focused bridge test and verify it fails**

运行：

```bash
cmake --build build --target test_preference_bridge -j2
ctest --test-dir build -R desktop.preference_bridge --output-on-failure
```

- [ ] **Step 3: Register PreferenceStore in the existing runtime**

在 `WorkbenchRuntime` 中持有 `PreferenceStore`，增加 `Q_INVOKABLE` 生命周期入口：

```cpp
Q_INVOKABLE bool flushPreferencesForProjectClose();
Q_INVOKABLE bool flushPreferencesForExport();
Q_INVOKABLE bool compilePreferencesNow();
Q_INVOKABLE QVariantMap preferenceStoreStatus() const;
```

项目切换/关闭和 Edward `aboutToQuit` 调用事实批处理；导出开始先提交事实事务，再异步触发收敛。收敛不得阻塞导出帧捕获。

- [ ] **Step 4: Expose the narrow WebChannel**

在 `main.cpp` 创建 `QWebChannel`，注册唯一对象名 `preferenceStore`；在 `Workbench.qml` 的 `WebEngineView` 设置 `webChannel`。只暴露 Task 1 的窄接口，不暴露文件系统、项目路径或任意 Qt 对象。

- [ ] **Step 5: Add lifecycle wiring**

在项目加载、项目关闭、导出按钮进入和应用退出路径分别调用对应接口。页面关闭或 WebEngine 重载不能直接清空内存队列；由 Qt 生命周期统一负责提交。

- [ ] **Step 6: Run bridge, desktop, and startup checks and commit**

运行：

```bash
cmake --build build --target test_preference_bridge test_workbench_plugins -j2
ctest --test-dir build -R 'desktop\.(preference_bridge|workbench_plugins)' --output-on-failure
```

提交：

```bash
git add src/desktop/include/edward/desktop/workbench_runtime.hpp \
  src/desktop/src/workbench_runtime.cpp src/desktop/src/main.cpp \
  src/desktop/qml/Workbench.qml src/desktop/CMakeLists.txt \
  tests/desktop/test_preference_bridge.cpp tests/desktop/CMakeLists.txt
git commit -m "feat: bridge preferences to FableCut lifecycle"
```

### Task 3: 在 FableCut 中接入 manifest 身份、新增默认值和确认事件

**Files:**
- Create: `third_party/FableCut/preference-client.js`
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/app.js`
- Create: `third_party/FableCut/test/preferences.test.js`
- Modify: `third_party/FableCut/package.json`

**Interfaces:**
- Consumes: Task 2 的 `window.edwardPreferences`，以及现有组件 manifest 的 `id/runtime/entry/props` 数据。
- Produces: 新建组件时经过校验的默认 `props`，以及只包含最终确认值的 observation。

- [ ] **Step 1: Write failing JavaScript tests**

覆盖以下行为：

```js
const identity = makePreferenceIdentity(manifest, "root.border", "color");
assert.equal(identity.propertyPath, "color");
assert.notEqual(
  makePreferenceIdentity(manifest, "root.title", "color").key,
  identity.key
);

const created = applyCreationPreferences(baseProps, profileA, manifest);
assert.equal(created.radius, 5);
assert.deepEqual(recordedSources(), ["user-confirmed"]);
```

测试还必须确认默认应用不会调用事实记录接口，属性拖动中间值不会产生事件，撤销到原值不会产生事件。

- [ ] **Step 2: Run the focused JavaScript test and verify it fails**

运行：

```bash
npm test -- --test-name-pattern=preferences
```

预期：因客户端和身份 helper 尚不存在而失败。

- [ ] **Step 3: Implement `preference-client.js`**

加载 `qrc:///qtwebchannel/qwebchannel.js` 后连接宿主 `preferenceStore`，提供：

```js
window.edwardPreferences = {
  getCreationPreferences(identity),
  recordConfirmedPropertyChange(observation),
  flushPendingPreferences(),
  compilePreferences()
};
```

宿主不可用时返回空方案和明确的 `unavailable` 状态，不能阻塞新增组件。

- [ ] **Step 4: Add deterministic identity and default application**

在 `app.js` 中新增 `makePreferenceIdentity(manifest, semanticPath, propertyPath, valueType)`，只使用 manifest 声明的字段。新增组件路径在创建实例前读取方案 A/B/C，先进行类型、范围和枚举校验，再合并系统默认值。

现有项目加载、已有片段选择和项目导入路径不得调用默认偏好。

- [ ] **Step 5: Record only confirmed final values**

在属性检查器的 change/blur/pointerup/Enter 提交点记录 observation；滑块 pointermove、画布拖动和动画时钟更新只更新内存 props，不记录事实。每次新建组件创建 `creationSessionId`，同一设置会话的最终确认值才允许参与组合候选。

- [ ] **Step 6: Add the inspector entry without changing existing clip data**

在当前“名称”位置增加“偏好”入口，用于新增流程选择 A/B/C；名称移动到基础信息区域。选中已有片段时入口只读或隐藏，不能把所选方案写回 clip 的 `props` 或 `project.json`。

- [ ] **Step 7: Run FableCut tests and commit**

运行：

```bash
npm test -- --test-name-pattern=preferences
node --test test/*.test.js
```

提交：

```bash
git add third_party/FableCut/preference-client.js third_party/FableCut/index.html \
  third_party/FableCut/app.js third_party/FableCut/test/preferences.test.js \
  third_party/FableCut/package.json
git commit -m "feat: apply preferences to new FableCut components"
```

### Task 4: 实现 Supabase 显式同步和设置入口

**Files:**
- Create: `src/resources/include/edward/resources/preference_sync_client.hpp`
- Create: `src/resources/src/preference_sync_client.cpp`
- Modify: `src/resources/CMakeLists.txt`
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/qml/Workbench.qml`
- Create: `tests/resources/test_preference_sync_client.cpp`
- Modify: `tests/resources/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的事实导入/导出接口、现有 Supabase Auth 会话和匿名客户端配置。
- Produces: 用户主动触发的上传/下载操作；下载后只合并事实并在本地重新收敛。

- [ ] **Step 1: Write failing sync tests**

测试固定行为：事件按 `eventId` 去重；服务端返回的非法 manifest、属性路径、类型和值范围被拒绝；网络失败不影响本地方案。

```cpp
const auto merged = client.mergeFacts(localFacts, remoteFacts);
assert(merged.size() == 2); // 相同 eventId 只保留一条
assert(!client.acceptRemoteFact(invalidManifestFact));
```

- [ ] **Step 2: Run focused sync tests and verify failure**

运行：

```bash
cmake --build build --target test_preference_sync_client -j2
ctest --test-dir build -R resources.preference_sync_client --output-on-failure
```

- [ ] **Step 3: Implement fact envelope and validation**

同步载荷只包含事实、schema 版本和 manifest 版本；明确排除项目 ID、项目名称、路径、时间线和素材内容。客户端在上传前和下载后都校验完整身份键和值范围。

- [ ] **Step 4: Add explicit settings actions**

在设置界面增加“上传偏好”和“下载偏好”两个明确入口。不得在登录、打开项目、普通保存或导出时自动上传/下载。下载成功后调用本地 `compilePreferences()`，不改写项目。

- [ ] **Step 5: Run sync and existing resource tests and commit**

运行：

```bash
cmake --build build --target test_preference_sync_client test_supabase_auth_client -j2
ctest --test-dir build -R 'resources\.(preference_sync_client|supabase_auth_client)' --output-on-failure
```

提交：

```bash
git add src/resources/include/edward/resources/preference_sync_client.hpp \
  src/resources/src/preference_sync_client.cpp src/resources/CMakeLists.txt \
  src/desktop/include/edward/desktop/workbench_runtime.hpp \
  src/desktop/src/workbench_runtime.cpp src/desktop/qml/Workbench.qml \
  tests/resources/test_preference_sync_client.cpp tests/resources/CMakeLists.txt
git commit -m "feat: add explicit preference sync"
```

### Task 5: 完成跨层验收、性能检查和文档闭环

**Files:**
- Create: `tests/e2e/test_edward_preferences.cpp`
- Modify: `tests/e2e/CMakeLists.txt`
- Modify: `third_party/FableCut/CLAUDE.md`
- Modify: `docs/operation-log/2026-09-15-edward-preferences.md`
- Modify: `.edward/acceptance/current.json`

**Interfaces:**
- Consumes: Tasks 1–4 的完整本地、桥接、FableCut 和同步行为。
- Produces: Edward 0.6.0 偏好功能的可执行验收记录。

- [ ] **Step 1: Write the end-to-end acceptance cases**

至少覆盖：

1. 新建文字、矩形和 Card 6 时读取方案 A；
2. 文字颜色和边框颜色分别统计；
3. 修改已有片段后后台收敛不改变 `project.json`；
4. 300 秒闲置、项目切换、正常退出和导出均能批量落盘；
5. 导出开始不会因偏好收敛等待而阻塞帧捕获；
6. Supabase 未登录或网络失败时仍能本地创建组件；
7. 偏好数据库损坏时回退系统默认值；
8. 预览和导出仍由同一 FableCut 场景负责，偏好层不生成中间转换数据。

- [ ] **Step 2: Add acceptance checks**

在 `.edward/acceptance/current.json` 增加可执行检查：SQLite schema、属性隔离、触发机制、项目 JSON 不变、显式同步和失败回退。每项记录命令、预期结果和实际结果。

- [ ] **Step 3: Run focused and broad validation**

运行：

```bash
cmake --build build -j2
ctest --test-dir build -R 'desktop\.(preference_store|preference_bridge)|resources\.preference_sync_client|e2e\.edward_preferences' --output-on-failure
node --test third_party/FableCut/test/*.test.js
```

对浏览器可见流程启动 Edward，验证新增组件默认值、已有片段不变、项目 JSON 无偏好字段、预览画面和导出画面未因偏好桥接发生变化。

- [ ] **Step 4: Update operational documentation**

在 FableCut 主手册中记录偏好只作用于新增组件、Qt WebChannel 边界和测试命令；在操作记录中记录数据库位置、触发规则、验证结果和未涉及的项目数据。

- [ ] **Step 5: Commit the completed implementation and acceptance evidence**

```bash
git add tests/e2e/test_edward_preferences.cpp tests/e2e/CMakeLists.txt \
  third_party/FableCut/CLAUDE.md \
  docs/operation-log/2026-09-15-edward-preferences.md \
  .edward/acceptance/current.json
git commit -m "test: verify Edward preference workflow"
```

## 计划自审

- 规格中的本地 SQLite、属性隔离、三套方案、300 秒闲置、项目/导出触发、已有片段保护、显式 Supabase 同步和失败回退均有对应任务。
- 没有使用 Component IR、理解层或渲染转换层；桥接只传递已校验的原生属性数据。
- 所有跨任务接口名称已固定：`PreferenceStore`、`preferenceStore`、`window.edwardPreferences`、`creationPreferences`、`recordConfirmedPropertyChange`、`flushPendingPreferences`、`compilePreferences`。
- 计划中的测试均先写失败测试，再实现，再运行范围最小的检查。
- 计划只涉及偏好功能需要的文件，不要求清理当前工作区中其他未提交改动。
