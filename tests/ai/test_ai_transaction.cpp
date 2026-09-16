#include <edward/ai/ai_transaction.hpp>

#include <cassert>

namespace {
edward::ai::ActionPlan action(qint64 revision, const QJsonArray& operations) {
  auto parsed = edward::ai::ActionPlan::parse({{"schemaVersion", "edward.action-plan.v1"}, {"requestId", "r"}, {"baseProjectRevision", revision}, {"operations", operations}});
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
  assert(transaction.undoDepth() == 5);
  assert(transaction.undoLast(state).ok);
}
void testRejectsExportCollision() {
  QString error;
  const auto plan = edward::ai::ActionPlan::parse({{"schemaVersion", "edward.action-plan.v1"}, {"requestId", "r"}, {"baseProjectRevision", 0}, {"operations", QJsonArray{QJsonObject{{"type", "export_timeline"}}}}}, &error);
  assert(!plan);
}
}
int main() { testRollbackAndUndoDepth(); testRejectsExportCollision(); }
