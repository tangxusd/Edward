# Edward Web Runtime Host 合同

组件包通过 `edward.web-runtime.v1` 清单接入官方 Web Runtime Host。预览和渲染都直接加载组件包提供的原生 Web 入口，使用同一份 `props` 与帧号；不经过 Component IR、理解层或转换层。

## 清单

清单必须严格符合 [`edward-runtime-host.schema.json`](edward-runtime-host.schema.json)，不得出现额外字段。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `protocol` | string | 固定为 `edward.web-runtime.v1` |
| `runtime` | string | `react`、`html-css`、`svg`、`gsap` 之一 |
| `width` / `height` | integer | 大于 0，组件画布像素尺寸 |
| `fps` | number | 大于 0 |
| `durationInFrames` | integer | 大于 0 |
| `previewEntry` | string | 组件包内相对入口 |
| `renderEntry` | string | 组件包内相对入口 |
| `propsSchema` | object | 组件 props 的 JSON Schema |
| `editableProperties` | string[] | 可编辑 props 名称，不得为空且不得重复 |
| `editableTracks` | string[] | 可创建关键帧的实例或组件属性轨道，不得出现未知路径 |
| `capabilities` | object | `preview`、`export`、`editable`、`audio`、`transparent` 五个布尔值；预览和导出必须为 true |

入口路径只能使用 `/` 或 `\\` 分隔的相对路径；绝对路径、盘符、URI、空路径以及 `.`/`..` 路径段都被拒绝。未列入合同的命令、进程、脚本路径、素材路径或 Component IR 字段也被拒绝。

宿主使用 `previewEntry` 建立预览，使用 `renderEntry` 执行离屏渲染。入口加载失败必须报告结构化错误，禁止回退到旧 IR 或素材层。

## 原生模块接口

两个入口都必须是组件包内的浏览器 ES 模块，并导出
`mount({ host, props, time, mode, viewport })`。`mount` 可返回带有
`update(props, time, viewport, mode)` 与 `destroy()` 的实例。宿主切换到导出
时重新加载 `renderEntry`；预览与导出使用同一片段 props 和帧号。属性检查器
直接读取 `propsSchema` 与 `editableProperties`，不构造组件中间表示。

组件作者帧率与总帧数由 `fps` 和 `durationInFrames` 定义。宿主把项目时间映射为最近的作者整数帧；拉伸、循环与反向播放只在组件清单明确声明时允许。实例只保存锁定版本、内容哈希、允许的 props 和 `editableTracks` 中的关键帧覆盖；升级必须由目标包提供的原生迁移入口完成，失败时保留原实例。

## 画布坐标合同

组件 props 中的位置字段统一采用 Edward 画布中心坐标：`(0, 0)` 位于画布中心，`x` 向右为正、向左为负，`y` 向上为正、向下为负。宿主将预览 CSS/DOM 坐标、Canvas 像素坐标和导出像素坐标转换到该合同后，才传入或读取组件。组件不得自行把左上角设为原点，也不得把正 Y 定义为向下。预览和 `renderEntry` 必须对同一 props 在不同输出分辨率下保持相同相对位置。

## 产品边界

Edward 当前只以本地 Web 宿主运行、预览和导出 React、HTML/CSS、GSAP、SVG
组件。Resolve、Fusion、Premiere 和 OpenShot 不属于产品运行或导出路径；宿主
不得启动、连接、轮询或依赖这些外部程序。
