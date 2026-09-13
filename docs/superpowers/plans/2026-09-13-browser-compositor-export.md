# 统一浏览器合成与导出 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 FableCut/Edward 使用同一浏览器合成器完成预览与离屏逐帧导出，确保 Card 6、视频、React、GSAP、SVG、HTML/CSS 在 4K 与 24–60 FPS 下不丢失、不变形。

**Architecture:** `project.json` 是唯一事实源；FableCut 提供确定性 `renderFrame(project, frame, outputSpec)`，预览和导出共用组件运行时、时间轴时钟和 viewport。Qt 只传递输出规格、接管文件路径和取消状态，ffmpeg 只编码完整合成帧与音频。

**Tech Stack:** Qt 6/QML、Qt WebEngine、JavaScript、Canvas/DOM/SVG、Node.js 本地 FableCut 服务、ffmpeg、现有 CMake 测试。

## Global Constraints

- 不使用 Resolve、Premiere 或 Component IR 转换层。
- 不复制第二份导出时间线状态；所有导出数据来自 FableCut `project.json`。
- 输出宽高、FPS、pixelRatio、crop 必须显式传递；默认 `crop: none`。
- 资源、字体、视频帧和插件异步更新未完成时必须阻止该帧导出并报告原因。
- 所有新增文件严格位于当前项目目录；不把密钥或本地路径写入源码。

---

### Task 1: 固化 FableCut 输出规格与时间轴快照

**Files:**
- Modify: `third_party/FableCut/app.js:367-400, 5460-5495`
- Create: `third_party/FableCut/export-compositor.js`
- Test: `third_party/FableCut/test/export-compositor.test.js`

**Interfaces:**
- `normalizeOutputSpec(input, project)` → `{width,height,fps,pixelRatio,crop,format,quality}`。
- `createExportSnapshot(project)` → 深拷贝且包含轨道、媒体、组件、动画和资源引用的快照。
- `renderFrameAt(snapshot, frame, outputSpec)` → `Promise<{canvas, width, height, frame}>`。

- [ ] Step 1: 在测试中覆盖默认 16:9、4K 3840×2160、60 FPS、`crop: none`，并断言快照不引用可变 project 对象。
- [ ] Step 2: 运行 `node --test third_party/FableCut/test/export-compositor.test.js`，确认新接口测试失败。
- [ ] Step 3: 实现 `export-compositor.js` 的规格校验、快照复制和逐帧调度；拒绝非正整数宽高、FPS 大于 60、非 `mp4` 格式和非 `none` 裁切。
- [ ] Step 4: 让 `drawFrame` 和导出入口调用同一规格归一化函数，不再从历史 `exportFrame` 推导输出裁切。
- [ ] Step 5: 重新运行测试并提交：`git add third_party/FableCut/app.js third_party/FableCut/export-compositor.js third_party/FableCut/test/export-compositor.test.js && git commit -m "feat: define deterministic browser export spec"`。

### Task 2: 把 DOM/组件层纳入完整合成帧

**Files:**
- Modify: `third_party/FableCut/component-runtime.js:1-60`
- Modify: `third_party/FableCut/app.js:5472-5490, 6839-6910`
- Modify: `third_party/FableCut/index.html`（合成根节点和导出所需脚本顺序）
- Test: `third_party/FableCut/test/export-compositor.test.js`

**Interfaces:**
- `window.fablecutDirectComponents.prepareFrame(clips, time, viewport)` → `Promise<void>`。
- `window.fablecutDirectComponents.captureCompositeFrame(outputSpec)` → `Promise<ImageBitmap|HTMLCanvasElement>`。

- [ ] Step 1: 增加失败测试：Card 6 组件在逐帧准备完成前不得返回捕获结果，组件异常必须 reject 且包含组件 id。
- [ ] Step 2: 运行测试确认失败。
- [ ] Step 3: 扩展组件运行时，使 `mount/update` 返回的 Promise 被统一等待；所有组件挂载到合成根节点，更新时注入 `{time, viewport}`。
- [ ] Step 4: 为导出创建独立合成 surface，将 Canvas、DOM、SVG 和组件层按轨道顺序绘制/栅格化到同一完整帧；导出帧不包含选中框、调试层或交互控件。
- [ ] Step 5: 逐帧导出调用 `prepareFrame` 后再捕获，失败时返回阶段、帧号和组件 id；运行 Node 测试并提交：`git add third_party/FableCut/app.js third_party/FableCut/component-runtime.js third_party/FableCut/index.html third_party/FableCut/test/export-compositor.test.js && git commit -m "feat: capture browser component layers in export"`。

### Task 3: Qt 输出规格、路径和取消桥接

**Files:**
- Modify: `src/desktop/qml/Workbench.qml:2448-2585`
- Modify: `src/desktop/include/edward/desktop/workbench_runtime.hpp:70-180, 315-330, 430-445`
- Modify: `src/desktop/src/workbench_runtime.cpp:3170-3260`
- Modify: `src/desktop/src/main.cpp:45-80`
- Test: `tests/desktop/test_visual_routes.cpp`, `tests/desktop/test_workbench_plugins.cpp`

**Interfaces:**
- QML 调用 `workbenchRuntime.beginFablecutExport(outputPath, width, height, fps, quality)`。
- C++ 属性 `pendingFablecutExportPath` 只保存一次待接管目标路径。
- 下载回调必须把 `QWebEngineDownloadRequest` 的目录和文件名设置为目标路径后再 `accept()`。

- [ ] Step 1: 增加静态回归断言：QML 传递宽高/FPS，明确 `crop: none`，取消调用清理函数；C++ 拒绝 0、负值和大于 60 FPS。
- [ ] Step 2: 运行 `cmake --build build --target test_visual_routes test_workbench_plugins -j2`，记录失败断言。
- [ ] Step 3: 将 QML 导出规格传入 FableCut JS；当原生 Edward 时间线为空时只调用 FableCut 导出，不再尝试读取空的 native snapshot。
- [ ] Step 4: 在 Qt 下载回调中校验目录存在、文件名为 `.mp4`，写入完成后清理 pending 状态；取消时终止 FableCut 会话并清理临时文件。
- [ ] Step 5: 重新运行两项测试并提交：`git add src/desktop/qml/Workbench.qml src/desktop/include/edward/desktop/workbench_runtime.hpp src/desktop/src/workbench_runtime.cpp src/desktop/src/main.cpp tests/desktop/test_visual_routes.cpp tests/desktop/test_workbench_plugins.cpp && git commit -m "feat: bridge deterministic FableCut export from Qt"`。

### Task 4: 导出编码与完整性校验

**Files:**
- Modify: `third_party/FableCut/server.js:630-760`
- Modify: `third_party/FableCut/app.js:6520-7130`
- Create: `third_party/FableCut/test/export-integrity.test.js`

**Interfaces:**
- `POST /api/export/begin` 接收 `{fps,width,height,format,quality,crop:"none"}`。
- `POST /api/export/frame` 接收完整 RGBA/JPEG 帧并按序写入编码器。
- `POST /api/export/end` 返回 `{src,width,height,fps,frameCount}`，不完整时返回错误。

- [ ] Step 1: 写失败测试，验证返回帧数、宽高、FPS 与请求不一致时导出失败。
- [ ] Step 2: 运行 `node --test third_party/FableCut/test/export-integrity.test.js` 确认失败。
- [ ] Step 3: 让导出会话保存输出规格、已写帧数和取消状态；结束时校验 ffmpeg 输出媒体信息。
- [ ] Step 4: 将错误报告统一为阶段、帧号、组件/资源标识，禁止静默丢帧或降级裁切。
- [ ] Step 5: 运行 Node 测试并提交：`git add third_party/FableCut/server.js third_party/FableCut/app.js third_party/FableCut/test/export-integrity.test.js && git commit -m "feat: validate exported media integrity"`。

### Task 5: 端到端一致性样例与发布验证

**Files:**
- Create: `third_party/FableCut/test/fixtures/card6-project.json`
- Create: `third_party/FableCut/test/preview-export-consistency.test.js`
- Modify: `.edward/acceptance/current.json`
- Modify: `docs/operation-log/2026-09-13-vercel-auth-recovery.md`

- [ ] Step 1: 固化包含视频、Card 6、SVG、文本和 GSAP 动画的 16:9 项目样例。
- [ ] Step 2: 测试同一帧的预览捕获与导出捕获，断言尺寸、alpha、组件存在和布局边界一致。
- [ ] Step 3: 测试 3840×2160 与 60 FPS 输出的媒体探测值，测试取消不留下半成品。
- [ ] Step 4: 运行完整检查：
  `node --test third_party/FableCut/test/*.test.js`
  `cmake --build build --target test_visual_routes test_workbench_plugins edward_app -j2`
  `git diff --check`
- [ ] Step 5: 只有所有检查通过后，将新增导出验收项标记为 `passed`，追加操作记录并提交：`git add third_party/FableCut/test .edward/acceptance/current.json docs/operation-log/2026-09-13-vercel-auth-recovery.md && git commit -m "test: verify preview export consistency"`。
