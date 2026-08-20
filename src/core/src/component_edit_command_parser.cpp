#include "edward/core/component_edit_command_parser.hpp"

#include <QJsonDocument>

namespace edward::core {
namespace {

bool isScalar(const QJsonValue& value) {
  return value.isDouble() || value.isString() || value.isBool();
}

std::optional<ComponentEditKind> kindForOperation(const QString& operation) {
  if (operation == QStringLiteral("setTransformNumber")) return ComponentEditKind::SetTransformNumber;
  if (operation == QStringLiteral("setProperty")) return ComponentEditKind::SetProperty;
  if (operation == QStringLiteral("setKeyframeValue")) return ComponentEditKind::SetKeyframeValue;
  return std::nullopt;
}

}  // namespace

std::optional<ComponentEditCommand> parseComponentEditCommand(const QJsonObject& object, QString* error) {
  const auto kind = kindForOperation(object.value(QStringLiteral("operation")).toString());
  const auto nodeId = object.value(QStringLiteral("nodeId")).toString();
  const auto field = object.value(QStringLiteral("field")).toString();
  const auto value = object.value(QStringLiteral("value"));
  if (!kind || nodeId.isEmpty() || field.isEmpty() || !isScalar(value)) {
    if (error) *error = QStringLiteral("AI component command has invalid operation or target");
    return std::nullopt;
  }
  if (*kind == ComponentEditKind::SetTransformNumber && !value.isDouble()) {
    if (error) *error = QStringLiteral("AI transform command requires a numeric value");
    return std::nullopt;
  }
  int frame = 0;
  if (*kind == ComponentEditKind::SetKeyframeValue) {
    const auto frameValue = object.value(QStringLiteral("frame"));
    if (!frameValue.isDouble() || frameValue.toInt() < 0) {
      if (error) *error = QStringLiteral("AI keyframe command requires a non-negative frame");
      return std::nullopt;
    }
    frame = frameValue.toInt();
  } else if (object.contains(QStringLiteral("frame"))) {
    if (error) *error = QStringLiteral("AI non-keyframe command must not include a frame");
    return std::nullopt;
  }
  return ComponentEditCommand{*kind, nodeId, field, frame, value};
}

std::optional<ComponentEditCommand> parseComponentEditCommandText(const QString& text, QString* error) {
  QString candidate = text.trimmed();
  if (candidate.startsWith(QStringLiteral("```json")) && candidate.endsWith(QStringLiteral("```"))) {
    candidate = candidate.sliced(7, candidate.size() - 10).trimmed();
  } else if (candidate.startsWith(QStringLiteral("```"))) {
    if (error) *error = QStringLiteral("AI 命令代码块必须标记为 json");
    return std::nullopt;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(candidate.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    if (error) *error = QStringLiteral("AI 响应必须是单个 JSON 对象");
    return std::nullopt;
  }
  return parseComponentEditCommand(document.object(), error);
}

}  // namespace edward::core
