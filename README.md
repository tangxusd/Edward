# AI 剪视频工具

跨 macOS 与 Windows 的桌面剪辑工程工具。用户导入一条音频或视频主媒体和文稿，本地转写后使用已配置的云端模型生成可人工调整的多轨时间线。

## 开发

```bash
pnpm install
pnpm dev
```

## 验证

```bash
pnpm --filter @ai-video/domain test
pnpm --filter desktop test
```

## 打包

```bash
pnpm --filter desktop package
```

输出写入 `apps/desktop/release/`。macOS 当前可生成 ARM64 DMG 与 ZIP；发布前仍需配置 macOS 公证和 Windows 代码签名。

更多信息见 [安装说明](docs/installation.md) 与 [运维说明](docs/operations.md)。
