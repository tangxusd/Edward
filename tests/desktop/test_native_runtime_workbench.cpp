#include <edward/desktop/workbench_runtime.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>

namespace {
void write(const QString& path, const QByteArray& data) { QFile file(path); assert(file.open(QIODevice::WriteOnly)); file.write(data); }
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QTemporaryDir settings; QTemporaryDir package;
  qputenv("EDWARD_SETTINGS_PATH", settings.filePath("settings.ini").toUtf8());
  edward::desktop::WorkbenchRuntime runtime;
  assert(!runtime.addNativeRuntimePackage(package.path()));
  write(package.filePath("preview.html"), "<html></html>");
  write(package.filePath("render.html"), "<html></html>");
  write(package.filePath("manifest.json"), R"({"protocol":"edward.web-runtime.v1","runtime":"svg","width":400,"height":100,"fps":30,"durationInFrames":90,"previewEntry":"preview.html","renderEntry":"render.html","propsSchema":{"type":"object"},"editableProperties":["color"]})");
  assert(runtime.addNativeRuntimePackage(package.path(), {{"color", "#007aff"}}));
  assert(runtime.nativeRuntimeSelected());
  assert(runtime.nativeRuntimeProps().value("color") == "#007aff");
  assert(runtime.nativeRuntimePreviewEntry().endsWith("preview.html"));
  assert(runtime.setNativeRuntimeProps({{"color", "#ff453a"}, {"radius", 5}}));
  assert(runtime.nativeRuntimeProps().value("color") == "#ff453a");
  const auto message = runtime.nativeRuntimeHostMessage();
  assert(message.value("protocol") == "edward.web-runtime.host-message.v1");
  assert(message.value("type") == "setFrame");
  assert(message.value("runtime") == "svg");
  assert(message.value("props").toObject().value("radius") == 5);
}
