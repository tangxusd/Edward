# Edward 0.2.0 C++ 测试合同

## 唯一测试框架

0.2.0 C++ 测试唯一使用 **QtTest 与 CTest**。禁止引入 GoogleTest、Catch2 或自定义断言框架。原因是桌面应用已依赖 Qt 6；QtTest 能覆盖核心单元测试、QML/事件循环测试与 CTest 集成，避免第二套测试运行时和构建来源。

每个测试源文件必须遵从以下骨架：

```cpp
#include <QtTest/QTest>

class TestProjectStore final : public QObject {
  Q_OBJECT
private slots:
  void savesAtomically();
  void rejectsAbsoluteMediaPath();
};

void TestProjectStore::savesAtomically() {
  QCOMPARE(actualRevision, expectedRevision);
}

void TestProjectStore::rejectsAbsoluteMediaPath() {
  QVERIFY(!store.accepts(absolutePath));
}

QTEST_MAIN(TestProjectStore)
#include "test_project_store.moc"
```

断言仅使用 `QCOMPARE`、`QVERIFY`、`QVERIFY2` 与 `QSKIP`。浮点误差必须写为 `QVERIFY(qAbs(actual - expected) < epsilon)`，不得恢复 GoogleTest 的 `EXPECT_NEAR`。异步媒体/导出测试必须使用 `QTRY_VERIFY_WITH_TIMEOUT` 或 `QSignalSpy`，并写明超时值。

## CMake 与 CTest 注册合同

每个测试目标通过 `qt_add_executable` 建立，链接 `Qt6::Test` 和被测库；由 `add_test(NAME <module.test_name> COMMAND <target>)` 注册。测试名与计划中的 `ctest -R` 一致，使用小写点分格式，例如 `core.project_store`、`timeline.drop`、`media.export_job`。

```cmake
qt_add_executable(test_project_store tests/core/test_project_store.cpp)
target_link_libraries(test_project_store PRIVATE edward_core Qt6::Test)
add_test(NAME core.project_store COMMAND test_project_store)
set_tests_properties(core.project_store PROPERTIES LABELS "core")
```

每个计划任务必须先运行指定的单一 CTest 名称确认失败，再实现最小代码并重跑确认通过。测试夹具只可位于 `tests/fixtures/` 与 `tests/perf/fixtures/`，不得使用用户媒体、网络、真实订阅或真实模型密钥。

## 非 C++ 验收

QML 视觉回归由独立的 QtTest 可执行目标载入确定 QML route，使用固定窗口尺寸、固定字体回退和 PNG 基线。它不是浏览器快照，不依赖旧 HTML 运行时。FFprobe、包内容、签名、性能脚本也必须由 CTest 调用并返回非零失败码。
