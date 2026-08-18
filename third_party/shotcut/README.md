# Shotcut 选择性来源

Edward 0.3.0 不编译 Shotcut 完整应用，也不使用 Shotcut 的 QML 界面。本目录只记录经过许可证审计、可能被选择性参考或移植的时间线编辑来源。

固定来源：`https://github.com/mltframework/shotcut.git`

固定版本：`v26.8.1`

固定提交：`0474a712131fe1a82d499a32f3d54be956d2963f`

许可证：GPL-3.0-or-later（以固定提交的 `COPYING` 文件为准）。

当前阶段不导入 Shotcut 源文件；`SOURCES.json` 的 `importedFiles` 为空。后续每个被移植文件必须先加入来源路径、目标路径、许可证、提交和移植理由，再进入代码审查。
