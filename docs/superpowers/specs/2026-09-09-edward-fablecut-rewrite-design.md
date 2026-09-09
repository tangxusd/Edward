# Edward 基于 FableCut 的桌面重写设计

## 目标

将 Edward 重写为基于 Qt WebEngine + FableCut 的跨平台桌面视频编辑器。FableCut 的 Chromium 页面作为唯一视觉渲染真源，直接运行 React、CSS、SVG、GSAP 以及 MP4/MOV；Edward Qt 主进程只负责窗口、进程、项目目录、系统集成和打包。

## 已确认的硬约束

- 完全移除 Resolve、Fusion、Component IR、MLT 作为运行时依赖。
- 不解析、转换或编译 React/CSS/SVG/GSAP 用户代码。
- 不建立第二套原生视觉渲染器。
- 预览和导出必须使用同一个 Chromium scene root。
- 不允许导出静默丢失组件或素材图层。
- macOS 和 Windows 保留系统原生标题栏与窗口控制。

## 总体架构

```text
Edward Qt 主进程
 ├─ Qt WebEngineView
 │   └─ FableCut 页面
 │       ├─ 时间线与项目状态
 │       ├─ React/CSS/SVG/GSAP 原生运行
 │       ├─ MP4/MOV 原生视频
 │       └─ 统一 scene root
 ├─ 本地 Node 服务
 │   ├─ project.json
 │   ├─ 素材与组件目录
 │   ├─ 属性栏接口
 │   └─ 导出帧接口
 └─ FFmpeg
     └─ 编码最终捕获帧
```

Chromium 是唯一视觉渲染真源。Edward C++ 不参与用户代码理解、属性补全、IR 生成或视觉转换。Node 服务只负责项目数据、资源服务和本地通信，不监听公网地址。

## 时间线与场景模型

项目时间线以整数帧和项目帧率为基础。clip 只保存起始帧、持续帧数、轨道、媒体或组件引用以及原始公开属性；不保存 React 树、CSS 计算值或 GSAP 中间状态。

每个当前帧统一调用 `setSceneTime(frame)`：视频元素定位到对应媒体时间，GSAP 实例直接 seek，React 组件收到原始时间，CSS/SVG 保留浏览器动画。所有活动层挂在同一个 scene root，交错关系由 DOM 顺序、z-index 和浏览器 stacking context 决定。

## 导出链路

导出逐帧设置 scene time，等待 React commit、GSAP 更新、字体、图片和视频帧稳定后，捕获同一个 scene root 的完整画面，再把帧交给 FFmpeg 编码。预览和导出不允许维护两套绘制实现。

捕获失败、组件异常、素材未加载或字体不可用时必须终止导出并报告原因，禁止输出缺层文件。捕获接口需要在 FableCut 页面与 Qt WebEngine 宿主之间定义明确的帧格式、尺寸、帧号和错误合同。

## Qt WebEngine 桌面封装

- Edward 启动并监控本地 Node 服务，服务仅绑定本机回环地址。
- 每个项目使用独立数据目录，资源访问限制在项目目录内。
- 应用只保留一个主 WebEngine 页面，减少 Chromium 重复进程和内存。
- 非活动 clip 暂停动画和视频解码；导出时锁定帧率、时间和资源并发。
- Node 或 WebEngine 崩溃时自动恢复，保留最近一次原子写入的项目快照。
- Windows/macOS 固定打包 Qt WebEngine、Node 运行时和 FFmpeg 版本，避免用户环境差异。
- macOS 使用系统原生红黄绿按钮；Windows 使用系统原生最小化、最大化和关闭按钮；应用页面不模拟标题栏。

## 实施阶段

1. 清理 Edward 运行时入口，建立 Qt WebEngine 启动 FableCut 的最小壳。
2. 将 FableCut 项目、素材、组件和本地服务纳入 Edward 数据目录与生命周期管理。
3. 重写 FableCut scene root 和统一帧时钟，覆盖视频、React、CSS、SVG、GSAP 的交错叠加。
4. 在 Qt WebEngine 中实现同场景逐帧捕获并接入 FFmpeg，替换当前 Canvas-only 导出。
5. 增加 Windows/macOS 打包、崩溃恢复、资源限制和端到端验收。

## 验收标准

- 任意浏览器可执行的 React/JavaScript、CSS、SVG、GSAP 组件可在预览中直接运行。
- MP4/MOV、组件和 SVG/CSS/GSAP 层可交错叠加，层级与预览一致。
- 属性栏修改在当前帧实时反映到预览并保存到项目。
- 导出逐帧包含 scene root 的所有活动层，结果不缺组件。
- 导出帧率、时长、音画同步和画面尺寸正确。
- 组件异常只隔离该实例，不使 Edward 主进程退出。
- 无 Resolve、Fusion、Component IR、MLT 运行时依赖。

## 明确风险

任意 DOM/CSS 图层的高质量导出依赖 Qt WebEngine 的实际画面捕获能力。该能力必须在当前 Qt 版本和 Windows/macOS 实机上先做最小验证；验证未通过前，不得声称导出完成，也不得退回 Canvas-only 导出。
