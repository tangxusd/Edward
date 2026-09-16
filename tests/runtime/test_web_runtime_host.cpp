#include <edward/runtime/web_runtime_host.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>

namespace {
void createEntryFile(const QString& path) {
  QFile file(path);
  assert(file.open(QIODevice::WriteOnly));
}

edward::runtime::RuntimeManifest manifest() {
  edward::runtime::RuntimeManifest value;
  value.protocol = "edward.web-runtime.v1";
  value.runtime = "react";
  value.width = 1920;
  value.height = 1080;
  value.fps = 30;
  value.durationInFrames = 90;
  value.previewEntry = "preview.html";
  value.renderEntry = "render.html";
  value.propsSchema = QJsonObject{{"type", "object"}};
  return value;
}
void testMountsAndSharesFrameProps() {
  QTemporaryDir dir;
  createEntryFile(dir.filePath("preview.html"));
  createEntryFile(dir.filePath("render.html"));
  edward::runtime::WebRuntimeHost host;
  const auto initial = QJsonObject{{"color", "blue"}};
  assert(host.mount(manifest(), dir.path(), initial).ok);
  assert(host.setFrame(12).ok && host.frame() == 12);
  assert(host.setProps(QJsonObject{{"color", "red"}}).ok);
  assert(host.props().value("color") == "red");
  assert(host.renderFrame(12, dir.filePath("out.png")).ok);
  const auto message = host.message("renderFrame");
  assert(message.value("frame") == 12 && message.value("props").toObject().value("color") == "red");
}
void testRejectsOutsideOrMissingPackage() {
  edward::runtime::WebRuntimeHost host;
  QTemporaryDir dir;
  assert(!host.mount(manifest(), dir.filePath("missing"), {}).ok);
  createEntryFile(dir.filePath("preview.html"));
  createEntryFile(dir.filePath("render.html"));
  assert(!host.mount(manifest(), dir.filePath("preview.html"), {}).ok);
}
void testRejectsInvalidFrameAndUnmount() {
  QTemporaryDir dir;
  createEntryFile(dir.filePath("preview.html"));
  createEntryFile(dir.filePath("render.html"));
  edward::runtime::WebRuntimeHost host;
  assert(host.mount(manifest(), dir.path(), {}).ok);
  assert(!host.setFrame(-1).ok && !host.setFrame(90).ok);
  host.unmount();
  assert(!host.setProps({}).ok);
}
}
int main() {
  testMountsAndSharesFrameProps();
  testRejectsOutsideOrMissingPackage();
  testRejectsInvalidFrameAndUnmount();
}
