# Resolve Studio 适配器协议

Edward 以独立外部应用运行，正式连接层不再自定义一套与 Resolve 生态无关的明文 TCP 协议。正式实现直接采用已在 `samuelgursky/davinci-resolve-mcp` 中验证的两级连接策略：Resolve Studio 官方外部脚本直连为主，Resolve 内部认证回环 Bridge 为备用。Edward 的 `ResolveAdapter` 只依赖统一的能力/时间线/组件/渲染语义，不感知底层是直连还是 Bridge。

参考实现：

- [DaVinci Resolve MCP README](https://github.com/samuelgursky/davinci-resolve-mcp/blob/main/README.md)
- [Resolve 外部连接选择](https://github.com/samuelgursky/davinci-resolve-mcp/blob/main/src/utils/resolve_connection.py)
- [认证 Bridge 客户端](https://github.com/samuelgursky/davinci-resolve-mcp/blob/main/src/utils/resolve_bridge_client.py)
- [Resolve 内部 Bridge](https://github.com/samuelgursky/davinci-resolve-mcp/blob/main/src/utils/resolve_bridge.py)

当前固定参考版本：`davinci-resolve-mcp` `v2.98.3`，Git commit
`132e134d3aa25d3d0df6bdf38f051bd29d128211`，许可证 MIT。升级该依赖必须重新运行真实 Resolve Studio 集成门，不能只更新版本号。

Edward 的只读启动探针位于 `src/resolve/resolve-sidecar/probe_resolve.py`。它只报告官方模块和认证 Bridge 配置是否可用，不会安装脚本或打印 token；适配器正式接入前，先用该探针确认环境，避免把“Resolve 已打开”误判为“Edward 已连接”。

官方直连侧车 `src/resolve/resolve-sidecar/resolve_direct_sidecar.py` 使用逐行 JSON 输入输出，当前提供 `health`、`capabilities` 和 `timeline.snapshot` 只读操作。它是 C++ `ResolveAdapter` 迁移到官方 API 的中间边界，不是新的产品级网络协议。

上游对 Resolve 21 的真实 API 探针确认：字幕轨道可以通过 `AddTrack("subtitle")` 创建，但脚本 API 没有稳定的单条字幕文本/时间范围写入或 SRT 导入方法。因此 `subtitle.insert` 使用文档化的 `InsertFusionTitleIntoTimeline("Text+")` 作为真实回退，只写入标准文本属性 `StyledText`；时间范围和不支持样式继续由 Edward 转换报告标记。

## 连接优先级

1. Studio 外部脚本 API：使用 Resolve 的 `scriptapp("Resolve")`，要求 Resolve Studio 的 `External scripting using` 设置为 `Local`。
2. 认证回环 Bridge：仅在直连不可用且用户启用 Bridge 时使用；Bridge 必须绑定 `127.0.0.1`，请求使用 HMAC-SHA256、时间戳、一次性 nonce、请求 ID 和能力探测。

当前仓库的 `ResolveConnection`/fixture 仅是迁移前的测试实现，禁止继续作为正式产品协议扩展；在正式桥接替换完成前，不应宣称 Edward 已完成真实 Resolve 连接。

## 能力与时间线

- `capabilities`：返回 `studioVersion`、`timeline`、`fusion`、`render`。
- `timeline.snapshot`：返回当前项目、时间线、播放头、帧率和轨道。
- `timeline.setPlayhead`：设置播放头帧。

## 组件与导出

- `subtitle.insert`：插入标准 Component IR 字幕组件，返回 `componentId`。
- `component.keyframe`：在指定帧写入数值属性关键帧。
- `render.start`：提交 Edward 覆盖的常用渲染参数，返回 `jobId`。
- `render.status`：返回 `queued`、`rendering`、`completed`、`failed` 或 `canceled` 状态及 0-100 进度。
- `render.cancel`：取消指定渲染任务。
- `ui.openDeliver`：打开 Resolve Studio 原生 Deliver 页面，供高级用户使用。

真实 Resolve Studio 测试只在设置 `EDWARD_RESOLVE_E2E=1` 和 `EDWARD_RESOLVE_BRIDGE_URL` 时启用；未启动桥接服务时测试跳过，不把外部应用缺失误报为代码失败。
