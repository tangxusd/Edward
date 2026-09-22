#include <edward/ai/ai_orchestrator.hpp>
#include <cassert>
int main() {
  edward::ai::AiOrchestrator orchestrator;
  edward::ai::ProjectSnapshot project{2, {"clip"}, {}};
  const char* plan = R"({"schemaVersion":"edward.action-plan.v1","requestId":"a","baseProjectRevision":2,"operations":[{"type":"move_clip","targetId":"clip","timelineStart":0}]})";
  assert(orchestrator.handle("解释", project).kind == edward::ai::AiResult::Kind::Conversation);
  assert(orchestrator.handle("{\"react\":\"x\"}", project).kind == edward::ai::AiResult::Kind::Unsupported);
  assert(orchestrator.handle(plan, project).kind == edward::ai::AiResult::Kind::ActionPlan);
  assert(orchestrator.handle(QStringLiteral("```json\n%1\n```").arg(QString::fromUtf8(plan)), project).kind == edward::ai::AiResult::Kind::ActionPlan);
}
