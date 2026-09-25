#include <edward/ai/ai_orchestrator.hpp>
#include <cassert>
int main() {
  edward::ai::AiOrchestrator orchestrator;
  edward::ai::ProjectSnapshot project{2, {"clip"}, {}};
  const char* plan = R"({"schemaVersion":"orbit.bound-action-plan.v2","requestId":"a","baseProjectRevision":2,"referenceSnapshotId":"ref-1","capabilitySet":{"version":3,"hash":"sha256:test"},"operations":[{"operationId":"op-1","capability":"clip.set_range","target":{"kind":"clip","id":"clip","resolvedFrom":"selected_clip"},"args":{"durationFrames":1},"policies":{"collision":"fail","trackPlacement":"specified","linkedMedia":"preserve","marker":"preserve"},"dependsOn":[],"preconditions":[{"kind":"object_version"}],"readSet":["clip:clip"],"writeSet":["clip:clip"],"resolutionEvidence":{"referenceSnapshotId":"ref-1"}}]})";
  assert(orchestrator.handle("解释", project).kind == edward::ai::AiResult::Kind::Conversation);
  assert(orchestrator.handle("{\"react\":\"x\"}", project).kind == edward::ai::AiResult::Kind::Unsupported);
  assert(orchestrator.handle(plan, project).kind == edward::ai::AiResult::Kind::Clarification);
  assert(orchestrator.handle(QStringLiteral("```json\n%1\n```").arg(QString::fromUtf8(plan)), project).kind == edward::ai::AiResult::Kind::Clarification);
}
