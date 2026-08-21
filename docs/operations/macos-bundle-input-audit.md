# macOS `.app` 发布输入审计

更新时间：2026-08-21

本文件记录当前开发机的实际动态依赖与发布前必须取得的材料。它不是许可证替代物，也不表示可以发布。

## 已确认的本机构建输入

| 依赖 | 当前版本或构建事实 | 当前许可事实 | 发布包需要的动作 |
| --- | --- | --- | --- |
| Qt | 6.11.1 | Homebrew 元数据包含 LGPL-3.0-only 等多许可项 | 使用动态 Qt 框架；随包放入适用许可证、第三方通知与可替换/重链接说明。 |
| FFmpeg | 8.1.2；`--enable-gpl --enable-version3` | GPL-3.0-or-later | 随包放入精确源码、构建配置、GPL 文本与第三方通知；不得把它当作 LGPL 构建。 |
| MLT | 7.40.0 | LGPL-2.1-only | 随包放入适用许可证与通知。 |
| libsodium | 1.0.22 | ISC | 随包放入许可证与通知。 |
| SDL3 | 3.4.14 | Zlib | 随包放入许可证与通知。 |

## 当前 `.app` 范围

开发构建会生成 `Edward.app`，包括 `Contents/Info.plist` 和主可执行文件。它可用于本机启动验证，但当前没有完成以下动作：

1. 用 `macdeployqt` 复制并修正 Qt 运行时；
2. 收集 FFmpeg、MLT、SDL3、libsodium 及其递归动态依赖；
3. 收集 MLT 模块并在应用内设置模块搜索路径；
4. 将本表所列许可证、精确构建配置、第三方通知和基础资源加入 `Contents/Resources`；
5. 对嵌入的二进制做依赖路径检查；
6. 代码签名、DMG 制作与安装验证。

因此当前 `.app` 不是分发候选物。后续发布打包只能在上述输入齐备后开始，不能用空白许可证、通用许可证文本或开发机绝对路径替代。

MLT 模块不是单个动态库：当前本机还需要 `lib/mlt` 下的模块和 `share/mlt` 下的数据、预设、profiles 等目录。正式打包入口已将这两类输入设为必填。

字体也必须同时提供字体文件和 `font-manifest.json`（许可证、版本、SHA-256）；旧 worktree 或历史构建目录中的字体不属于当前分支发布输入，不能直接复用。

## 可复核命令

```sh
qmake -query QT_VERSION
ffmpeg -version
pkg-config --modversion libavformat libsodium sdl3
brew info --json=v2 qt ffmpeg mlt libsodium sdl3
```
