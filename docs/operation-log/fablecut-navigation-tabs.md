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

## 2026-09-12（图标边框与箭头对齐）

- 结果：移除所有 Tab 图标容器外框；导入 SVG 蓝色路径改为灰色，底部橙色路径保留；左右按钮底部对齐并下移。
- 验证：服务与 Qt 应用已重启。

## 2026-09-12（Tab 与监视器紧凑化）

- 结果：Tab 宽度与间距收拢；素材内容改为卡片网格；节目监视器右侧按钮、下拉框和文字统一为 11px/24px 紧凑尺寸。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（导航 SVG 规范）

- 结果：九个导航 Tab SVG 图标线宽统一为 1px，尺寸缩小 20%。
- 验证：服务与 Qt 应用已重启。

## 2026-09-12（导入 Tab 内嵌与箭头定位）

- 结果：导入按钮改为直接嵌入“导入”Tab；其他图标容器去除外框；左右移动按钮固定在 Tab 文字区域左右角。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（导入图标与移动按钮）

- 结果：导入按钮仅在导入 Tab 的操作区显示；导入图标替换为附件提供的双色 SVG 且无外框；左右移动按钮移至 Tab 行下方并保持可见。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（原始 SVG 与按钮位置）

- 结果：导入按钮仅在“导入”Tab 显示；导航图标按附件 HTML 的 SVG 风格重建；左右移动按钮缩小并移至 Tab 行下方。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（导入显示与图标尺寸）

- 结果：导入按钮仅在“导入”Tab 显示；九个图标放大一倍；左右移动按钮缩小并固定在右侧；Qt 初始窗口尺寸改为正常值，消除启动时 1px 放大闪烁。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（滚动与导入收敛）

- 结果：仅保留“导入”操作按钮并固定在 Tab 顶部；图标/文字恢复上下布局；左右按钮改为固定控制位，通过平移 Tab 内容工作，不再随内容移动或消失。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。
