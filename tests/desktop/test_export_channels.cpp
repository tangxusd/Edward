#include "edward/desktop/workbench_runtime.hpp"

#include <QCoreApplication>

#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  edward::desktop::WorkbenchRuntime runtime;
  assert(!runtime.openResolveDeliverPage());
  assert(runtime.resolveStatus() == QStringLiteral("请先启动并连接 Resolve Studio"));
  assert(!runtime.exportWithEdwardOptions(QStringLiteral("relative.mp4"), 1920, 1080, 30, 0));
  return 0;
}
