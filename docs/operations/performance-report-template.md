# Edward 性能基准报告模板

此模板只记录已在指定基准设备和已校验夹具上实际完成的测量；不得用开发机、小尺寸测试素材或缺失指标替代发布结论。

## 执行信息

- 报告 JSON：
- 执行时间（UTC）：
- Edward 提交：
- macOS：M1 MacBook Air / 8 GB，或 Windows：第 10 代 i5 / 16 GB / GTX 1030。
- Qt、FFmpeg、MLT、驱动版本：

## 夹具校验

| ID | 文件 | SHA-256 | 时长 / 帧率 / 编码 | 状态 |
| --- | --- | --- | --- | --- |
| 1080p |  |  |  |  |
| 4k |  |  |  |  |
| vfr |  |  |  |  |

## 五次采样结果

每个场景运行五次，报告中填写中位数与 p95。场景至少包括冷启动、导入、首帧、seek、拖拽、代理、H.264/HEVC/ProRes 导出和输出 SSIM。仅通过夹具校验时，状态必须为 `fixtures_ready_not_collected`，不得填写或推断性能结论。

| 场景 | 中位数 | p95 | 峰值 RSS | GPU 内存 | 失败原因 |
| --- | --- | --- | --- | --- | --- |
| 冷启动 |  |  |  |  |  |
| 首帧 |  |  |  |  |  |
| Seek |  |  |  |  |  |
| 拖拽 |  |  |  |  |  |
| 代理生成 |  |  |  |  |  |
| 导出 |  |  |  |  |  |

## 部分采集状态

当报告 JSON 的 `status` 为 `metrics_collected_partial` 时，只能引用 `samples` 中实际存在的
`importMedianMs`、`firstFrameMedianMs` 和 `seekP95Ms`。必须同时保留
`uncollectedRequiredMetrics`，不得将空白项补成估算值，也不得据此做发布性能或竞品比较结论。

## 结论

仅在两台指定设备均完成相同夹具、相同项目和五次采样后，才能填写与剪映 11.0.0 的比较结论。
