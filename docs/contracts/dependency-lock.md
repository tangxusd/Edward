# Edward 0.2.0 跨平台依赖锁定与发现合同

本合同是实施前的唯一依赖版本与发现策略。任何升级必须先更新本文件、重新构建 macOS/Windows 两端并重新跑全部 CTest；不得只在单端升级。

| 依赖 | 固定来源与版本 | 用途 | 许可证 |
| --- | --- | --- | --- |
| CMake | 最低 3.28.3（本机已核验 4.4.2） | 配置与预设 | BSD-3-Clause |
| Ninja | 最低 1.12.1（本机已核验 1.13.2） | 构建执行器 | Apache-2.0 |
| Qt | `v6.11.1`，commit `20cb2b725f7a7192c7491e395bfb54c22df46688` | QML、QtTest、原生桌面壳 | LGPL-3.0-only/GPL-3.0-only/商业，使用 LGPL 路径 |
| FFmpeg | `n8.1.2`，commit `38b88335f99e76ed89ff3c93f877fdefce736c13` | 探测、解码、代理、软件编码、滤镜 | GPL-2.0-or-later |
| MLT | `v7.40.0`，commit `bef9d89c0c279e558d9625dac3399c2aa3d961bc` | timeline producer/filter/tractor 执行适配 | LGPL-2.1-only |
| nlohmann/json | `v3.12.0`，commit `65ee68451d8eb2b5f3a30b410476ab83deb3289b` | 工程与目录 JSON | MIT |
| whisper.cpp | `v1.9.2`，commit `306c88f4d1286aec1bf96e544632897886af5501`，源码 SHA-256 `a6abd064fcca8b85e794d205abf328c522e9451db43a3eadc178b883b7d0e9cd` | 本地中英文转写与词级/段级时间戳 | MIT |
| libzip | `1.11.4`，官方源码归档 `libzip-1.11.4.tar.xz`，SHA-256 `8a247f57d1e3e6f6d11413b12a6f28a9d388de110adc0ec608d893180ed7097b` | DOCX ZIP 容器只读解析 | BSD-3-Clause |
| pugixml | `v1.16`，commit `c8033ce9d039e7f9d134877c363397b3cfe20816`，源码 SHA-256 `4cee1ca4aad395170f4c7a07824f3bdd41f28316c6e1e1090a1425b278ec0b4b` | DOCX XML 安全解析 | MIT |
| Poppler | `26.08.0`，源码 SHA-256 `dc906e68cea698109706ac6aa3d2c9d4512fcfcac42d90b8afcda486d1b9abd0` | 可复制文本 PDF 提取 | GPL-2.0-only OR GPL-3.0-only |
| libsodium | `1.0.22`，commit `bc5892beb87c388e123baa7c8f4862f30d9206a7`，源码 SHA-256 `adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349` | Ed25519 目录/插件/订阅验签 | ISC |

上表所列 Git tag 与提交在 2026-08-11 通过对应官方 Git 仓库的 `git ls-remote --tags` 核验。macOS 使用 Homebrew FFmpeg 8.1.2 时，必须以 `ffmpeg -buildconf` 核验 GPL、`libx264`、`libx265` 和 VideoToolbox；MLT 必须以 `brew info mlt --json=v2` 核验 7.40.0。Windows 必须核验 tag 解析到同一 commit，并记录 SHA-256；不接受浮动分支、未校验镜像或混合 MinGW/MSVC 产物。

## 统一安装前缀与发现方法

| 平台 | 前缀 | 编译器/架构 | 发现策略 |
| --- | --- | --- | --- |
| macOS Apple Silicon | `/opt/homebrew` | Apple Clang，arm64，最低 macOS 14 | Qt 使用 `Qt6_ROOT` 指向官方 Qt 6.11.1 安装目录；FFmpeg 与 libsodium 使用 `pkg_check_modules` 导入；MLT 使用 `find_package(Mlt7 CONFIG REQUIRED)`；JSON 以 `find_path` 查找 Homebrew 头文件。 |
| Windows 11 x64 | `C:\edward-deps\0.2.0` | MSVC 2022 v143，x64 | Qt 使用 `Qt6_ROOT`；FFmpeg/MLT 由同一 MSVC 前缀下的 `.pc` 文件配合 pkgconf 提供；JSON 由固定 vcpkg baseline 或源码安装配置提供。 |

`CMAKE_FIND_ROOT_PATH` 不用于查找宿主编译工具或替代 `pkg-config`。项目的 `cmake/Dependencies.cmake` 必须：

1. `find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick Test Multimedia Network)`；
2. `find_package(PkgConfig REQUIRED)` 后以 `pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavfilter libavutil libswresample libswscale)`；
3. 以 `find_package(Mlt7 7.40 CONFIG REQUIRED)` 发现 MLT，并链接 `Mlt7::mlt`；如实现采用 C++ API，则链接 `Mlt7::mlt++`。macOS Homebrew 的兼容性检查使用 `pkg-config --modversion mlt-framework-7`，不得查找不存在的未版本化 `mlt-framework`；
4. `find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp REQUIRED)`，并验证该头文件中的 `NLOHMANN_JSON_VERSION_MAJOR/MINOR/PATCH` 为 3.12.0；Homebrew 的该 formula 只提供头文件，不假定其提供 CMake package config；
5. 输出每一项的 include、library 与运行时目录；发现失败立即终止，绝不静默禁用模块。

文稿与安全依赖也必须在配置阶段显式报告：`whisper.cpp`、libzip、pugixml、Poppler 和 libsodium 均采用锁定版本或上述固定源码归档与 SHA-256；不得以系统私有 API、外部在线转换服务或未验证的预编译 DLL 替代。libsodium 通过 `pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)` 导入；Qt 网络调用只能位于 desktop/ai/resources/account 层，核心层不得链接 `Qt6::Network`。

## 构建产物与部署要求

- FFmpeg 必须启用 GPL、`libx264`、`libx265`、`libfreetype`、`libass`、`libvpx`、`libopus` 与共享库；不可用的硬件后端以编译器实际探测为准。`libx264/libx265` 的许可证影响随 GPL 发布，不做非 GPL 替代。
- MLT 必须启用与 FFmpeg 8.1.2 API 兼容的 avformat/avfilter 模块；macOS 使用 Homebrew MLT 7.40.0 与同源 FFmpeg bottle，Windows 使用同一 MSVC 前缀构建；其 plugin 目录必须在开发运行和安装包中可定位。
- macOS 包必须携带所需 dylib、MLT modules、Qt frameworks/QML modules 与 GPL/第三方通知；Windows 包必须携带 DLL、MLT modules、Qt plugins/QML imports 与通知。
- 安装包检查须运行 `ffprobe -version`、加载一个 MLT avformat producer、运行 QML smoke 与一次 1080p 导出；缺少任一运行时文件都必须失败。

## 手动部署步骤

开发者先按 `docs/installation/2026-08-11-edward-0.2.0-developer-environment.md` 获取工具链。macOS 使用已核验的 Homebrew FFmpeg/MLT bottle 与官方 Qt 安装器；Windows 从上表固定 tag 检出源码、以同一前缀安装。每一步完成后保存 `deps-manifest.json`：依赖名、tag/版本、编译器、架构、configure 参数、安装前缀、二进制 SHA-256、日期。此 manifest 不含任何密钥，可以进入本地操作记录；不要将本机前缀或用户目录硬编码入工程。
