# FableCut 左栏导航标签

## 2026-09-12

- 目的：将九个导航 Tab 改为横向排列，并支持超出宽度时左右移动。
- 涉及文件：`third_party/FableCut/index.html`、`third_party/FableCut/style.css`、`third_party/FableCut/app.js`。
- 结果：九项 Tab 单行横向显示；最右侧提供左右移动按钮，按内容溢出状态自动禁用/启用；保留激活态和原有数据切换逻辑。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务可访问；已启动 Qt 应用。

## 2026-09-12（导入 Tab）

- 目的：移除原资源栏标题，将导入操作集中到“导入”Tab。
- 结果：原资源栏顶部已移除；首页改名为“导入”；标题、组件、调整层、导入、URL 操作移入导入 Tab；九个 Tab 图标与文字上下居中，间距收拢。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。
