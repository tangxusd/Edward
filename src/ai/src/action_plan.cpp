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
  static const QSet<QString> allowed = {"insert_native_component", "insert_resource_component", "set_component_props", "move_clip", "resize_clip", "set_keyframes", "remove_clip", "render_component", "export_timeline"};
  return allowed.contains(value);
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
  plan.baseProjectRevision = static_cast<qint64>(object.value("baseProjectRevision").toDouble());
  plan.operations = object.value("operations").toArray();
  if (plan.operations.isEmpty()) { fail(error, QStringLiteral("ActionPlan operations must not be empty")); return std::nullopt; }
  for (const auto& item : plan.operations) {
    if (!item.isObject() || !item.toObject().value("type").isString() || !knownOperation(item.toObject().value("type").toString())) {
      fail(error, QStringLiteral("ActionPlan contains unsupported operation")); return std::nullopt;
    }
  }
  return plan;
}

bool ActionPlan::validate(const ProjectSnapshot& project, QString* error) const {
  if (schemaVersion != QStringLiteral("edward.action-plan.v1")) { fail(error, QStringLiteral("ActionPlan schema version is unsupported")); return false; }
  if (baseProjectRevision != project.revision) { fail(error, QStringLiteral("ActionPlan project revision is stale")); return false; }
  for (const auto& item : operations) {
    const auto operation = item.toObject();
    const auto type = operation.value("type").toString();
    const auto targetId = operation.value("targetId").toString();
    if (!targetId.isEmpty() && !project.knownTargetIds.contains(targetId)) { fail(error, QStringLiteral("ActionPlan target does not exist")); return false; }
    const auto resourceId = operation.value("resourceId").toString();
    if (!resourceId.isEmpty() && !project.verifiedResourceIds.contains(resourceId)) { fail(error, QStringLiteral("ActionPlan resource is not verified")); return false; }
    if (type == QStringLiteral("export_timeline") && !operation.value("explicitUserRequest").toBool()) { fail(error, QStringLiteral("export requires explicit user request")); return false; }
  }
  return true;
}
}  // namespace edward::ai
