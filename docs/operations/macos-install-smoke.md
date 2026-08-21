# macOS 开发包验收清单

当前阶段只验收 `.app`，不验收 DMG、签名或公证。

## 构建

```bash
cmake -S . -B build/0.3-runtime \
  -DEDWARD_MLT_MODULE_DIR=/实际/MLT/modules \
  -DEDWARD_MLT_DATA_DIR=/实际/MLT/data
cmake --build build/0.3-runtime --target validate_macos_development_bundle
```

## 自动检查

- `.app` 目录和主可执行文件存在。
- 主程序动态库引用为 bundle 相对路径或系统框架。
- Frameworks 与 `Resources/mlt/modules` 内的所有 Mach-O 文件无开发机前缀依赖。
- MLT 模块、数据、预设实际存在。
- 开发包保留 `NOT_FOR_DISTRIBUTION.txt`。

## 当前边界

通过上述检查只代表开发包结构和依赖可迁移；首次 GUI 启动、素材导入、预览和长视频导出仍需在本机窗口会话中确认。正式发布还需要真实许可证、第三方声明、字体授权、签名和公证。
