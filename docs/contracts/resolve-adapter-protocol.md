# Resolve Studio 适配器协议

Edward 以独立外部应用运行，通过本地桥接 URL 与已启动的 Resolve Studio 通信。协议采用逐行 JSON-RPC 请求/响应；每个请求包含数值 `id`、字符串 `method` 和对象 `params`，响应必须回传相同 `id`。

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
