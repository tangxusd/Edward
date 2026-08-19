#include "edward/plugins/plugin_manifest.hpp"

#include <QJsonArray>

#include <QRegularExpression>

namespace edward::plugins {
namespace {

QStringList stringList(const QJsonValue& value) {
  if (!value.isArray()) return {};
  QStringList result;
  for (const auto& item : value.toArray()) {
    if (!item.isString() || item.toString().isEmpty()) return {};
    result.push_back(item.toString());
  }
  return result;
}

bool requiredText(const QJsonObject& object, const char* key, QString& target, QString* error) {
  target = object.value(QLatin1String(key)).toString();
  if (target.isEmpty()) {
    if (error) *error = QStringLiteral("manifest field is required: %1").arg(QString::fromLatin1(key));
    return false;
  }
  return true;
}

}  // namespace

std::optional<PluginManifest> PluginManifest::parse(const QJsonObject& object, QString* error) {
  PluginManifest manifest;
  if (!requiredText(object, "pluginId", manifest.pluginId, error) ||
      !requiredText(object, "version", manifest.version, error) ||
      !requiredText(object, "entry", manifest.entry, error)) return std::nullopt;
  if (object.contains("runtime")) {
    manifest.runtime = object.value("runtime").toString();
    if (manifest.runtime != QStringLiteral("native") && manifest.runtime != QStringLiteral("node") &&
        manifest.runtime != QStringLiteral("bun")) {
      if (error) *error = QStringLiteral("manifest runtime is not supported");
      return std::nullopt;
    }
  }
  manifest.signingKeyId = object.value("signingKeyId").toString();
  manifest.signatureBase64 = object.value("signature").toString();
  if (manifest.signingKeyId.isEmpty() != manifest.signatureBase64.isEmpty()) {
    if (error) *error = QStringLiteral("manifest signing key and signature must be supplied together");
    return std::nullopt;
  }
  if (manifest.entry.startsWith('/') || manifest.entry.contains(QStringLiteral(".."))) {
    if (error) *error = QStringLiteral("manifest entry must be a relative path");
    return std::nullopt;
  }
  const auto capabilities = stringList(object.value("capabilities"));
  const auto permissions = stringList(object.value("permissions"));
  const auto editableProps = stringList(object.value("editableProps"));
  if ((object.contains("capabilities") && capabilities.isEmpty()) ||
      (object.contains("permissions") && permissions.isEmpty()) ||
      (object.contains("editableProps") && editableProps.isEmpty())) {
    if (error) *error = QStringLiteral("manifest lists must contain non-empty strings");
    return std::nullopt;
  }
  manifest.capabilities = capabilities;
  manifest.permissions = permissions;
  manifest.editableProps = editableProps;
  if (manifest.allows(QStringLiteral("network")) || manifest.allows(QStringLiteral("engine_write"))) {
    if (error) *error = QStringLiteral("manifest requests a forbidden permission");
    return std::nullopt;
  }
  return manifest;
}

bool PluginManifest::allows(const QString& permission) const { return permissions.contains(permission); }

}  // namespace edward::plugins
