#include "edward/resources/component_package.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <fstream>
#include <QRegularExpression>

namespace edward::resources {
namespace {
bool relativePath(const QString& value) {
  if (value.isEmpty()) return false;
  const auto path = std::filesystem::path(value.toStdString());
  return !path.is_absolute() && value != "." && !value.contains("..") && !value.startsWith('/');
}
bool validResourceId(const QString& value) {
  return QRegularExpression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{2,127}$")).match(value).hasMatch();
}
void setError(QString* error, const QString& value) { if (error) *error = value; }
}

std::optional<ComponentPackage> ComponentPackage::load(const std::filesystem::path& directory, QString* error) {
  QFile manifest(QString::fromStdString((directory / "manifest.json").string()));
  QFile componentFile(QString::fromStdString((directory / "component.json").string()));
  if (!manifest.open(QIODevice::ReadOnly) || !componentFile.open(QIODevice::ReadOnly)) {
    setError(error, "component package files are missing");
    return std::nullopt;
  }
  QJsonParseError parseError;
  const auto manifestJson = QJsonDocument::fromJson(manifest.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !manifestJson.isObject()) {
    setError(error, "component package manifest is invalid");
    return std::nullopt;
  }
  const auto componentJson = QJsonDocument::fromJson(componentFile.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !componentJson.isObject()) {
    setError(error, "component package component is invalid");
    return std::nullopt;
  }
  auto component = edward::core::ComponentIr::parse(componentJson.object());
  if (!component) { setError(error, "component ir is invalid"); return std::nullopt; }
  ComponentPackage package{manifestJson.object().value("resourceId").toString(),
                           manifestJson.object().value("displayName").toString(), *component,
                           manifestJson.object().value("pluginId").toString(),
                           manifestJson.object().value("pluginVersion").toString(),
                           manifestJson.object().value("thumbnail").toString(), {}};
  package.category = manifestJson.object().value("category").toString(QStringLiteral("my"));
  for (const auto& asset : manifestJson.object().value("assets").toArray()) package.assets.push_back(asset.toString());
  if (!package.validate(error)) return std::nullopt;
  return package;
}

bool ComponentPackage::validate(QString* error) const {
  if (!validResourceId(resourceId) || displayName.isEmpty()) { setError(error, "resource id or display name is invalid"); return false; }
  if (!component.validate(error)) return false;
  const auto dependency = component.pluginDependency();
  if (dependency) {
    if (pluginId != dependency->pluginId || pluginVersion != dependency->version) {
      setError(error, "package plugin dependency does not match component");
      return false;
    }
  } else if (!pluginId.isEmpty() || !pluginVersion.isEmpty()) {
    setError(error, "package declares an unexpected plugin dependency");
    return false;
  }
  if (!thumbnail.isEmpty() && !relativePath(thumbnail)) { setError(error, "thumbnail must be relative"); return false; }
  std::vector<QString> uniqueAssets;
  for (const auto& asset : assets) {
    if (!relativePath(asset) || uniqueAssets.end() != std::find(uniqueAssets.begin(), uniqueAssets.end(), asset)) {
      setError(error, "asset path must be relative and unique");
      return false;
    }
    uniqueAssets.push_back(asset);
  }
  return true;
}

bool ComponentPackage::saveLocal(const std::filesystem::path& directory, QString* error) const {
  if (!validate(error)) return false;
  std::error_code filesystemError;
  std::filesystem::create_directories(directory, filesystemError);
  if (filesystemError) { setError(error, "component package directory cannot be created"); return false; }
  QFile manifest(QString::fromStdString((directory / "manifest.json").string()));
  QFile componentFile(QString::fromStdString((directory / "component.json").string()));
  if (!manifest.open(QIODevice::WriteOnly | QIODevice::Truncate) || !componentFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    setError(error, "component package cannot be written");
    return false;
  }
  QJsonArray assetsJson;
  for (const auto& asset : assets) assetsJson.append(asset);
  manifest.write(QJsonDocument(QJsonObject{{"resourceId", resourceId}, {"displayName", displayName},
                                            {"pluginId", pluginId}, {"pluginVersion", pluginVersion},
                                            {"thumbnail", thumbnail}, {"category", category},
                                            {"assets", assetsJson}}).toJson(QJsonDocument::Indented));
  componentFile.write(QJsonDocument(component.toJson()).toJson(QJsonDocument::Indented));
  return true;
}

}  // namespace edward::resources
