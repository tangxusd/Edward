#include "edward/plugins/installed_plugin.hpp"

#include <QFile>
#include <QJsonDocument>

namespace edward::plugins {

PluginDependencyStatus dependencyStatus(const edward::core::ComponentIr& component,
                                        const std::optional<InstalledPlugin>& installedPlugin) {
  const auto& dependency = component.pluginDependency();
  if (!dependency) return PluginDependencyStatus::NotRequired;
  if (!installedPlugin || installedPlugin->manifest.pluginId != dependency->pluginId)
    return PluginDependencyStatus::Missing;
  return installedPlugin->manifest.version == dependency->version
             ? PluginDependencyStatus::Available
             : PluginDependencyStatus::VersionMismatch;
}

std::optional<InstalledPlugin> loadInstalledPlugin(const std::filesystem::path& root, QString* error) {
  if (root.empty() || !std::filesystem::is_directory(root)) {
    if (error) *error = QStringLiteral("plugin root is unavailable");
    return std::nullopt;
  }
  const auto manifestPath = root / "edward-plugin.json";
  if (!std::filesystem::is_regular_file(manifestPath)) {
    if (error) *error = QStringLiteral("plugin manifest is unavailable");
    return std::nullopt;
  }
  QFile file(QString::fromStdString(manifestPath.string()));
  if (!file.open(QIODevice::ReadOnly)) {
    if (error) *error = QStringLiteral("plugin manifest cannot be read");
    return std::nullopt;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    if (error) *error = QStringLiteral("plugin manifest is invalid JSON");
    return std::nullopt;
  }
  auto manifest = PluginManifest::parse(document.object(), error);
  if (!manifest) return std::nullopt;
  if (!std::filesystem::is_regular_file(root / manifest->entry.toStdString())) {
    if (error) *error = QStringLiteral("plugin entry is unavailable");
    return std::nullopt;
  }
  return InstalledPlugin{root, std::move(*manifest)};
}

}  // namespace edward::plugins
