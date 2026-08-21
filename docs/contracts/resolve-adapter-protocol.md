# Resolve Studio 适配器协议

Edward 以独立外部应用运行，正式连接层不再自定义一套与 Resolve 生态无关的明文 TCP 协议。正式实现直接采用已在 `samuelgursky/davinci-resolve-mcp` 中验证的两级连接策略：Resolve Studio 官方外部脚本直连为主，Resolve 内部认证回环 Bridge 为备用。Edward 的 `ResolveAdapter` 只依赖统一的能力/时间线/组件/渲染语义，不感知底层是直连还是 Bridge。

参考实现：

- [DaVinci Resolve MCP README](https://github.com/samuelgursky/davinci-resolve-mcp/blob/main/README.md)
- [Resolve 外部连接选择](https://github.com/samuelgursky/davinci-resolve-mcp/blob/main/src/utils/resolve_connection.py)
- [认证 Bridge 客户端](https://github.com/samuelgursky/davinci-resolve-mcp/blob/main/src/utils/resolve_bridge_client.py)
- [Resolve 内部 Bridge](https://github.com/samuelgursky/davinci-resolve-mcp/blob/main/src/utils/resolve_bridge.py)

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
