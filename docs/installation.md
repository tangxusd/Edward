# 安装

运行 `pnpm --filter desktop package` 生成当前平台的安装包。macOS ARM64 产物为 DMG 与 ZIP，Windows 目标为 NSIS 与 ZIP。

首次打开应用后设置项目工作目录。项目、资源库、转写缓存和导出文件均保存在该目录中。

当前构建未签名或公证。发布前必须配置 Apple Developer ID、macOS 公证和 Windows 代码签名证书。
