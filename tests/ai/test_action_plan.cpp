#include <edward/ai/action_plan.hpp>

#include <cassert>

namespace {
QJsonObject plan(QJsonObject operation = {{"type", "move_clip"}, {"targetId", "clip-1"}, {"timelineStart", 0}}) {
  return {{"schemaVersion", "edward.action-plan.v1"}, {"requestId", "request-1"}, {"baseProjectRevision", 3}, {"operations", QJsonArray{operation}}};
}
void testRejectsUnknownOperationAndIr() {
  QString error;
  auto invalid = plan({{"type", "component_ir"}});
  assert(!edward::ai::ActionPlan::parse(invalid, &error));
  invalid = plan(); invalid.insert("componentIr", QJsonObject{});
  assert(!edward::ai::ActionPlan::parse(invalid, &error));
}
void testRejectsStaleRevision() {
  const auto parsed = edward::ai::ActionPlan::parse(plan()); assert(parsed);
  edward::ai::ProjectSnapshot project{4, {"clip-1"}, {}}; QString error;
  assert(!parsed->validate(project, &error));
}
void testRequiresExplicitExport() {
  QString error;
  assert(!edward::ai::ActionPlan::parse(plan({{"type", "export_timeline"}, {"explicitUserRequest", true}}), &error));
}
void testRejectsPartialOrUnexpectedOperations() {
  QString error;
  assert(!edward::ai::ActionPlan::parse(plan({{"type", "move_clip"}, {"targetId", "clip-1"}}), &error));
  assert(!edward::ai::ActionPlan::parse(plan({{"type", "remove_clip"}, {"targetId", "clip-1"}, {"path", "/tmp"}}), &error));
}
}
int main() { testRejectsUnknownOperationAndIr(); testRejectsStaleRevision(); testRequiresExplicitExport(); testRejectsPartialOrUnexpectedOperations(); }
