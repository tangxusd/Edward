#include "edward/desktop/workbench_runtime.hpp"

#include <QCoreApplication>

#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  qunsetenv("EDWARD_RESOLVE_BRIDGE_URL");
  edward::desktop::WorkbenchRuntime runtime;
  runtime.generateComponentDraft();
  assert(!runtime.setSelectedComponentPropertyAtPlayhead(QStringLiteral("demo-box"), QStringLiteral("x"), 12.0));
  return 0;
}
