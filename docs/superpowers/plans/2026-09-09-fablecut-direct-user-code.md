# FableCut Direct User-Code Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 FableCut 中让用户提供的 React/GSAP/CSS/SVG 原始代码直接进入同一预览场景，能由属性栏修改并通过同一场景逐帧导出。

**Architecture:** 扩展 FableCut 本地服务提供受限的用户组件目录与 manifest；浏览器端直接加载用户提供的浏览器可执行 JavaScript/ES Module，在每个时间线组件实例中挂载真实 React/DOM/SVG 图层，GSAP 直接运行并受统一时间控制。预览和导出都使用同一个场景根节点，ffmpeg 只负责最终编码，不引入 IR、JSX/TSX 编译或转换器。

**Tech Stack:** Node.js 18+、原生 JavaScript、Chromium、React、GSAP、CSS/SVG、Canvas capture、ffmpeg、Node test runner。

## Global Constraints

- 禁止 IR、组件转换器、React 转 SVG、GSAP 转关键帧和 CSS 自定义解析器。
- 用户源代码必须在本地受控页面中直接运行；默认不得访问项目目录外文件或网络。
- 预览和导出必须调用同一场景渲染路径与统一时间控制。
- 验证输入必须是浏览器可直接执行的 JavaScript/ES Module；不接受需要 JSX/TSX 编译的源码。
- React/ReactDOM/GSAP 由用户代码自行提供或通过已加载的浏览器运行时提供；验证阶段不引入 Babel/Vite 编译链。
- 所有新增文件严格位于当前 Edward 项目目录内。
- 不接入 Resolve、Fusion、MLT 或现有 Edward C++ 主程序；这些属于第二阶段。

---

### Task 1: 用户组件 manifest 与本地服务

**Files:**
- Create: `third_party/FableCut/components/demo/manifest.json`
- Create: `third_party/FableCut/components/demo/component.jsx`
- Create: `third_party/FableCut/components/demo/component.css`
- Modify: `third_party/FableCut/paths.js`
- Modify: `third_party/FableCut/server.js`
- Test: `third_party/FableCut/test/user-components.test.js`

**Interfaces:**
- `GET /api/components` 返回组件 manifest 列表。
- `GET /components/<component>/<asset>` 只服务组件根目录内文件。
- manifest 字段：`name`、`entry`、`style`、`props`。

- [ ] **Step 1: 写失败测试**：验证目录扫描只返回合法 manifest，拒绝 `..` 路径和目录外资源，并验证示例 manifest 的公开属性字段。
- [ ] **Step 2: 运行 `node --test test/user-components.test.js`，确认测试因接口不存在而失败。**
- [ ] **Step 3: 实现组件目录路径、manifest 读取、字段校验和只读静态服务；服务端拒绝路径穿越、脚本目录外文件和非法 JSON。**
- [ ] **Step 4: 写入 demo React 组件：组件接收 `title`、`color`、`x`、`time`，内部使用 GSAP timeline 控制一个 SVG/DOM 元素，并由 CSS 提供动画样式。**
- [ ] **Step 5: 运行 `node --test test/user-components.test.js`，确认通过。**
- [ ] **Step 6: 提交 `git add third_party/FableCut/components third_party/FableCut/paths.js third_party/FableCut/server.js third_party/FableCut/test/user-components.test.js && git commit -m "feat: serve direct user components"`。**

### Task 2: 浏览器直接挂载 React/GSAP/CSS/SVG 图层

**Files:**
- Create: `third_party/FableCut/component-runtime.js`
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/style.css`
- Modify: `third_party/FableCut/app.js`
- Test: `third_party/FableCut/test/component-runtime.test.js`

**Interfaces:**
- `createUserComponentInstance({ manifest, clip, host, onError })` 直接挂载用户入口并返回 `{ update(props, time), destroy() }`。
- `update(props, time)` 只更新原始组件运行时输入，不生成 IR 或转换数据。

- [ ] **Step 1: 写失败测试**：验证实例创建、属性更新、时间更新、销毁和组件异常回调的公开行为。
- [ ] **Step 2: 运行测试确认失败。**
- [ ] **Step 3: 实现浏览器运行时加载：在组件实例容器中加载 React/ReactDOM/GSAP，注入用户 CSS，并保留组件真实 DOM/SVG 输出。**
- [ ] **Step 4: 在 `app.js` 的时间线渲染入口挂载实例，按 clip 的 `start`/`duration` 控制显示，并把所有实例放入同一个 scene root；层级使用 DOM 顺序与 `z-index`。**
- [ ] **Step 5: 实现属性栏 manifest 控件：string、number、boolean、color、select；修改后调用实例 `update` 并保存项目。**
- [ ] **Step 6: 运行浏览器运行时测试和现有语法检查，确认通过。**
- [ ] **Step 7: 提交 `git add third_party/FableCut/component-runtime.js third_party/FableCut/index.html third_party/FableCut/style.css third_party/FableCut/app.js third_party/FableCut/test/component-runtime.test.js && git commit -m "feat: mount direct user component layers"`。**

### Task 3: 统一时间控制与交错图层预览

**Files:**
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/component-runtime.js`
- Test: `third_party/FableCut/test/component-preview.test.js`

**Interfaces:**
- `setSceneTime(seconds, { export: boolean })` 更新所有活动视频、用户组件、SVG/CSS 和 GSAP 实例。
- `sceneRoot` 是唯一预览与导出的画面根节点。

- [ ] **Step 1: 写失败测试**：建立视频→React→SVG→GSAP→视频交错时间线，验证每个时间点的活动层、层级和公开属性。
- [ ] **Step 2: 运行测试确认失败。**
- [ ] **Step 3: 实现统一时间分发；GSAP 实例直接 `seek`，视频直接定位 `currentTime`，React 组件接收原始 `time`，CSS/SVG 保留浏览器动画。**
- [ ] **Step 4: 处理 inactive 图层暂停/隐藏、组件错误提示和层叠上下文，确保交错图层仍在同一 scene root。**
- [ ] **Step 5: 运行测试并用 Chromium fixture 截取至少 3 个时间点，确认层级和属性更新正确。**
- [ ] **Step 6: 提交 `git add third_party/FableCut/app.js third_party/FableCut/component-runtime.js third_party/FableCut/test/component-preview.test.js && git commit -m "feat: synchronize mixed preview layers"`。**

### Task 4: 同场景逐帧导出

**Files:**
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/server.js`
- Test: `third_party/FableCut/test/component-export.test.js`

**Interfaces:**
- `captureSceneFrame(seconds)` 返回当前 `sceneRoot` 的完整 RGBA/JPEG 帧。
- 现有 `/api/export/begin`、`/api/export/frame`、`/api/export/end` 接口继续作为编码通道。

- [ ] **Step 1: 写失败测试**：验证导出循环按项目 fps 调用 `setSceneTime`，每帧包含交错用户组件和视频图层，且异常会终止导出并报告原因。
- [ ] **Step 2: 运行测试确认失败。**
- [ ] **Step 3: 让导出循环调用与预览相同的 `setSceneTime`，等待 React commit、GSAP 更新、字体、图片和视频帧稳定后再 capture。**
- [ ] **Step 4: 保留 Fast JPEG+ffmpeg 作为基线；WebCodecs/MediaRecorder 只在现有能力探测通过时启用。**
- [ ] **Step 5: 运行导出测试，检查输出文件存在、帧数正确、ffprobe 能读取时长和帧率。**
- [ ] **Step 6: 提交 `git add third_party/FableCut/app.js third_party/FableCut/server.js third_party/FableCut/test/component-export.test.js && git commit -m "feat: export direct component scenes"`。**

### Task 5: 最小验证报告与回归门

**Files:**
- Create: `third_party/FableCut/test/fixtures/mixed-user-scene/project.json`
- Create: `third_party/FableCut/test/fixtures/mixed-user-scene/README.md`
- Modify: `third_party/FableCut/test/` only where fixture assertions are required
- Modify: `docs/operation-log/2026-09-01-agent-reach-install.md`

- [ ] **Step 1: 固化 mixed fixture：原始 MP4/MOV 代理、React+GSAP、CSS 动画、SVG 动画和交错轨道。**
- [ ] **Step 2: 运行完整 FableCut 检查：`node --check server.js && node --check app.js && node --check mcp-server.js && npm test`。**
- [ ] **Step 3: 运行浏览器预览/导出验收，保存帧差异、导出文件信息和失败原因。**
- [ ] **Step 4: 只有当属性栏修改、交错层级、实时预览、逐帧导出和错误隔离全部通过时，才记录第一阶段完成。**
- [ ] **Step 5: 提交 `git add third_party/FableCut/test/fixtures docs/operation-log/2026-09-01-agent-reach-install.md && git commit -m "test: verify direct user scene export"`。**

## 第二阶段入口

第一阶段通过后，另写 Edward 集成计划：用 Qt WebEngine 或独立 Chromium 壳包装 FableCut 本地服务，加入 Windows/macOS 安装、数据目录、自动更新和崩溃恢复。第二阶段不得重新引入 IR、Resolve、Fusion 或 MLT 视觉真源。
