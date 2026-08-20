#include <edward/resources/component_library.hpp>

#include <QTemporaryDir>

#include <cassert>

int main() {
  QTemporaryDir directory;
  assert(directory.isValid());
  const auto component = edward::core::ComponentIr::parse({{"version", "1"}, {"root", QJsonObject{{"id", "root"}, {"type", "container"}}}});
  assert(component);
  edward::resources::ComponentLibrary library(directory.path().toStdString());
  edward::resources::ComponentPackage package{"demo.card", "Demo card", *component, {}, {}, {}, {}, "my"};
  QString error;
  assert(library.save(package, &error));
  const auto items = library.list(&error);
  assert(items.size() == 1);
  assert(items.front().resourceId == "demo.card");
  assert(items.front().category == "my");
  const auto loaded = library.load("demo.card", &error);
  assert(loaded && loaded->displayName == "Demo card");
  assert(!library.load("missing.card", &error));
  return 0;
}
