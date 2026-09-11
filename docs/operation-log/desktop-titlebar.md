# 桌面标题栏改造记录

## 2026-09-11

- 目的：修复 macOS 原生标题栏出现第二行、项目状态未居中及 L 布局按钮选中态丢失。
- 涉及文件：`src/desktop/src/native_titlebar_macos.mm`。
- 结果：移除 `NSTitlebarAccessoryViewController`，改为直接在系统标题栏容器内布局状态文本与右侧按钮；L 按钮显式使用橙色边框、背景和文字。
- 验证：`cmake --build build --target edward_app -j4` 通过；已启动 `build/bin/Edward.app`。
