#include "edward/resources/component_package.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace edward::resources {
namespace {
bool relativePath(const QString& value) { return !value.isEmpty() && !std::filesystem::path(value.toStdString()).is_absolute() && value != "." && !value.contains(".."); }
bool validResourceId(const QString& value) { return QRegularExpression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{2,127}$")).match(value).hasMatch(); }
void setError(QString* error, const QString& value) { if (error) *error = value; }
}

std::optional<ComponentPackage> ComponentPackage::load(const std::filesystem::path& directory, QString* error) {
  QFile manifestFile(QString::fromStdString((directory / "edward-runtime.json").string()));
  QFile metadataFile(QString::fromStdString((directory / "edward-package.json").string()));
  QFile propsFile(QString::fromStdString((directory / "props.json").string()));
  if (!manifestFile.open(QIODevice::ReadOnly) || !metadataFile.open(QIODevice::ReadOnly) || !propsFile.open(QIODevice::ReadOnly)) { setError(error, "native runtime package files are missing"); return std::nullopt; }
  QJsonParseError parseError;
  const auto manifestDocument = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject()) { setError(error, "native runtime manifest is invalid"); return std::nullopt; }
  const auto metadataDocument = QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !metadataDocument.isObject()) { setError(error, "native runtime package metadata is invalid"); return std::nullopt; }
  const auto propsDocument = QJsonDocument::fromJson(propsFile.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !propsDocument.isObject()) { setError(error, "native runtime props are invalid"); return std::nullopt; }
  auto runtimeManifest = edward::runtime::RuntimeManifest::parse(manifestDocument.object(), error);
  if (!runtimeManifest) return std::nullopt;
  const auto metadata = metadataDocument.object();
  ComponentPackage package{metadata.value("resourceId").toString(), metadata.value("displayName").toString(),
      {QString::fromStdString(directory.string()), QStringLiteral("edward-runtime.json"), runtimeManifest->runtime, propsDocument.object()},
      *runtimeManifest, metadata.value("thumbnail").toString(), {}, metadata.value("category").toString(QStringLiteral("my"))};
  for (const auto& asset : metadata.value("assets").toArray()) package.assets.push_back(asset.toString());
  if (!package.validate(error)) return std::nullopt;
  return package;
}

bool ComponentPackage::validate(QString* error) const {
  if (!validResourceId(resourceId) || displayName.isEmpty() || !nativeRuntime.valid() || !runtimeManifest.validate(error)) { if (!error || error->isEmpty()) setError(error, "native runtime package metadata is invalid"); return false; }
  if (nativeRuntime.runtime != runtimeManifest.runtime || !relativePath(thumbnail) && !thumbnail.isEmpty()) { setError(error, "native runtime package paths are invalid"); return false; }
  for (const auto& asset : assets) if (!relativePath(asset)) { setError(error, "asset path must be relative"); return false; }
  return true;
}

bool ComponentPackage::saveLocal(const std::filesystem::path& directory, QString* error) const {
  if (!validate(error)) return false;
  std::error_code filesystemError;
  std::filesystem::create_directories(directory, filesystemError);
  if (filesystemError) { setError(error, "native runtime package directory cannot be created"); return false; }
  QJsonObject manifest{{"protocol", runtimeManifest.protocol}, {"runtime", runtimeManifest.runtime}, {"width", runtimeManifest.width}, {"height", runtimeManifest.height}, {"fps", runtimeManifest.fps}, {"durationInFrames", static_cast<qint64>(runtimeManifest.durationInFrames)}, {"previewEntry", runtimeManifest.previewEntry}, {"renderEntry", runtimeManifest.renderEntry}, {"propsSchema", runtimeManifest.propsSchema}, {"editableProperties", QJsonArray::fromStringList(runtimeManifest.editableProperties)}};
  QJsonObject metadata{{"resourceId", resourceId}, {"displayName", displayName}, {"thumbnail", thumbnail}, {"assets", QJsonArray::fromStringList(QList<QString>(assets.begin(), assets.end()))}, {"category", category}};
  QFile manifestFile(QString::fromStdString((directory / "edward-runtime.json").string()));
  QFile metadataFile(QString::fromStdString((directory / "edward-package.json").string()));
  QFile propsFile(QString::fromStdString((directory / "props.json").string()));
  if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate) || !metadataFile.open(QIODevice::WriteOnly | QIODevice::Truncate) || !propsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) { setError(error, "native runtime package cannot be written"); return false; }
  manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
  metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
  propsFile.write(QJsonDocument(nativeRuntime.props).toJson(QJsonDocument::Indented));
  return true;
}
}  // namespace edward::resources
