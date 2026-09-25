#include "edward/ai/action_plan.hpp"

#include <QSet>

namespace edward::ai {
namespace {
void fail(QString* error, const QString& message) { if (error) *error = message; }
bool string(const QJsonObject& object, const char* key, QString& value) {
  const auto raw = object.value(QLatin1String(key));
  if (!raw.isString() || raw.toString().isEmpty()) return false;
  value = raw.toString();
  return true;
}
bool knownOperation(const QString& value) {
  static const QSet<QString> allowed = {
      "insert_native_component", "set_component_props", "move_clip", "resize_clip", "remove_clip",
      "insert_media", "insert_text", "insert_subtitle", "set_clip_props", "replace_source",
      "duplicate_clip", "split_clip", "trim_head", "trim_tail", "set_speed", "set_audio_props",
      "create_marker", "delete_marker", "set_marker_color", "close_gap"};
  return allowed.contains(value);
}

bool hasOnlyFields(const QJsonObject& operation, const QSet<QString>& allowed) {
  for (auto it = operation.begin(); it != operation.end(); ++it)
    if (!allowed.contains(it.key())) return false;
  return true;
}

bool validOperationShape(const QJsonObject& operation, QString* error) {
  const auto type = operation.value(QStringLiteral("type")).toString();
  const auto requiredString = [&operation](const char* key) {
    return operation.value(QLatin1String(key)).isString() && !operation.value(QLatin1String(key)).toString().isEmpty();
  };
  if (type == QStringLiteral("insert_native_component")) {
    if (hasOnlyFields(operation, {QStringLiteral("type"), QStringLiteral("resourceId")}) && requiredString("resourceId")) return true;
  } else if (type == QStringLiteral("set_component_props")) {
    if (hasOnlyFields(operation, {QStringLiteral("type"), QStringLiteral("targetId"), QStringLiteral("props")}) &&
        requiredString("targetId") && operation.value(QStringLiteral("props")).isObject()) return true;
  } else if (type == QStringLiteral("move_clip")) {
    if (hasOnlyFields(operation, {QStringLiteral("type"), QStringLiteral("targetId"), QStringLiteral("timelineStart")}) &&
        requiredString("targetId") && operation.value(QStringLiteral("timelineStart")).isDouble() &&
        operation.value(QStringLiteral("timelineStart")).toInteger(-1) >= 0) return true;
  } else if (type == QStringLiteral("resize_clip")) {
    if (hasOnlyFields(operation, {QStringLiteral("type"), QStringLiteral("targetId"), QStringLiteral("durationFrames")}) &&
        requiredString("targetId") && operation.value(QStringLiteral("durationFrames")).isDouble() &&
        operation.value(QStringLiteral("durationFrames")).toInteger(0) > 0) return true;
  } else if (type == QStringLiteral("remove_clip")) {
    if (hasOnlyFields(operation, {QStringLiteral("type"), QStringLiteral("targetId")}) && requiredString("targetId")) return true;
  } else if (type == QStringLiteral("insert_media")) {
    if (hasOnlyFields(operation, {"type", "mediaId", "kind", "timelineStart", "durationFrames", "track"}) && requiredString("mediaId")) return true;
  } else if (type == QStringLiteral("insert_text") || type == QStringLiteral("insert_subtitle")) {
    if (hasOnlyFields(operation, {"type", "text", "props", "timelineStart", "durationFrames", "track"}) && operation.value("text").isString()) return true;
  } else if (type == QStringLiteral("set_clip_props") || type == QStringLiteral("set_audio_props")) {
    if (hasOnlyFields(operation, {"type", "targetId", "props"}) && requiredString("targetId") && operation.value("props").isObject()) return true;
  } else if (type == QStringLiteral("replace_source")) {
    if (hasOnlyFields(operation, {"type", "targetId", "mediaId"}) && requiredString("targetId") && requiredString("mediaId")) return true;
  } else if (type == QStringLiteral("duplicate_clip")) {
    if (hasOnlyFields(operation, {"type", "targetId", "timelineStart", "track"}) && requiredString("targetId")) return true;
  } else if (type == QStringLiteral("split_clip") || type == QStringLiteral("trim_head") || type == QStringLiteral("trim_tail")) {
    if (hasOnlyFields(operation, {"type", "targetId", "atFrame"}) && requiredString("targetId") && operation.value("atFrame").isDouble()) return true;
  } else if (type == QStringLiteral("set_speed")) {
    if (hasOnlyFields(operation, {"type", "targetId", "speed"}) && requiredString("targetId") && operation.value("speed").isDouble()) return true;
  } else if (type == QStringLiteral("create_marker")) {
    if (hasOnlyFields(operation, {"type", "timelineFrame", "label", "color", "clipId", "localFrame"}) && operation.value("timelineFrame").isDouble()) return true;
  } else if (type == QStringLiteral("delete_marker") || type == QStringLiteral("set_marker_color")) {
    if (hasOnlyFields(operation, {"type", "markerId", "color"}) && requiredString("markerId")) return true;
  } else if (type == QStringLiteral("close_gap")) {
    if (hasOnlyFields(operation, {"type", "track", "startFrame", "endFrame"}) && requiredString("track") && operation.value("startFrame").isDouble() && operation.value("endFrame").isDouble()) return true;
  }
  fail(error, QStringLiteral("ActionPlan operation fields are invalid"));
  return false;
}
}

std::optional<ActionPlan> ActionPlan::parse(const QJsonObject& object, QString* error) {
  static const QSet<QString> allowed = {"schemaVersion", "requestId", "baseProjectRevision", "operations"};
  for (auto it = object.begin(); it != object.end(); ++it) {
    if (!allowed.contains(it.key())) { fail(error, QStringLiteral("ActionPlan field is not allowed: %1").arg(it.key())); return std::nullopt; }
  }
  ActionPlan plan;
  if (!string(object, "schemaVersion", plan.schemaVersion) || !string(object, "requestId", plan.requestId) ||
      !object.value("baseProjectRevision").isDouble() || !object.value("operations").isArray()) {
    fail(error, QStringLiteral("ActionPlan has missing or invalid required fields")); return std::nullopt;
  }
  if (plan.schemaVersion != QStringLiteral("orbit.bound-action-plan.v2")) {
    fail(error, QStringLiteral("ActionPlan schema version is unsupported")); return std::nullopt;
  }
  plan.baseProjectRevision = static_cast<qint64>(object.value("baseProjectRevision").toDouble());
  plan.operations = object.value("operations").toArray();
  if (plan.operations.isEmpty()) { fail(error, QStringLiteral("ActionPlan operations must not be empty")); return std::nullopt; }
  for (const auto& item : plan.operations) {
    if (!item.isObject() || !item.toObject().value("type").isString() || !knownOperation(item.toObject().value("type").toString()) ||
        !validOperationShape(item.toObject(), error)) {
      fail(error, QStringLiteral("ActionPlan contains unsupported operation")); return std::nullopt;
    }
  }
  return plan;
}

bool ActionPlan::validate(const ProjectSnapshot& project, QString* error) const {
  if (schemaVersion != QStringLiteral("orbit.bound-action-plan.v2")) { fail(error, QStringLiteral("ActionPlan schema version is unsupported")); return false; }
  if (baseProjectRevision != project.revision) { fail(error, QStringLiteral("ActionPlan project revision is stale")); return false; }
  for (const auto& item : operations) {
    const auto operation = item.toObject();
    const auto type = operation.value("type").toString();
    const auto targetId = operation.value("targetId").toString();
    if (!targetId.isEmpty() && !project.knownTargetIds.contains(targetId)) { fail(error, QStringLiteral("ActionPlan target does not exist")); return false; }
    const auto resourceId = operation.value("resourceId").toString();
    if (!resourceId.isEmpty() && !project.verifiedResourceIds.contains(resourceId)) { fail(error, QStringLiteral("ActionPlan resource is not verified")); return false; }
    const auto mediaId = operation.value("mediaId").toString();
    if (!mediaId.isEmpty() && !project.knownMediaIds.contains(mediaId)) { fail(error, QStringLiteral("ActionPlan media is not in the project")); return false; }
  }
  return true;
}
}  // namespace edward::ai
