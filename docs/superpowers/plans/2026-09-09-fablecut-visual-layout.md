# FableCut 0.6.0 视觉布局实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**目标：** 在不改变 FableCut 功能和数据结构的前提下，将右侧属性栏延伸至窗口底部，并让时间线只横跨左侧资源区和中间预览区。

**架构：** 保留现有 `.app`、`.upper`、`.timeline-panel` 和三面板 DOM，优先通过 CSS Grid/Flex 调整几何关系；只在必要处调整 HTML 包裹关系。属性栏独立滚动，资源区和预览区保持原有滚动行为。

**技术栈：** HTML、CSS、原生 JavaScript、Node test runner、Chromium 浏览器验收。

## 约束

- 属性栏宽度沿用当前值，不改变宽度。
- 右侧属性栏从窗口内容区顶部延伸至底部。
- 时间线横跨左侧资源区和中间预览区，不进入右侧属性栏。
- 保留中文/英文切换、Adjust、画面调节、属性双击回退和导出功能。
- 不引入新依赖，不修改时间线数据模型。

### Task 1：建立布局回归测试

**文件：**
- 新建：`third_party/FableCut/test/visual-layout.test.js`
- 修改：`third_party/FableCut/index.html`（仅必要结构标记）

- [ ] 写测试检查资源区、预览区、属性栏和时间线的现有 ID/class 存在。
- [ ] 写测试检查属性栏和时间线的布局 CSS 选择器已定义。
- [ ] 运行测试，确认新断言先失败。

### Task 2：调整页面布局 CSS

**文件：**
- 修改：`third_party/FableCut/style.css`

- [ ] 将主内容布局定义为左资源区、中间编辑区、右属性栏三列。
- [ ] 让中间编辑区内部以节目监视器和时间线两行布局。
- [ ] 让时间线跨越左资源区和中间编辑区的列范围。
- [ ] 让属性栏占满主内容区高度并设置 `overflow-y: auto`。
- [ ] 保持属性栏现有宽度变量和现有控件样式。
- [ ] 增加窗口窄宽度下的最小尺寸和滚动约束，避免重叠。

### Task 3：浏览器视觉验收

**文件：**
- 修改：`third_party/FableCut/test/visual-layout.test.js`

- [ ] 运行 `node --check app.js && node --check server.js`。
- [ ] 运行 `npm test`，确认完整测试通过。
- [ ] 在 `http://localhost:7777/` 验证属性栏顶部到底部、时间线横跨范围、属性栏独立滚动。
- [ ] 验证语言切换、Adjust、画面调节和属性双击回退没有消失。
- [ ] 保存桌面和窄窗口截图作为验收证据。

### Task 4：提交

- [ ] 检查 diff 只包含布局文件和测试文件。
- [ ] 提交 `git commit -m "feat: apply Edward visual panel layout"`。
