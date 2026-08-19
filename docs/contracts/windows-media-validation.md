# Edward Windows 媒体验证合同

## 目的

Windows 构建和发布验证必须在 Windows x64 的同一 MSVC 媒体前缀内完成。macOS 构建、CTest 和导出结果不能代替该验证。

## 前提

- 编译器为同一版本的 MSVC x64 工具链。
- Qt、FFmpeg、MLT 及其运行时 DLL 均来自同一 x64 前缀；不得混入 MinGW 产物或系统中未知来源的 DLL。
- 使用与 Edward 链接配置一致的 FFmpeg、MLT 版本和许可构建选项。
- 运行测试和应用时，`PATH` 仅加入该前缀的 Qt、FFmpeg、MLT DLL 目录。

## 必做验证

1. 配置并构建 `edward_app` 与全部测试目标。
2. 运行完整 CTest，包含 `e2e.edward_0_3_0_smoke`。
3. 用端到端测试夹具导出 1920×1080 H.264 文件，并用 FFprobe 验证尺寸、帧数和可播放性。
4. 在无开发环境的干净 Windows 虚拟机中启动应用，确认 QML、MLT producer 和 FFmpeg 可执行文件均可加载。
5. 记录 MSVC、Qt、FFmpeg、MLT 版本、依赖前缀位置和测试输出到发布操作日志。

## 通过条件

仅当以上五项均在同一 Windows 前缀中通过，才可把 Windows 媒体运行时标记为已验证。MSI 打包验证属于其后的独立发布门。
