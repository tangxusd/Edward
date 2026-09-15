#include <edward/desktop/workbench_runtime.hpp>

#include <QCoreApplication>

#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  edward::desktop::WorkbenchRuntime runtime;
  const auto before = runtime.preferenceStoreStatus();
  assert(before.contains("pending"));
  assert(runtime.flushPreferencesForProjectClose());
  assert(runtime.flushPreferencesForExport());
  assert(runtime.compilePreferencesNow());
  assert(runtime.preferenceStoreStatus().value("pending").toLongLong() == 0);
  assert(runtime.preferenceStore() != nullptr);
  return 0;
}
