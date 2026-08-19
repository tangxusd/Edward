#include <edward/resources/component_package.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <cassert>

int main() {
  const QJsonObject root{{"id", "root"}, {"type", "container"}};
  const auto component = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(component);
  edward::resources::ComponentPackage package{"demo.card", "Demo Card", *component, {}, {}, "thumb.png", {"thumb.png"}};
  QTemporaryDir directory;
  assert(directory.isValid());
  QString error;
  assert(package.saveLocal(directory.path().toStdString(), &error));
  const auto loaded = edward::resources::ComponentPackage::load(directory.path().toStdString(), &error);
  assert(loaded && loaded->resourceId == "demo.card");
  edward::resources::ComponentPackage missingId{"bad id", "Demo", *component, {}, {}, {}, {}};
  assert(!missingId.validate());
  edward::resources::ComponentPackage absoluteAsset{"demo", "Demo", *component, {}, {}, "/private/file.png", {}};
  assert(!absoluteAsset.validate());
  edward::resources::ComponentPackage duplicateAssets{"demo.card", "Demo", *component, {}, {}, {}, {"a.png", "a.png"}};
  assert(!duplicateAssets.validate());
  const auto dependencyComponent = edward::core::ComponentIr::parse(
      {{"version", "1"}, {"root", root}, {"pluginDependency", QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"}}}});
  assert(dependencyComponent);
  edward::resources::ComponentPackage dependencyPackage{"demo.plugin", "Plugin", *dependencyComponent,
      "remotion", "1.0.0", {}, {}};
  assert(dependencyPackage.validate());
  dependencyPackage.pluginVersion = "2.0.0";
  assert(!dependencyPackage.validate());
  return 0;
}
