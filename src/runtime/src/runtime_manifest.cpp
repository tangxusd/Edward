#include "edward/runtime/runtime_manifest.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QRegularExpression>

#include <cmath>
#include <limits>

namespace edward::runtime {
namespace {

void setError(QString* error, const QString& message) {
  if (error) *error = message;
}

bool requiredString(const QJsonObject& object, const char* key, QString& target, QString* error) {
  const auto value = object.value(QLatin1String(key));
  if (!value.isString() || value.toString().isEmpty()) {
    setError(error, QStringLiteral("runtime manifest field is required: %1").arg(QString::fromLatin1(key)));
    return false;
  }
  target = value.toString();
  return true;
}

bool validEntryPath(const QString& path) {
  if (path.isEmpty() || path.startsWith('/') || path.startsWith('\\') || path.contains(':')) return false;
  const auto parts = path.split(QRegularExpression(QStringLiteral("[/\\\\]")), Qt::KeepEmptyParts);
  for (const auto& part : parts) {
    if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..")) return false;
  }
  return true;
}

bool positiveInteger(const QJsonValue& value, std::int64_t& target) {
  if (!value.isDouble()) return false;
  const double number = value.toDouble();
  if (!std::isfinite(number) || number <= 0.0 || std::floor(number) != number ||
      number >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) return false;
  target = static_cast<std::int64_t>(number);
  return true;
}

}  // namespace

std::optional<RuntimeManifest> RuntimeManifest::parse(const QJsonObject& object, QString* error) {
  static const QSet<QString> allowed = {
      QStringLiteral("protocol"),          QStringLiteral("runtime"),
      QStringLiteral("width"),             QStringLiteral("height"),
      QStringLiteral("fps"),               QStringLiteral("durationInFrames"),
      QStringLiteral("previewEntry"),      QStringLiteral("renderEntry"),
      QStringLiteral("propsSchema"),       QStringLiteral("editableProperties"),
  };
  for (auto it = object.begin(); it != object.end(); ++it) {
    if (!allowed.contains(it.key())) {
      setError(error, QStringLiteral("runtime manifest field is not allowed: %1").arg(it.key()));
      return std::nullopt;
    }
  }

  RuntimeManifest manifest;
  if (!requiredString(object, "protocol", manifest.protocol, error) ||
      !requiredString(object, "runtime", manifest.runtime, error) ||
      !requiredString(object, "previewEntry", manifest.previewEntry, error) ||
      !requiredString(object, "renderEntry", manifest.renderEntry, error))
    return std::nullopt;

  const auto width = object.value(QStringLiteral("width"));
  const auto height = object.value(QStringLiteral("height"));
  const auto fps = object.value(QStringLiteral("fps"));
  std::int64_t duration = 0;
  std::int64_t widthValue = 0;
  std::int64_t heightValue = 0;
  if (!positiveInteger(width, widthValue) || widthValue > std::numeric_limits<int>::max() ||
      !positiveInteger(height, heightValue) || heightValue > std::numeric_limits<int>::max()) {
    setError(error, QStringLiteral("runtime manifest width and height must be positive integers"));
    return std::nullopt;
  }
  manifest.width = static_cast<int>(widthValue);
  manifest.height = static_cast<int>(heightValue);
  if (!fps.isDouble() || !std::isfinite(fps.toDouble()) || fps.toDouble() <= 0.0) {
    setError(error, QStringLiteral("runtime manifest fps must be positive"));
    return std::nullopt;
  }
  manifest.fps = fps.toDouble();
  if (!positiveInteger(object.value(QStringLiteral("durationInFrames")), duration)) {
    setError(error, QStringLiteral("runtime manifest durationInFrames must be a positive integer"));
    return std::nullopt;
  }
  manifest.durationInFrames = duration;

  manifest.propsSchema = object.value(QStringLiteral("propsSchema")).toObject();
  if (!object.value(QStringLiteral("propsSchema")).isObject()) {
    setError(error, QStringLiteral("runtime manifest propsSchema must be an object"));
    return std::nullopt;
  }
  const auto editable = object.value(QStringLiteral("editableProperties"));
  if (!editable.isArray()) {
    setError(error, QStringLiteral("runtime manifest editableProperties must be an array"));
    return std::nullopt;
  }
  for (const auto& value : editable.toArray()) {
    if (!value.isString() || value.toString().isEmpty() || manifest.editableProperties.contains(value.toString())) {
      setError(error, QStringLiteral("runtime manifest editableProperties must contain unique non-empty strings"));
      return std::nullopt;
    }
    manifest.editableProperties.push_back(value.toString());
  }
  if (!manifest.validate(error)) return std::nullopt;
  return manifest;
}

bool RuntimeManifest::validate(QString* error) const {
  if (protocol != QStringLiteral("edward.web-runtime.v1")) {
    setError(error, QStringLiteral("runtime manifest protocol is not supported"));
    return false;
  }
  if (runtime != QStringLiteral("react") && runtime != QStringLiteral("html-css") &&
      runtime != QStringLiteral("svg") && runtime != QStringLiteral("gsap")) {
    setError(error, QStringLiteral("runtime manifest runtime is not supported"));
    return false;
  }
  if (width <= 0 || height <= 0 || !std::isfinite(fps) || fps <= 0.0 || durationInFrames <= 0) {
    setError(error, QStringLiteral("runtime manifest metadata must be positive"));
    return false;
  }
  if (!validEntryPath(previewEntry) || !validEntryPath(renderEntry)) {
    setError(error, QStringLiteral("runtime manifest entries must be relative paths without traversal"));
    return false;
  }
  for (const auto& property : editableProperties) {
    if (property.isEmpty() || editableProperties.count(property) != 1) {
      setError(error, QStringLiteral("runtime manifest editableProperties must not contain empty names"));
      return false;
    }
  }
  return true;
}

}  // namespace edward::runtime
