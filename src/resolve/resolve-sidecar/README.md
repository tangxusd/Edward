# Resolve Studio 启动探针

`probe_resolve.py` 是 Edward 正式桥接层的只读启动探针。它不安装脚本、不启动 Resolve、不会输出 Bridge token，也不会修改用户目录。

```sh
python3 src/resolve/resolve-sidecar/probe_resolve.py --json
```

连接优先级固定为：

1. `DaVinciResolveScript` 官方 Studio 外部脚本模块；
2. 上游 `davinci-resolve-mcp` 认证回环 Bridge 配置。

Bridge 配置文件默认位于 `~/.config/davinci-resolve-mcp/bridge.json`，只显示是否存在和是否合法，不显示其中的 token。真实连接仍需在 Resolve 中启动 `Workspace > Scripts > resolve_bridge`，并由后续适配器调用。
