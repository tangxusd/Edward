#include "edward/desktop/workbench_runtime.hpp"

#include <QCoreApplication>

#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  qunsetenv("EDWARD_RESOLVE_BRIDGE_URL");
  edward::desktop::WorkbenchRuntime runtime;
  assert(!runtime.resolveConnected());
  assert(runtime.resolveStatus() == QStringLiteral("未连接 Resolve Studio"));
  assert(!runtime.connectResolve());
  assert(!runtime.resolveConnected());
  assert(runtime.resolveStatus() == QStringLiteral("请先启动并连接 Resolve Studio"));
  assert(runtime.setPlayhead(10));
  runtime.disconnectResolve();
  assert(!runtime.resolveConnected());
  assert(runtime.resolveStatus() == QStringLiteral("未连接 Resolve Studio"));
  return 0;
}
