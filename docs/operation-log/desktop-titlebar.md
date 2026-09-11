# 桌面标题栏改造记录

## 2026-09-11

- 目的：修复 macOS 原生标题栏出现第二行、项目状态未居中及 L 布局按钮选中态丢失。
- 涉及文件：`src/desktop/src/native_titlebar_macos.mm`。
- 结果：移除 `NSTitlebarAccessoryViewController`，改为直接在系统标题栏容器内布局状态文本与右侧按钮；L 按钮显式使用橙色边框、背景和文字。
- 验证：`cmake --build build --target edward_app -j4` 通过；已启动 `build/bin/Edward.app`。

## 2026-09-11（按钮交互与文字）

- 目的：恢复标题栏按钮点击逻辑并统一缩小控件字号/尺寸，项目状态文字改为纯白。
- 涉及文件：`src/desktop/src/native_titlebar_macos.mm`、`src/desktop/qml/Workbench.qml`。
- 结果：原生按钮通过 Qt 元对象调用 QML 统一入口；字号 10px、按钮高度 22px；状态与普通按钮文字纯白，L 选中态保留橙色。
- 验证：`cmake --build build --target edward_app -j4` 通过；已重新启动 `build/bin/Edward.app`。

## 2026-09-11（按钮背景）

- 目的：移除标题栏按钮额外灰色底层。
- 涉及文件：`src/desktop/src/native_titlebar_macos.mm`。
- 结果：按钮图层背景设为透明，保留边框、文字及 L 选中态。
- 验证：目标构建通过并已重新启动应用。

## 2026-09-11（拖拽恢复）

- 目的：恢复扩展客户区下系统标题栏空白区域的窗口拖拽。
- 涉及文件：`src/desktop/src/native_titlebar_macos.mm`。
- 结果：启用 `movableByWindowBackground`，按钮区域仍由原生按钮接收点击。
- 验证：目标构建通过并已重新启动应用。

## 2026-09-11（布局与轨道按钮）

- 目的：去除按钮灰色底层，恢复拖拽，并修正布局/S/M/L 的业务含义。
- 结果：按钮改为无底色 inline 样式；“布局”调用 FableCut 默认布局；S/M/L 调用 `setTrackSize` 改变轨道高度，不再改变窗口宽度。
- 验证：目标构建通过并已重新启动应用。

## 2026-09-11（设置与语言）

- 结果：移除问号按钮；设置按钮直接打开设置对话框；语言按钮默认显示 `ENG`，点击后切换为 `中文`，再次点击恢复 `ENG`。
- 验证：目标构建通过并已重新启动应用。
