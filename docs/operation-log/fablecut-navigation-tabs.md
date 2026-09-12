# FableCut 左栏导航标签

## 2026-09-12

- 目的：用图片 1 的左栏九项导航替换原中部四个资源标签。
- 涉及文件：`third_party/FableCut/index.html`、`third_party/FableCut/style.css`、`third_party/FableCut/app.js`。
- 结果：新增首页、媒体、文本、音频、卡片、图表、背景、标注、数字九个纵向标签；项目素材与资源库分流保持可用，原有媒体/元素/音效/SVG 数据源按标签映射复用。
- 验证：`node --check third_party/FableCut/app.js` 通过；FableCut 服务可访问；已启动 Qt 应用。
