#include "edward/plugins/installed_plugin.hpp"
#include "edward/plugins/plugin_trust_store.hpp"

#include <QFile>
#include <QJsonDocument>

namespace edward::plugins {

namespace {

bool verifyReleaseManifest(const PluginManifest& manifest, QString* error) {
#ifdef EDWARD_RELEASE_BUILD
#if defined(EDWARD_PLUGIN_TRUSTED_KEY_ID) && defined(EDWARD_PLUGIN_TRUSTED_PUBLIC_KEY_B64)
  const auto publicKey = PluginTrustStore::decodeBase64PublicKey(
      QStringLiteral(EDWARD_PLUGIN_TRUSTED_PUBLIC_KEY_B64));
  if (!publicKey) {
    if (error) *error = QStringLiteral("Edward plugin trust root is invalid");
    return false;
  }
  PluginTrustStore store({{QStringLiteral(EDWARD_PLUGIN_TRUSTED_KEY_ID), *publicKey}});
  if (store.verifyManifest(manifest)) return true;
  if (error) *error = QStringLiteral("plugin manifest signature is not trusted");
  return false;
#else
  Q_UNUSED(manifest);
  if (error) *error = QStringLiteral("Edward plugin trust root is unavailable");
  return false;
#endif
#else
  Q_UNUSED(manifest);
  Q_UNUSED(error);
  return true;
#endif
}

bool pathInside(const std::filesystem::path& child, const std::filesystem::path& parent) {
  auto childIt = child.begin();
  auto parentIt = parent.begin();
  for (; parentIt != parent.end(); ++parentIt, ++childIt) {
    if (childIt == child.end() || *childIt != *parentIt) return false;
  }
  return true;
}

}  // namespace

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
  if (!verifyReleaseManifest(*manifest, error)) return std::nullopt;
  const auto entryPath = root / manifest->entry.toStdString();
  std::error_code statusError;
  if (std::filesystem::is_symlink(std::filesystem::symlink_status(entryPath, statusError)) || statusError ||
      !std::filesystem::is_regular_file(entryPath)) {
    if (error) *error = QStringLiteral("plugin entry is unavailable");
    return std::nullopt;
  }
  std::error_code canonicalError;
  const auto canonicalRoot = std::filesystem::canonical(root, canonicalError);
  const auto canonicalEntry = std::filesystem::canonical(entryPath, canonicalError);
  if (canonicalError || !pathInside(canonicalEntry, canonicalRoot)) {
    if (error) *error = QStringLiteral("plugin entry must stay inside plugin root");
    return std::nullopt;
  }
  return InstalledPlugin{root, std::move(*manifest)};
}

}  // namespace edward::plugins
