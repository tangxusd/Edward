#include "edward/resources/catalog_cache.hpp"

#include <QJsonObject>
#include <QCoreApplication>
#include <QTemporaryDir>

#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  QTemporaryDir directory;
  edward::resources::CatalogCache cache(directory.filePath("catalog.sqlite"));
  assert(cache.open());
  auto session = cache.openSession("user", "project", edward::resources::CatalogType::Color);
  const auto first = cache.checkRevisionOnce(session, QJsonObject{{"revision", "r1"}});
  assert(first.ok && first.changed && session.revision == "r1");
  const edward::resources::ColorValue color{"component.text.color", "rgba", QJsonObject{{"r", 1}}, QString(64, QLatin1Char('a'))};
  assert(cache.storeColor(session, color));
  const auto restored = cache.readColor(session, color.semanticPath);
  assert(restored && restored->contentHash == color.contentHash);
  const auto second = cache.checkRevisionOnce(session, QJsonObject{{"revision", "r1"}});
  assert(second.ok && !second.changed);
  return 0;
}
