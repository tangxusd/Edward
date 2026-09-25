#include "edward/ai/action_plan.hpp"

#include <QHash>
#include <QSet>

#include <functional>

namespace edward::ai {
namespace {

void fail(QString* error, const QString& message) {
  if (error) *error = message;
}

bool nonEmptyString(const QJsonValue& value) { return value.isString() && !value.toString().isEmpty(); }

bool onlyFields(const QJsonObject& object, const QSet<QString>& allowed) {
  for (auto it = object.begin(); it != object.end(); ++it)
    if (!allowed.contains(it.key())) return false;
  return true;
}

bool stringArray(const QJsonValue& value) {
  if (!value.isArray()) return false;
  for (const auto& item : value.toArray())
    if (!nonEmptyString(item)) return false;
  return true;
}

bool objectArray(const QJsonValue& value) {
  if (!value.isArray()) return false;
  for (const auto& item : value.toArray())
    if (!item.isObject()) return false;
  return true;
}

bool nonEmptyObjectArray(const QJsonValue& value) { return objectArray(value) && !value.toArray().isEmpty(); }

bool nonEmptyStringArray(const QJsonValue& value) { return stringArray(value) && !value.toArray().isEmpty(); }

bool validCapabilitySet(const QJsonValue& value) {
  if (!value.isObject()) return false;
  const auto capabilitySet = value.toObject();
  if (!onlyFields(capabilitySet, {QStringLiteral("version"), QStringLiteral("hash")})) return false;
  const auto version = capabilitySet.value(QStringLiteral("version"));
  return version.isDouble() && version.toDouble() > 0 && version.toDouble() == version.toInteger() &&
         nonEmptyString(capabilitySet.value(QStringLiteral("hash")));
}

bool validBoundOperation(const QJsonValue& value, QString* error) {
  if (!value.isObject()) {
    fail(error, QStringLiteral("BoundActionPlan operation must be an object"));
    return false;
  }
  const auto operation = value.toObject();
  static const QSet<QString> allowed = {
      QStringLiteral("operationId"), QStringLiteral("capability"), QStringLiteral("target"),
      QStringLiteral("args"), QStringLiteral("policies"), QStringLiteral("dependsOn"),
      QStringLiteral("preconditions"), QStringLiteral("readSet"), QStringLiteral("writeSet"),
      QStringLiteral("resolutionEvidence")};
  if (!onlyFields(operation, allowed) || !nonEmptyString(operation.value(QStringLiteral("operationId"))) ||
      !nonEmptyString(operation.value(QStringLiteral("capability"))) || !operation.value(QStringLiteral("target")).isObject() ||
      !operation.value(QStringLiteral("args")).isObject() || operation.value(QStringLiteral("policies")).toObject().isEmpty() ||
      !stringArray(operation.value(QStringLiteral("dependsOn"))) || !nonEmptyObjectArray(operation.value(QStringLiteral("preconditions"))) ||
      !nonEmptyStringArray(operation.value(QStringLiteral("readSet"))) || !nonEmptyStringArray(operation.value(QStringLiteral("writeSet"))) ||
      !operation.value(QStringLiteral("resolutionEvidence")).isObject()) {
    fail(error, QStringLiteral("BoundActionPlan operation fields are invalid"));
    return false;
  }
  const auto target = operation.value(QStringLiteral("target")).toObject();
  if (!onlyFields(target, {QStringLiteral("kind"), QStringLiteral("id"), QStringLiteral("resolvedFrom")})) {
    fail(error, QStringLiteral("BoundActionPlan target fields are invalid"));
    return false;
  }
  if (!nonEmptyString(target.value(QStringLiteral("kind"))) || !nonEmptyString(target.value(QStringLiteral("id"))) ||
      !nonEmptyString(target.value(QStringLiteral("resolvedFrom")))) {
    fail(error, QStringLiteral("BoundActionPlan target is incomplete"));
    return false;
  }
  const auto evidence = operation.value(QStringLiteral("resolutionEvidence")).toObject();
  if (!evidence.contains(QStringLiteral("referenceSnapshotId")) ||
      !nonEmptyString(evidence.value(QStringLiteral("referenceSnapshotId")))) {
    fail(error, QStringLiteral("BoundActionPlan resolution evidence is incomplete"));
    return false;
  }
  return true;
}

bool validDependencyGraph(const QJsonArray& operations, QString* error) {
  QHash<QString, QJsonArray> dependencies;
  for (const auto& value : operations) {
    const auto operation = value.toObject();
    const auto operationId = operation.value(QStringLiteral("operationId")).toString();
    if (dependencies.contains(operationId)) {
      fail(error, QStringLiteral("BoundActionPlan operationId is duplicated"));
      return false;
    }
    dependencies.insert(operationId, operation.value(QStringLiteral("dependsOn")).toArray());
  }
  QSet<QString> visiting;
  QSet<QString> visited;
  std::function<bool(const QString&)> visit = [&](const QString& id) {
    if (visiting.contains(id)) return false;
    if (visited.contains(id)) return true;
    if (!dependencies.contains(id)) return false;
    visiting.insert(id);
    for (const auto& item : dependencies.value(id)) {
      if (!visit(item.toString())) return false;
    }
    visiting.remove(id);
    visited.insert(id);
    return true;
  };
  for (auto it = dependencies.cbegin(); it != dependencies.cend(); ++it) {
    if (!visit(it.key())) {
      fail(error, QStringLiteral("BoundActionPlan dependency graph is invalid"));
      return false;
    }
  }
  return true;
}

bool validBoundPlan(const ActionPlan& plan, QString* error) {
  if (plan.schemaVersion != QStringLiteral("orbit.bound-action-plan.v2") || plan.requestId.isEmpty() ||
      plan.baseProjectRevision < 0 || plan.referenceSnapshotId.isEmpty() || !validCapabilitySet(plan.capabilitySet) ||
      plan.operations.isEmpty()) {
    fail(error, QStringLiteral("BoundActionPlan identity or capability set is invalid"));
    return false;
  }
  for (const auto& value : plan.operations)
    if (!validBoundOperation(value, error)) return false;
  return validDependencyGraph(plan.operations, error);
}

}  // namespace

std::optional<ActionPlan> ActionPlan::parse(const QJsonObject& object, QString* error) {
  static const QSet<QString> allowed = {QStringLiteral("schemaVersion"), QStringLiteral("requestId"),
                                        QStringLiteral("baseProjectRevision"), QStringLiteral("referenceSnapshotId"),
                                        QStringLiteral("capabilitySet"), QStringLiteral("operations")};
  if (!onlyFields(object, allowed)) {
    fail(error, QStringLiteral("BoundActionPlan field is not allowed"));
    return std::nullopt;
  }
  ActionPlan plan;
  plan.schemaVersion = object.value(QStringLiteral("schemaVersion")).toString();
  plan.requestId = object.value(QStringLiteral("requestId")).toString();
  plan.baseProjectRevision = object.value(QStringLiteral("baseProjectRevision")).toInteger(-1);
  plan.referenceSnapshotId = object.value(QStringLiteral("referenceSnapshotId")).toString();
  plan.capabilitySet = object.value(QStringLiteral("capabilitySet")).toObject();
  plan.operations = object.value(QStringLiteral("operations")).toArray();
  if (!object.value(QStringLiteral("baseProjectRevision")).isDouble() ||
      object.value(QStringLiteral("baseProjectRevision")).toDouble() != plan.baseProjectRevision ||
      !validBoundPlan(plan, error)) {
    if (error && error->isEmpty()) fail(error, QStringLiteral("BoundActionPlan is invalid"));
    return std::nullopt;
  }
  return plan;
}

bool ActionPlan::validate(const ProjectSnapshot& project, QString* error) const {
  if (!validBoundPlan(*this, error)) return false;
  if (baseProjectRevision != project.revision) {
    fail(error, QStringLiteral("ActionPlan project revision is stale"));
    return false;
  }
  const auto version = capabilitySet.value(QStringLiteral("version")).toInteger(-1);
  const auto hash = capabilitySet.value(QStringLiteral("hash")).toString();
  if (project.capabilitySetVersion >= 0 && version != project.capabilitySetVersion) {
    fail(error, QStringLiteral("ActionPlan capability set version is stale"));
    return false;
  }
  if (!project.capabilitySetHash.isEmpty() && hash != project.capabilitySetHash) {
    fail(error, QStringLiteral("ActionPlan capability set hash is stale"));
    return false;
  }
  if (!project.referenceSnapshotId.isEmpty() && referenceSnapshotId != project.referenceSnapshotId) {
    fail(error, QStringLiteral("ActionPlan reference snapshot is stale"));
    return false;
  }
  for (const auto& item : operations) {
    const auto operation = item.toObject();
    const auto targetId = operation.value(QStringLiteral("target")).toObject().value(QStringLiteral("id")).toString();
    if (!project.knownTargetIds.contains(targetId)) {
      fail(error, QStringLiteral("ActionPlan target does not exist"));
      return false;
    }
    const auto args = operation.value(QStringLiteral("args")).toObject();
    const auto resourceId = args.value(QStringLiteral("resourceId")).toString();
    if (!resourceId.isEmpty() && !project.verifiedResourceIds.contains(resourceId)) {
      fail(error, QStringLiteral("ActionPlan resource is not verified"));
      return false;
    }
    const auto mediaId = args.value(QStringLiteral("mediaId")).toString();
    if (!mediaId.isEmpty() && !project.knownMediaIds.contains(mediaId)) {
      fail(error, QStringLiteral("ActionPlan media is not in the project"));
      return false;
    }
  }
  return true;
}

}  // namespace edward::ai
