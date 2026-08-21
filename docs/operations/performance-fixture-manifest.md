# Edward 性能夹具清单

`edward_benchmark --manifest <fixtures.json> --report <report.json>` 在开始任何采集前校验本清单。它要求以下三个 ID 均存在：`1080p`、`4k`、`vfr`。

真实采集使用 `--collect`；若采集被重启或系统中断，可使用同一份清单和报告执行
`--collect --resume`。程序会跳过检查点中已经完整完成的夹具，只继续剩余夹具。

```json
{
  "fixtures": [
    {
      "id": "1080p",
      "path": "/绝对路径/edward-1080p.mp4",
      "sha256": "64 位小写 SHA-256"
    },
    {
      "id": "4k",
      "path": "/绝对路径/edward-4k.mp4",
      "sha256": "64 位小写 SHA-256"
    },
    {
      "id": "vfr",
      "path": "/绝对路径/edward-vfr.mp4",
      "sha256": "64 位小写 SHA-256"
    }
  ]
}
```

媒体二进制不进入 Git；在两台基准机上使用同一份清单。文件内容变化、路径失效、ID 缺失或 SHA-256 不一致都会生成 `fixture_validation_failed`，不得继续采集或填写发布结论。

可用 `packaging/macos/GeneratePerformanceFixtureManifest.cmake` 生成清单：

```bash
cmake \
  -DOUTPUT_PATH=build/performance/fixtures.json \
  -DFIXTURE_1080P=/绝对路径/edward-1080p.mp4 \
  -DFIXTURE_4K=/绝对路径/edward-4k.mp4 \
  -DFIXTURE_VFR=/绝对路径/edward-vfr.mp4 \
  -P packaging/macos/GeneratePerformanceFixtureManifest.cmake
```

三个文件必须真实存在；生成器会重新计算 SHA-256，不接受手填哈希。
