#pragma once

#include "edward/resources/catalog_cache.hpp"

#include <QJsonObject>
#include <QVariantMap>

#include <QHash>

namespace edward::desktop {

struct StyleSnapshot {
  QHash<QString, edward::resources::ColorValue> colors;
  QHash<QString, edward::resources::FontRecord> fonts;
  QString catalogRevision;
  QString createdAt;

  static StyleSnapshot freeze(const edward::resources::CatalogCache& cache,
                              const edward::resources::CatalogSession& colorSession,
                              const edward::resources::CatalogSession& fontSession,
                              const QStringList& colorPaths,
                              const QStringList& fontIds);
  QJsonObject toJson() const;
  std::optional<edward::resources::ColorValue> color(const QString& semanticPath) const;
  std::optional<edward::resources::FontRecord> font(const QString& fontId) const;
};

}  // namespace edward::desktop
