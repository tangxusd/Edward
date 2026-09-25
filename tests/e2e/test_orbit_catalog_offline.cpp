#include "edward/resources/catalog_cache.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QTemporaryDir dir;
  edward::resources::CatalogCache cache(dir.filePath("catalog.db"));
  assert(cache.open());
  auto session = cache.openSession("user", "project", edward::resources::CatalogType::Color);
  const auto first = cache.checkRevisionOnce(session, QJsonObject{{"revision", "r1"}});
  assert(first.ok && first.changed);
  assert(cache.storeColor(session, {"background", "hex", QJsonObject{{"value", "#000000"}}, "hash"}));
  const auto second = cache.checkRevisionOnce(session, QJsonObject{{"revision", "r1"}});
  assert(second.ok && !second.changed);
  assert(cache.readColor(session, "background")->value.value("value") == "#000000");
  const auto invalid = cache.checkRevisionOnce(session, QJsonObject{{"revision", ""}});
  assert(!invalid.ok);
  assert(cache.readColor(session, "background")->value.value("value") == "#000000");
  return 0;
}
