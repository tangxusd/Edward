#include "edward/desktop/style_snapshot.hpp"

#include <QCoreApplication>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  QTemporaryDir directory;
  edward::resources::CatalogCache cache(directory.filePath("catalog.sqlite"));
  assert(cache.open());
  auto colors = cache.openSession("u", "p", edward::resources::CatalogType::Color);
  auto fonts = cache.openSession("u", "p", edward::resources::CatalogType::Font);
  assert(cache.checkRevisionOnce(colors, QJsonObject{{"revision", "r1"}}).ok);
  assert(cache.checkRevisionOnce(fonts, QJsonObject{{"revision", "r1"}}).ok);
  assert(cache.storeColor(colors, {"text.color", "rgba", QJsonObject{{"r", 1}}, QString(64, 'a')}));
  assert(cache.storeFont(fonts, {"roboto", "Roboto", "Roboto", "Regular", 400, "preview", "install", QString(64, 'b'), "1"}));
  const auto snapshot = edward::desktop::StyleSnapshot::freeze(cache, colors, fonts, {"text.color"}, {"roboto"});
  assert(snapshot.color("text.color") && snapshot.font("roboto"));
  assert(snapshot.toJson().value("catalogRevision").toString() == "r1");
  return 0;
}
