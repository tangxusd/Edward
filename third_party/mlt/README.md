# MLT 运行时

Edward 0.3.0 使用 MLT 作为媒体运行时，负责媒体 producer、时间映射、tractor 合成和导出所需的底层处理。

固定来源：`https://github.com/mltframework/mlt.git`

固定版本：`v7.40.0`

固定提交：`bef9d89c0c279e558d9625dac3399c2aa3d961bc`

许可证：LGPL-2.1-only。

MLT 作为外部依赖发现和链接，不复制到 Edward 源码树。运行时必须能定位 MLT modules 目录，并与依赖合同中的 FFmpeg 前缀一致。
