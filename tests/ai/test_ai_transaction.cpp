#include <edward/ai/ai_transaction.hpp>

#include <cassert>

namespace {
edward::ai::ActionPlan action(qint64 revision, const QJsonArray& operations) {
  auto parsed = edward::ai::ActionPlan::parse({{"schemaVersion", "edward.action-plan.v1"}, {"requestId", "r"}, {"baseProjectRevision", revision}, {"operations", operations}});
  assert(parsed); return *parsed;
}
void testRollbackAndUndoDepth() {
  edward::ai::ProjectState state{0, {}}; edward::ai::AiTransaction transaction;
  const auto broken = action(0, QJsonArray{QJsonObject{{"type", "insert_native_component"}, {"id", "one"}}, QJsonObject{{"type", "insert_native_component"}, {"id", "one"}}});
  assert(!transaction.apply(broken, state).ok && state.objects.isEmpty());
  for (int index = 0; index < 6; ++index) {
    const auto plan = action(state.revision, QJsonArray{QJsonObject{{"type", "insert_native_component"}, {"id", QStringLiteral("id%1").arg(index)}}});
    assert(transaction.apply(plan, state).ok);
  }
  assert(transaction.undoDepth() == 5);
  assert(transaction.undoLast(state).ok);
}
void testRejectsExportCollision() {
  edward::ai::ProjectState state{0, {}}; edward::ai::AiTransaction transaction;
  const auto plan = action(0, QJsonArray{QJsonObject{{"type", "export_timeline"}, {"explicitUserRequest", true}, {"outputPath", "/"}}});
  assert(!transaction.apply(plan, state).ok);
}
}
int main() { testRollbackAndUndoDepth(); testRejectsExportCollision(); }
