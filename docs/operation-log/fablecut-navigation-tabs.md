# FableCut 左栏导航标签

## 2026-09-12

- 目的：将九个导航 Tab 改为横向排列，并支持超出宽度时左右移动。
- 涉及文件：`third_party/FableCut/index.html`、`third_party/FableCut/style.css`、`third_party/FableCut/app.js`。
- 结果：九项 Tab 单行横向显示；最右侧提供左右移动按钮，按内容溢出状态自动禁用/启用；保留激活态和原有数据切换逻辑。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务可访问；已启动 Qt 应用。
