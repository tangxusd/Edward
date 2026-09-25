#include "edward/desktop/style_snapshot.hpp"

#include <QDateTime>
#include <QJsonArray>

namespace edward::desktop {

StyleSnapshot StyleSnapshot::freeze(const edward::resources::CatalogCache& cache,
                                    const edward::resources::CatalogSession& colorSession,
                                    const edward::resources::CatalogSession& fontSession,
                                    const QStringList& colorPaths,
                                    const QStringList& fontIds) {
  StyleSnapshot snapshot;
  snapshot.catalogRevision = colorSession.revision == fontSession.revision ? colorSession.revision : QStringLiteral("mixed");
  snapshot.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  for (const auto& path : colorPaths) {
    if (const auto value = cache.readColor(colorSession, path)) snapshot.colors.insert(path, *value);
  }
  for (const auto& id : fontIds) {
    if (const auto value = cache.readFont(fontSession, id)) snapshot.fonts.insert(id, *value);
  }
  return snapshot;
}

QJsonObject StyleSnapshot::toJson() const {
  QJsonArray colorsArray;
  for (const auto& color : colors) {
    colorsArray.push_back(QJsonObject{{"semanticPath", color.semanticPath}, {"valueType", color.valueType}, {"value", color.value}, {"contentHash", color.contentHash}});
  }
  QJsonArray fontsArray;
  for (const auto& font : fonts) {
    fontsArray.push_back(QJsonObject{{"fontId", font.fontId}, {"displayName", font.displayName}, {"familyName", font.familyName}, {"styleName", font.styleName}, {"weight", font.weight}, {"contentHash", font.contentHash}, {"resourceVersion", font.resourceVersion}});
  }
  return QJsonObject{{"catalogRevision", catalogRevision}, {"createdAt", createdAt}, {"colors", colorsArray}, {"fonts", fontsArray}};
}

std::optional<edward::resources::ColorValue> StyleSnapshot::color(const QString& semanticPath) const {
  if (!colors.contains(semanticPath)) return std::nullopt;
  return colors.value(semanticPath);
}

std::optional<edward::resources::FontRecord> StyleSnapshot::font(const QString& fontId) const {
  if (!fonts.contains(fontId)) return std::nullopt;
  return fonts.value(fontId);
}

}  // namespace edward::desktop
