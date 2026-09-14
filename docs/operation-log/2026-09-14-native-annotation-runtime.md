# 原生标注组件测试管道

## 2026-09-14

- 目的：验证 React、GSAP、HTML/CSS、SVG 组件在 FableCut 中的时间线插入、属性编辑、预览与逐帧导出入口。
- 涉及：`third_party/FableCut/components/annotation.rect.*`、`component-runtime.js`、`app.js`、本地 `vendor` 浏览器运行时。
- 结果：四个组件已使用各自原生入口和统一 `mount/update/destroy` 合同；标注 tab 从 `/api/components` 读取并可插入；manifest 参数已绑定属性检查器；预览与导出调用同一直接组件运行时。
- 验证：`third_party/FableCut` 的 `npm test` 通过 65/65；服务端实测列出四个 annotation 组件并读取 SVG manifest。
- 未覆盖：当前环境未进行人工浏览器画面验收，需在 Edward 内打开“标注”tab并逐一拖入时间线确认视觉效果。
