#include <edward/resources/component_package.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <cassert>

int main() {
  QTemporaryDir directory;
  assert(directory.isValid());
  QFile preview(directory.filePath("preview.html"));
  QFile render(directory.filePath("render.html"));
  assert(preview.open(QIODevice::WriteOnly));
  assert(render.open(QIODevice::WriteOnly));

  edward::runtime::RuntimeManifest manifest;
  manifest.protocol = "edward.web-runtime.v1";
  manifest.runtime = "svg";
  manifest.width = 400;
  manifest.height = 100;
  manifest.fps = 30;
  manifest.durationInFrames = 90;
  manifest.previewEntry = "preview.html";
  manifest.renderEntry = "render.html";
  manifest.propsSchema = {{"type", "object"}};
  manifest.editableProperties = {"color"};
  const edward::resources::ComponentPackage package{
      "demo.native-runtime", "Demo", {directory.path(), "edward-runtime.json", "svg", {{"color", "#007aff"}}},
      manifest, {}, {}, "my"};
  QString error;
  assert(package.saveLocal(directory.path().toStdString(), &error));
  const auto loaded = edward::resources::ComponentPackage::load(directory.path().toStdString(), &error);
  assert(loaded);
  assert(loaded->resourceId == "demo.native-runtime");
  assert(loaded->nativeRuntime.props.value("color") == "#007aff");
  assert(QFile::exists(directory.filePath("edward-package.json")));
}
