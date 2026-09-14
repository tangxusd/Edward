# Edward 原生标注组件测试管道设计

- 日期：2026-09-14
- 状态：已确认设计，待实现计划
- 范围：本地标注资源测试；不包含云端管理员发布、支付、Resolve、Premiere 或 OpenShot 集成。

## 目标

在 Edward/FableCut 中加入四个本地测试资源，分别使用 React、GSAP、HTML/CSS 和 SVG 原生运行时绘制同一个横向无填充圆角矩形：蓝色边框、边框粗细 2px、圆角 5px，边框从未闭合逐渐闭合。四个资源必须能够：

1. 出现在“标注”tab；
2. 插入时间线并保存起止时间与参数；
3. 在预览中按播放头时间显示；
4. 在 Edward 属性检查器中修改原生参数；
5. 使用同一时间基准逐帧导出，导出结果与预览一致。

## 禁止事项

- 不使用 Component IR、理解层、转换层或任何等价中间表示。
- 不使用 OpenShot、DaVinci Resolve Studio、Premiere 及其 API、插件、脚本或通信方式。
- 不把组件预先栅格化为视频或图片序列作为组件的唯一形态；这样会丢失参数编辑能力。
- 不用墙钟时间驱动动画。预览和导出都必须传入时间线局部时间。

## 方案

采用“原生组件模块直连”方案。每个资源保留自己的源代码与运行时，时间线只保存资源引用和原生参数快照。组件运行时合同为：

```text
mount({ host, props, time, mode }) -> instance
update(instance, { props, time, mode })
unmount(instance)
```

其中 `time` 是该片段内的秒数，`mode` 为 `preview` 或 `export`。组件可以使用原生 DOM、SVG、React 或 GSAP API，但不能要求宿主先把源代码转换成其他格式。

## 本地资源

资源放在 FableCut 的本地标注资源目录，并由现有“标注”tab 的本地库加载：

| 资源 ID | 原生运行时 | 入口 | 验证重点 |
|---|---|---|---|
| `annotation.rect.react` | React + ReactDOM | JSX/React 组件入口 | React 挂载、更新和卸载；本地运行时可离线加载 |
| `annotation.rect.gsap` | GSAP | GSAP 时间轴入口 | 按局部时间 seek，不依赖 `requestAnimationFrame` 墙钟时间 |
| `annotation.rect.html-css` | HTML/CSS | DOM 组件入口 | CSS 样式和过渡由原生 DOM 保留 |
| `annotation.rect.svg` | SVG | SVG 组件入口 | `stroke-dasharray/stroke-dashoffset` 直接控制闭合进度 |

React/ReactDOM 和 GSAP 运行时作为本地测试依赖保存，禁止通过公网 CDN 加载。组件 manifest 只声明入口、运行时和参数字段，不承载转换规则。

## 原生参数

四个资源声明同一组可编辑参数，但各资源直接消费自己的参数对象：

```json
{
  "x": 0.5,
  "y": 0.5,
  "width": 0.56,
  "height": 0.28,
  "borderWidth": 2,
  "radius": 5,
  "color": "#1683ff",
  "progress": 0,
  "duration": 2
}
```

`progress` 为 0 到 1 的闭合比例；运行时根据 `time` 与 `duration` 得出当前视觉状态。属性检查器只绑定 manifest 声明的字段，编辑后直接调用组件的 `update`，不做字段猜测、格式改写或跨格式转换。

## 时间线数据

时间线片段保存以下事实数据，不生成统一组件 IR：

```json
{
  "id": "clip-...",
  "kind": "native-component",
  "resourceId": "annotation.rect.svg",
  "runtime": "svg",
  "source": "components/annotation/rect-svg/component.js",
  "start": 0,
  "duration": 2,
  "props": { "borderWidth": 2, "radius": 5, "color": "#1683ff" }
}
```

`runtime` 仅用于选择该资源自身的入口，不承担不同格式之间的转换。资源入口必须从 `source` 读取，不能由宿主按 `runtime` 猜测代码。

## 预览与导出流程

1. 播放头改变时，时间线计算片段局部时间；未进入片段范围时调用 `unmount` 或隐藏该片段宿主。
2. 进入范围后，运行时只收到自己的 `props` 和局部 `time`，直接更新 DOM、SVG、React 树或 GSAP 时间轴。
3. 导出逐帧使用与预览相同的局部时间和参数快照，设置 `mode: export`，等待组件更新完成后捕获当前画面。
4. 导出完成后销毁导出实例，不污染预览实例。
5. 四个资源均不得依赖实时计时器、随机值或未固定的外部网络资源。

## 管理员测试入口

本阶段不接云端发布。管理员测试入口只负责：

- 从本地标注资源目录加载四个资源；
- 将选中的资源插入当前播放头；
- 显示并编辑资源声明的参数；
- 触发预览和导出验收。

云端管理员发布、权限校验、资源版本、预览图上传和审核流程另立设计，不与本次本地测试混合。

## 失败边界

- 依赖文件缺失、manifest 无效或入口不存在：资源不可插入，并显示确定性错误。
- 组件更新抛错或未在帧截止前完成：该帧导出失败，不使用上一帧残留画面。
- 参数超出 manifest 范围：拒绝写入并保留上一份有效参数。
- 预览和导出输出尺寸不一致：阻断导出并记录宽高、DPR、局部时间和资源 ID。
- React/GSAP 本地运行时不可加载：对应资源显示不可用，其他三种资源不受影响。

## 验证计划

### 静态验证

- 四个 manifest 均声明正确运行时、入口和参数字段。
- 入口文件不包含 Component IR、Resolve、Premiere 或 OpenShot 引用。
- React/ReactDOM/GSAP 依赖只从项目内本地路径加载。

### 行为验证

- 四种资源均能在“标注”tab显示并插入时间线。
- 播放头位于片段外时画面为空，不残留上一个组件。
- `t=0`、`t=duration/2`、`t=duration` 三个采样点的闭合比例分别正确。
- 修改位置、尺寸、边框、圆角和颜色后，预览立即更新，导出使用相同值。
- 预览帧与导出帧在相同时间点的像素尺寸和组件边界一致。
- 四种资源均通过 24/30/60 fps 的时间采样，动画时长不随导出速度改变。

## 不在本次范围

- 云端资源库管理员发布管道；
- 公开资源审核和版本回滚；
- React/GSAP/HTML/CSS/SVG 之间的互转；
- 任意第三方 NLE 的投放或渲染；
- 复杂嵌套组件、外部字体、网络素材和交互触发器。
