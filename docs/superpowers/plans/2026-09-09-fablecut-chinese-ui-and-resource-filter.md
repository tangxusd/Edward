# FableCut 中文界面与资源过滤实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 FableCut 第一阶段界面完整切换为中文优先的中英文可切换界面，并从所有资源列表中排除以 `.` 开头的 macOS 隐藏/AppleDouble 文件，同时保留 Adjust 和画面调节功能。

**Architecture:** 在 FableCut 前端增加轻量的语言资源表和 `t(key)` 查找函数，所有用户可见固定文案通过资源表渲染；用户项目名称、素材名称、组件内容和用户代码保持原文。Node 服务端在媒体库、默认资源库和组件目录扫描入口统一拒绝 basename 以 `.` 开头的文件，前端再做一次防御性过滤。

**Tech Stack:** 原生 JavaScript、Node.js 18+、HTML、CSS、Node test runner。

## Global Constraints

- 开发阶段默认中文，系统支持中文和英文切换。
- Adjust、亮度/对比度/饱和度等画面调节功能保留，不在本阶段删除。
- 禁止使用百度搜索引擎。
- 不引入 IR、组件转换器或新的生产依赖。
- 所有修改严格位于当前项目目录。
- 以 `.` 开头的文件不得出现在媒体列表、资源库列表、组件列表或素材扫描结果中。

---

### Task 1: 建立中英文界面资源与语言切换

**Files:**
- Create: `third_party/FableCut/i18n.js`
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/app.js`
- Modify: `third_party/FableCut/style.css`
- Test: `third_party/FableCut/test/i18n.test.js`

**Interfaces:**
- `createI18n(initialLanguage)` 返回 `{ getLanguage(), setLanguage(lang), t(key, vars) }`。
- 支持语言值 `zh-CN`、`en-US`；未知语言回退 `zh-CN`。
- `window.fablecutI18n` 暴露当前页面语言控制，不翻译用户输入内容。

- [ ] **Step 1: 写失败测试**：验证中文默认值、英文切换、未知语言回退、缺失键回退和变量插值。
- [ ] **Step 2: 运行 `node --test test/i18n.test.js`，确认测试因模块不存在而失败。**
- [ ] **Step 3: 实现 `i18n.js`**：建立覆盖主界面、时间线、属性栏、导入、导出、设置、错误提示和资源列表的中文/英文键值表。
- [ ] **Step 4: 在 `index.html` 增加语言选择控件和必要的 `data-i18n` 标记；默认语言设置为中文。**
- [ ] **Step 5: 在 `app.js` 接入 `t()`**：将固定界面文案、toast、alert、导出状态、空列表提示和属性标签改为资源键；项目名、素材名、组件标题等用户数据继续原样显示。
- [ ] **Step 6: 增加语言切换事件**：切换后重新渲染静态界面、资源列表、属性栏和导出设置，并将选择保存在项目目录设置或浏览器本地设置中。
- [ ] **Step 7: 运行 `node --check app.js && node --test test/i18n.test.js`，确认通过。**
- [ ] **Step 8: 提交 `git add third_party/FableCut/i18n.js third_party/FableCut/index.html third_party/FableCut/app.js third_party/FableCut/style.css third_party/FableCut/test/i18n.test.js && git commit -m "feat: add Chinese-first bilingual interface"`。**

### Task 2: 服务端统一过滤 macOS 隐藏文件

**Files:**
- Modify: `third_party/FableCut/server.js`
- Modify: `third_party/FableCut/paths.js`
- Test: `third_party/FableCut/test/resource-filter.test.js`

**Interfaces:**
- `isVisibleResourceName(name)` 返回布尔值；仅当 basename 非空且不以 `.` 开头时返回 `true`。
- `/api/media`、`/api/library`、`/api/components` 以及组件/媒体静态服务均使用同一过滤规则。

- [ ] **Step 1: 写失败测试**：用 `._current.json`、`.DS_Store`、`.hidden`、`Card.svg` 和嵌套路径验证列表过滤和静态访问规则。
- [ ] **Step 2: 运行 `node --test test/resource-filter.test.js`，确认测试先失败。**
- [ ] **Step 3: 在服务端增加 `isVisibleResourceName()`，扫描目录时在读取 metadata 前过滤 basename。**
- [ ] **Step 4: 对媒体库、默认资源库、组件 manifest 和静态文件服务应用相同规则；目录穿越和点文件都返回 404 或不出现在列表。**
- [ ] **Step 5: 在前端 `renderBin()`、`renderLibrary()` 和组件列表增加同名防御过滤，防止旧服务或缓存数据重新显示点文件。**
- [ ] **Step 6: 运行 `node --check server.js && node --test test/resource-filter.test.js`，确认通过。**
- [ ] **Step 7: 提交 `git add third_party/FableCut/server.js third_party/FableCut/paths.js third_party/FableCut/app.js third_party/FableCut/test/resource-filter.test.js && git commit -m "fix: hide macOS dot files from resource lists"`。**

### Task 3: 双语与资源过滤验收

**Files:**
- Modify: `third_party/FableCut/test/user-components.test.js`
- Modify: `docs/operation-log/2026-09-01-agent-reach-install.md`

- [ ] **Step 1: 在测试夹具中加入 `.DS_Store`、`._sample.svg` 和正常素材，验证三个接口均只返回正常素材。**
- [ ] **Step 2: 运行 `node --check server.js && node --check app.js && node --check mcp-server.js && npm test`。**
- [ ] **Step 3: 在浏览器中验证中文默认界面、英文切换、下拉列表文案、属性栏、导出弹窗和点文件过滤。**
- [ ] **Step 4: 确认 Adjust 和画面调节控件仍存在并可用。**
- [ ] **Step 5: 将命令、结果、已过滤文件类型和未验证项追加到操作记录。**
- [ ] **Step 6: 仅当双语文案、下拉列表、资源过滤和既有调节功能全部通过时，记录本阶段完成。**

