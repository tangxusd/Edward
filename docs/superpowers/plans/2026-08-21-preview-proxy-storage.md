# Edward 预览代理与统一存储实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将代理、缓存和预渲染位置接入引擎；预览可选择原画、清晰或流畅，导出始终使用原始媒体。

**Architecture:** 全局设置保存三个根目录；工程保存稳定 UUID。代理、波形缓存和预渲染文件使用 `<根目录>/<工程 UUID>/` 隔离。代理只服务预览，不能进入工程真相源或导出路径。

**Tech Stack:** C++20、Qt 6.11、FFmpeg、CMake、CTest。

## Global Constraints

- 不在 `project.json` 保存派生文件路径；仅保存工程 UUID。
- 设置变更只影响后续生成任务，不迁移或删除已有派生文件。
- 清理只能影响指定根目录中的 UUID 子目录，绝不删除工程素材或工程 JSON。
- 原画预览和所有导出直接读取原始媒体；代理缺失时安全回退原画。

---

### Task 1: 工程 UUID 与受限存储路径

**Files:**
- Create: `src/core/include/edward/core/project_identity.hpp`
- Create: `src/core/src/project_identity.cpp`
- Create: `src/media/include/edward/media/render_storage.hpp`
- Create: `src/media/src/render_storage.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `src/media/CMakeLists.txt`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Test: `tests/core/test_project_identity.cpp`
- Test: `tests/media/test_render_storage.cpp`

**Interfaces:**
- `ProjectIdentity::create()` 生成 UUID；`ProjectIdentity::parse(QString)` 只接受规范 UUID。
- `RenderStorage::paths(ProjectIdentity, RenderStorageRoots)` 返回代理、缓存、预渲染目录。
- `RenderStorage::clearDerived(RenderStorageRoots, DerivedStorageKind)` 只删除根目录内的 UUID 子目录。

- [x] **Step 1: 写失败测试**：拒绝 `../outside` 作为工程 ID；断言代理目录等于 `proxyRoot / UUID`；断言清理不影响根目录中的非 UUID 文件。
- [x] **Step 2: 确认红灯**：运行 `ctest --test-dir build/0.3-runtime -R 'core.project_identity|media.render_storage' --output-on-failure`，预期测试不存在或失败。
- [x] **Step 3: 最小实现**：新工程写 UUID；旧工程下次保存时补写 UUID；目录生成和清理均验证 UUID 路径层级。
- [x] **Step 4: 确认绿灯**：重跑同一测试；工程 JSON 不出现代理、缓存或预渲染绝对路径。
- [x] **Step 5: 提交**：`git commit -m "feat: add isolated render storage roots"`。

### Task 2: 真实代理与预览选择

**Files:**
- Create: `src/media/include/edward/media/proxy_manager.hpp`
- Create: `src/media/src/proxy_manager.cpp`
- Create: `src/media/include/edward/media/preview_session.hpp`
- Create: `src/media/src/preview_session.cpp`
- Modify: `src/media/CMakeLists.txt`
- Test: `tests/media/test_proxy_manager.cpp`
- Test: `tests/media/test_preview_session.cpp`

**Interfaces:**
- `ProxyManager::ensureProxy(source, project, quality)` 返回可读代理路径或失败。
- `PreviewSession::sourceFor(source)` 在代理可用时返回代理，否则返回原始素材。
- `ExportJob` 不引用 `PreviewSession` 或 `ProxyManager`。

- [ ] **Step 1: 写失败测试**：清晰代理最大宽高为 1920；流畅代理最大宽高为 854×480；原画模式返回源路径；源变更后旧代理不被选用；导出测试仍读取源路径。
- [ ] **Step 2: 确认红灯**：运行 `ctest --test-dir build/0.3-runtime -R 'media.proxy_manager|media.preview_session' --output-on-failure`，预期失败。
- [x] **Step 3: 最小实现**：FFmpeg 写同目录临时文件，`MediaProbe` 验证后原子替换；稳定代理名包含源规范路径、文件大小和修改时间；清晰/流畅缺失时回退原画。
- [x] **Step 4: 确认绿灯**：运行 `ctest --test-dir build/0.3-runtime -R 'media.proxy_manager|media.preview_session|media.export_job' --output-on-failure`。
- [x] **Step 5: 提交**：`git commit -m "feat: add preview proxy generation"`。

### Task 3: 设置页与工作台预览接线

**Files:**
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp`
- Modify: `src/desktop/src/workbench_runtime.cpp`
- Modify: `src/desktop/qml/Workbench.qml`
- Modify: `src/desktop/qml/EdwardPreview.qml`
- Modify: `tests/desktop/test_workbench_plugins.cpp`
- Modify: `tests/desktop/test_visual_routes.cpp`

**Interfaces:**
- 工作台暴露 `previewQuality`、三个存储目录和 `clearDerivedStorage(kind)`。
- 设置变更驱动 `PreviewSession`；导出仍构造只含原始媒体的 `TimelineSnapshot`。

- [x] **Step 1: 写失败测试**：选择流畅预览后请求代理；代理未就绪时仍能看到原画；清理代理不删除工程文件；导出仍成功。
- [x] **Step 2: 确认红灯**：运行 `QT_QPA_PLATFORM=offscreen ctest --test-dir build/0.3-runtime -R 'desktop.(visual_routes|workbench_plugins)' --output-on-failure`，预期失败。
- [x] **Step 3: 最小实现**：在既有“项目设置”接入目录和原画/清晰/流畅；首次代理生成异步执行并显示真实状态；清理只删除选定根目录中各 UUID 子目录。
- [x] **Step 4: 确认绿灯**：运行 `QT_QPA_PLATFORM=offscreen ctest --test-dir build/0.3-runtime -R 'desktop.(visual_routes|workbench_plugins)|media.(proxy_manager|preview_session|export_job)' --output-on-failure`。
- [x] **Step 5: 完整回归与提交**：运行离屏完整 CTest、`git diff --check`，追加操作日志后提交 `feat: connect preview storage settings`。
