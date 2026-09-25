#include <edward/ai/ai_transaction.hpp>

#include <cassert>

namespace {
edward::ai::ActionPlan action(qint64 revision, const QString& capability, const QString& targetId, const QJsonObject& args) {
  const auto requestId = QStringLiteral("r-%1").arg(args.value(QStringLiteral("props")).toObject().value(QStringLiteral("index")).toInt(-1));
  const QJsonArray boundOperations{QJsonObject{
      {"operationId", "op-1"}, {"capability", capability},
      {"target", QJsonObject{{"kind", "clip"}, {"id", targetId}, {"resolvedFrom", "selected_clip"}}},
      {"args", args}, {"policies", QJsonObject{{"collision", "fail"}}}, {"dependsOn", QJsonArray{}},
      {"preconditions", QJsonArray{QJsonObject{{"kind", "object_version"}}}},
      {"readSet", QJsonArray{QStringLiteral("clip:%1").arg(targetId)}},
      {"writeSet", QJsonArray{QStringLiteral("clip:%1").arg(targetId)}},
      {"resolutionEvidence", QJsonObject{{"referenceSnapshotId", "ref-1"}}}}};
  auto parsed = edward::ai::ActionPlan::parse({{"schemaVersion", "orbit.bound-action-plan.v2"}, {"requestId", requestId},
                                                {"baseProjectRevision", revision}, {"referenceSnapshotId", "ref-1"},
                                                {"capabilitySet", QJsonObject{{"version", 3}, {"hash", "sha256:test"}}},
                                                {"operations", boundOperations}});
  assert(parsed); return *parsed;
}
void testRollbackAndUndoDepth() {
  edward::ai::ProjectState state{0, {{"target", QJsonObject{}}}, 3, "sha256:test", "ref-1"}; edward::ai::AiTransaction transaction;
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

void testMultiOperationReceiptAndJournalRoundTrip() {
  const QJsonArray operations{
      QJsonObject{{"operationId", "op-a"}, {"capability", "component.set_props"},
                  {"target", QJsonObject{{"kind", "clip"}, {"id", "a"}, {"resolvedFrom", "selected_clip"}}},
                  {"args", QJsonObject{{"props", QJsonObject{{"color", "red"}}}}},
                  {"policies", QJsonObject{{"collision", "fail"}}}, {"dependsOn", QJsonArray{}},
                  {"preconditions", QJsonArray{QJsonObject{{"kind", "object_version"}}}},
                  {"readSet", QJsonArray{"clip:a"}}, {"writeSet", QJsonArray{"clip:a"}},
                  {"resolutionEvidence", QJsonObject{{"referenceSnapshotId", "ref-1"}}}},
      QJsonObject{{"operationId", "op-b"}, {"capability", "component.set_props"},
                  {"target", QJsonObject{{"kind", "clip"}, {"id", "b"}, {"resolvedFrom", "selected_clip"}}},
                  {"args", QJsonObject{{"props", QJsonObject{{"opacity", 0.5}}}}},
                  {"policies", QJsonObject{{"collision", "fail"}}}, {"dependsOn", QJsonArray{}},
                  {"preconditions", QJsonArray{QJsonObject{{"kind", "object_version"}}}},
                  {"readSet", QJsonArray{"clip:b"}}, {"writeSet", QJsonArray{"clip:b"}},
                  {"resolutionEvidence", QJsonObject{{"referenceSnapshotId", "ref-1"}}}}};
  const auto parsed = edward::ai::ActionPlan::parse({{"schemaVersion", "orbit.bound-action-plan.v2"}, {"requestId", "multi"},
                                                      {"baseProjectRevision", 0}, {"referenceSnapshotId", "ref-1"},
                                                      {"capabilitySet", QJsonObject{{"version", 3}, {"hash", "sha256:test"}}},
                                                      {"operations", operations}});
  assert(parsed);
  edward::ai::ProjectState state{0, {{"a", QJsonObject{}}, {"b", QJsonObject{}}}, 3, "sha256:test", "ref-1"};
  edward::ai::AiTransaction transaction;
  const auto applied = transaction.apply(*parsed, state);
  assert(applied.ok && applied.receipt.operationCount == 2 && applied.receipt.revisionBefore == 0 && applied.receipt.revisionAfter == 1);
  assert(applied.receipt.readSet == QStringList({"clip:a", "clip:b"}));
  assert(applied.receipt.writeSet == QStringList({"clip:a", "clip:b"}));
  assert(transaction.undoDepth() == 1 && transaction.redoDepth() == 0);
  assert(state.objects.value("a").toObject().value("color").toString() == "red");
  assert(state.objects.value("b").toObject().value("opacity").toDouble() == 0.5);

  assert(transaction.undoLast(state).ok);
  assert(state.revision == 2 && state.objects.value("a").toObject().isEmpty() && state.objects.value("b").toObject().isEmpty());
  assert(transaction.redo(state).ok);
  assert(state.revision == 3 && state.objects.value("a").toObject().value("color").toString() == "red");
  assert(state.objects.value("b").toObject().value("opacity").toDouble() == 0.5);
}
}
int main() { testRollbackAndUndoDepth(); testRejectsExportCollision(); testMultiOperationReceiptAndJournalRoundTrip(); }
