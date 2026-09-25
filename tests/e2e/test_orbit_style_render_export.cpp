#include "edward/desktop/style_snapshot.hpp"
#include "edward/resources/catalog_cache.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QTemporaryDir>
#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QTemporaryDir dir;
  edward::resources::CatalogCache cache(dir.filePath("catalog.db"));
  assert(cache.open());
  auto colorSession = cache.openSession("user", "project", edward::resources::CatalogType::Color);
  auto fontSession = cache.openSession("user", "project", edward::resources::CatalogType::Font);
  assert(cache.checkRevisionOnce(colorSession, QJsonObject{{"revision", "r1"}}).ok);
  assert(cache.checkRevisionOnce(fontSession, QJsonObject{{"revision", "r1"}}).ok);
  assert(cache.storeColor(colorSession, {"text.primary", "rgba", QJsonObject{{"r", 1}, {"g", 2}, {"b", 3}, {"a", 1}}, "hash-color"}));
  assert(cache.storeFont(fontSession, {"font-1", "Orbit Sans", "Orbit Sans", "Regular", 400, "preview", "install", "hash-font", "v1"}));
  const auto snapshot = edward::desktop::StyleSnapshot::freeze(cache, colorSession, fontSession, {"text.primary"}, {"font-1"});
  assert(snapshot.color("text.primary")->contentHash == "hash-color");
  assert(snapshot.font("font-1")->contentHash == "hash-font");
  const auto serialized = snapshot.toJson();
  const auto colors = serialized.value("colors").toArray();
  assert(!colors.isEmpty() && colors.first().toObject().value("contentHash") == "hash-color");
  return 0;
}
