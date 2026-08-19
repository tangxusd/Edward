#include <edward/resources/component_package.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <cassert>

int main() {
  const QJsonObject root{{"id", "root"}, {"type", "container"}};
  const auto component = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(component);
  edward::resources::ComponentPackage package{"demo.card", "Demo Card", *component, "thumb.png", {"thumb.png"}};
  QTemporaryDir directory;
  assert(directory.isValid());
  QString error;
  assert(package.saveLocal(directory.path().toStdString(), &error));
  const auto loaded = edward::resources::ComponentPackage::load(directory.path().toStdString(), &error);
  assert(loaded && loaded->resourceId == "demo.card");
  edward::resources::ComponentPackage missingId{"", "Demo", *component, {}, {}};
  assert(!missingId.validate());
  edward::resources::ComponentPackage absoluteAsset{"demo", "Demo", *component, "/private/file.png", {}};
  assert(!absoluteAsset.validate());
  return 0;
}
