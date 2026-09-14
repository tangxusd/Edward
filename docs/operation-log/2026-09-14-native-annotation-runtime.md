# 原生标注组件测试管道

## 2026-09-14

- 目的：验证 React、GSAP、HTML/CSS、SVG 组件在 FableCut 中的时间线插入、属性编辑、预览与逐帧导出入口。
- 涉及：`third_party/FableCut/components/annotation.rect.*`、`component-runtime.js`、`app.js`、本地 `vendor` 浏览器运行时。
- 结果：四个组件已使用各自原生入口和统一 `mount/update/destroy` 合同；标注 tab 从 `/api/components` 读取并可插入；manifest 参数已绑定属性检查器；预览与导出调用同一直接组件运行时。
- 验证：`third_party/FableCut` 的 `npm test` 通过 65/65；服务端实测列出四个 annotation 组件并读取 SVG manifest。
- 未覆盖：当前环境未进行人工浏览器画面验收，需在 Edward 内打开“标注”tab并逐一拖入时间线确认视觉效果。

## 2026-09-14（动画与变换修正）

- 动画统一为单方向描边闭合：0.5 秒内使用三次方缓出，组件默认时长 3 秒，后续时间保持闭合状态。
- 组件中心改为按输出视口的归一化 `x/y` 计算，默认值为 `(0.5, 0.5)`；缩放同时作用于包围盒和原生绘制尺寸。
- 拖拽改为写入归一化坐标，缩放继续写入 `scale`，指针移动期间立即重绘组件层，避免 DOM 预览残留或消失。
- 导出逐帧调用同一组件入口的 `export` 模式，并传入输出视口；已完成静态契约和测试自检，未替代人工画面验收。
