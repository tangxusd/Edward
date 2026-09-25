#include <edward/ai/ai_transaction.hpp>

#include <cassert>

namespace {
edward::ai::ActionPlan action(qint64 revision, const QJsonArray& operations) {
  QJsonArray boundOperations;
  for (const auto& value : operations) {
    const auto legacy = value.toObject();
    const auto type = legacy.value("type").toString();
    const auto targetId = legacy.value("targetId").toString();
    QString capability;
    QJsonObject args;
    if (type == QStringLiteral("remove_clip")) capability = QStringLiteral("clip.remove");
    else if (type == QStringLiteral("set_component_props")) {
      capability = QStringLiteral("component.set_props");
      args.insert("props", legacy.value("props").toObject());
    }
    boundOperations.append(QJsonObject{
        {"operationId", QStringLiteral("op-%1").arg(boundOperations.size() + 1)}, {"capability", capability},
        {"target", QJsonObject{{"kind", "clip"}, {"id", targetId}, {"resolvedFrom", "selected_clip"}}},
        {"args", args}, {"policies", QJsonObject{{"collision", "fail"}}}, {"dependsOn", QJsonArray{}},
        {"preconditions", QJsonArray{QJsonObject{{"kind", "object_version"}}}},
        {"readSet", QJsonArray{QStringLiteral("clip:%1").arg(targetId)}},
        {"writeSet", QJsonArray{QStringLiteral("clip:%1").arg(targetId)}},
        {"resolutionEvidence", QJsonObject{{"referenceSnapshotId", "ref-1"}}}});
  }
  auto parsed = edward::ai::ActionPlan::parse({{"schemaVersion", "orbit.bound-action-plan.v2"}, {"requestId", "r"},
                                                {"baseProjectRevision", revision}, {"referenceSnapshotId", "ref-1"},
                                                {"capabilitySet", QJsonObject{{"version", 3}, {"hash", "sha256:test"}}},
                                                {"operations", boundOperations}});
  assert(parsed); return *parsed;
}
void testRollbackAndUndoDepth() {
  edward::ai::ProjectState state{0, {{"target", QJsonObject{}}}}; edward::ai::AiTransaction transaction;
  const auto broken = action(0, QJsonArray{QJsonObject{{"type", "remove_clip"}, {"targetId", "missing"}}});
  assert(!transaction.apply(broken, state).ok && state.objects.contains("target"));
  for (int index = 0; index < 6; ++index) {
    const auto plan = action(state.revision, QJsonArray{QJsonObject{{"type", "set_component_props"}, {"targetId", "target"}, {"props", QJsonObject{{"index", index}}}}});
    assert(transaction.apply(plan, state).ok);
  }
  assert(transaction.undoDepth() == 6);
  assert(transaction.undoLast(state).ok);
}
void testRejectsExportCollision() {
  QString error;
  const auto plan = edward::ai::ActionPlan::parse({{"schemaVersion", "orbit.bound-action-plan.v2"}, {"requestId", "r"}, {"baseProjectRevision", 0}, {"operations", QJsonArray{QJsonObject{{"type", "export_timeline"}}}}}, &error);
  assert(!plan);
}
}
int main() { testRollbackAndUndoDepth(); testRejectsExportCollision(); }
