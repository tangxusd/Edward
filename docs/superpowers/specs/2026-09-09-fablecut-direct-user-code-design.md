# FableCut 方案 A：用户代码直接运行的最小验证设计

## 目标

在不引入 IR、组件转换器、React 转 SVG 或 GSAP 转关键帧的前提下，让用户提供的 React/GSAP/CSS/SVG 代码直接运行在 FableCut 预览页面中，并完成属性栏修改、交错图层预览和视频导出验证。

## 范围

第一阶段只验证 FableCut 内部链路，不接入 Edward C++/Qt，也不恢复 Resolve。用户代码作为本地项目资产加载；FableCut 负责时间线、图层顺序、属性栏、预览控制和导出。

不包含：远程 URL 组件、iframe 多进程隔离、自动安装任意 npm 依赖、用户代码静态语义分析、将 React/GSAP/CSS/SVG 转成另一种数据结构。

## 核心架构

```text
用户组件目录
  ├─ manifest.json       运行入口和属性栏公开字段
  ├─ component.jsx       用户原始 React/GSAP 代码
  ├─ component.css       用户原始 CSS
  └─ assets/*            同目录资源
          ↓
FableCut 本地 HTTP 服务
          ↓
同一个 Chromium 预览页面
          ↓
视频、React、SVG、CSS、GSAP 真实 DOM 图层
          ↓
浏览器原生层叠合成
          ↓
实时预览或逐帧截图 → ffmpeg 导出
```

时间线只保存片段起点、时长、层级、组件入口和公开属性值。它不保存 React 树、CSS 计算结果或 GSAP 中间状态。每个片段对应预览页中的真实 DOM/SVG/video 元素；交错叠加由 DOM 顺序、`z-index` 和浏览器原生 stacking context 完成。

## 用户组件协议

`manifest.json` 只描述入口与编辑器公开属性，不是组件 IR：

```json
{
  "name": "MyOverlay",
  "entry": "./component.jsx",
  "style": "./component.css",
  "props": {
    "title": { "type": "string", "default": "Hello" },
    "color": { "type": "color", "default": "#ffffff" },
    "x": { "type": "number", "default": 0, "min": -1920, "max": 1920 }
  }
}
```

组件入口接收 `props` 和受控时间对象。GSAP 必须绑定到组件自己的 timeline，并能由宿主在导出时跳转到指定时间；不得依赖系统时钟、不可控随机数或鼠标状态才能得到可导出的画面。

## 属性栏

属性栏根据 `manifest.json` 展示 string、number、boolean、color、select 等有限控件。修改后只更新该组件实例的 props，并触发该实例重新渲染；不修改组件源文件，不重写 CSS，不生成中间结构。非法值显示错误并保留上一次有效值。

## 时间与预览

- 时间线使用帧号和项目帧率控制宿主时钟。
- 预览时将当前时间传给所有活动实例。
- React 组件保持原始运行时；GSAP 在同一页面中直接 seek；CSS/SVG 使用浏览器动画机制；视频使用原生 `<video>`。
- 所有层位于一个场景根节点中，禁止先分别渲染到多个画布再拼接。
- inactive 图层暂停或隐藏，但不能改变 active 图层的层叠关系。

## 导出

导出沿用同一预览页：对每个目标帧设置时间，等待 React 更新、GSAP seek、视频帧、字体和图片稳定后，截取场景根节点；帧送入 ffmpeg 编码。预览与导出不得使用两套渲染代码。

导出验收至少包含：交错视频/React/SVG/GSAP 层、透明度、transform、CSS filter、blend mode、字体和 30fps 连续帧。导出失败必须返回明确错误，不得静默生成不完整文件。

## 用户代码安全与资源边界

- 组件默认只允许本地项目目录内的模块和资源。
- 不自动执行 shell，不读取项目目录外文件，不默认访问网络。
- 用户代码异常只标记该实例失败并在预览窗显示错误，不得让编辑器进程退出。
- 外部字体和资源必须经过明确配置；导出前检查资源加载完成。

## 验证用例

最小 fixture 必须同时包含：一个 React 组件、一个 GSAP 动画、一个 CSS 动画、一个 SVG 动画、一个原始视频层，并让它们在时间线上交错出现。验收步骤：

1. 本地加载用户组件并显示在预览窗。
2. 属性栏修改文字、颜色、位置和动画参数，预览立即更新。
3. 拖动时间线，所有图层按正确时间和 z-order 合成。
4. 播放预览并导出同一时间范围。
5. 比较导出帧与预览截图，确认层级、动画和属性一致。
6. 让组件抛出异常，确认编辑器仍可继续操作。

## 后续 Edward 集成边界

最小验证通过后，Edward 只包装 FableCut 的本地服务、窗口、数据目录和打包流程。React/GSAP/CSS/SVG 仍由 Chromium 直接运行；不重新引入 Component IR、Fusion 转换器或 Resolve 适配层。
