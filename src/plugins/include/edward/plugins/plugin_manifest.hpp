#pragma once

#include <QJsonObject>
#include <QStringList>

#include <optional>

namespace edward::plugins {

struct PluginManifest final {
  QString pluginId;
  QString version;
  QString entry;
  QString runtime = QStringLiteral("native");
  QStringList capabilities;
  QStringList permissions;
  QStringList editableProps;

  static std::optional<PluginManifest> parse(const QJsonObject& object, QString* error = nullptr);
  [[nodiscard]] bool allows(const QString& permission) const;
};

}  // namespace edward::plugins
