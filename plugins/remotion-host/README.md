# Edward Remotion adapter

此目录是独立插件，不随 Edward 主程序加载。使用 Node 22 或更高版本，在本目录执行 `npm install`，随后在 Edward 的插件设置中选择本目录。Edward 通过 `edward-plugin.json` 启动 `host.mjs`，只接收标准 Component IR 描述或透明 PNG 帧。

Remotion 运行时依赖其自身的浏览器渲染环境；Edward 的工程、时间线和最终合成不依赖其作为唯一数据源。
