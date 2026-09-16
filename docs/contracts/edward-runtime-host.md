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

入口路径只能使用 `/` 或 `\\` 分隔的相对路径；绝对路径、盘符、URI、空路径以及 `.`/`..` 路径段都被拒绝。未列入合同的命令、进程、脚本路径、素材路径或 Component IR 字段也被拒绝。

宿主使用 `previewEntry` 建立预览，使用 `renderEntry` 执行离屏渲染。入口加载失败必须报告结构化错误，禁止回退到旧 IR 或素材层。
