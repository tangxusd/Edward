#include <edward/ai/ai_transaction.hpp>

#include <cassert>

namespace {
edward::ai::ActionPlan action(qint64 revision, const QString& capability, const QString& targetId, const QJsonObject& args) {
  const QJsonArray boundOperations{QJsonObject{
      {"operationId", "op-1"}, {"capability", capability},
      {"target", QJsonObject{{"kind", "clip"}, {"id", targetId}, {"resolvedFrom", "selected_clip"}}},
      {"args", args}, {"policies", QJsonObject{{"collision", "fail"}}}, {"dependsOn", QJsonArray{}},
      {"preconditions", QJsonArray{QJsonObject{{"kind", "object_version"}}}},
      {"readSet", QJsonArray{QStringLiteral("clip:%1").arg(targetId)}},
      {"writeSet", QJsonArray{QStringLiteral("clip:%1").arg(targetId)}},
      {"resolutionEvidence", QJsonObject{{"referenceSnapshotId", "ref-1"}}}}};
  auto parsed = edward::ai::ActionPlan::parse({{"schemaVersion", "orbit.bound-action-plan.v2"}, {"requestId", "r"},
                                                {"baseProjectRevision", revision}, {"referenceSnapshotId", "ref-1"},
                                                {"capabilitySet", QJsonObject{{"version", 3}, {"hash", "sha256:test"}}},
                                                {"operations", boundOperations}});
  assert(parsed); return *parsed;
}
void testRollbackAndUndoDepth() {
  edward::ai::ProjectState state{0, {{"target", QJsonObject{}}}}; edward::ai::AiTransaction transaction;
  const auto broken = action(0, "clip.remove", "missing", {});
  assert(!transaction.apply(broken, state).ok && state.objects.contains("target"));
  for (int index = 0; index < 6; ++index) {
    const auto plan = action(state.revision, "component.set_props", "target", QJsonObject{{"props", QJsonObject{{"index", index}}}});
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
