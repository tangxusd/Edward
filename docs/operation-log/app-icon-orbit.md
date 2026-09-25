# Orbit 应用图标

## 2026-09-25（应用图标接入）

- 目的：将 `/Volumes/file/Codex/Edward界面设计/orbit-logo.svg` 设为 macOS 桌面应用图标，使 Finder 和 Dock 使用同一图标资源。
- 涉及文件：`src/desktop/resources/orbit-logo.svg`、`src/desktop/resources/Orbit.icns`、`src/desktop/CMakeLists.txt`。
- 实现：保留 SVG 作为设计源文件，生成 macOS `icns` 资源，并通过 `MACOSX_BUNDLE_ICON_FILE` 写入应用 Bundle 的 `Info.plist`。
- 验证：`cmake --build --preset macos-debug --target edward_app -j2` 通过；`Edward.app/Contents/Resources/Orbit.icns` 存在；`CFBundleIconFile` 为 `Orbit.icns`。
- 说明：Dock 图标的排列位置由 macOS 用户 Dock 设置管理，本次只更新应用图标资源，不改动 Dock 排列。
