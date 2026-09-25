#include <edward/ai/action_plan.hpp>
#include <edward/ai/intent_plan.hpp>

#include <cassert>

namespace {
QJsonObject plan(QJsonObject operation = {{"type", "move_clip"}, {"targetId", "clip-1"}, {"timelineStart", 0}}) {
  return {{"schemaVersion", "orbit.bound-action-plan.v2"}, {"requestId", "request-1"}, {"baseProjectRevision", 3},
          {"referenceSnapshotId", "ref-1"}, {"capabilitySet", QJsonObject{{"version", 3}, {"hash", "sha256:test"}}},
          {"operations", QJsonArray{operation}}};
}
QJsonObject boundOperation(const QString& operationId = QStringLiteral("op-1")) {
  return {{"operationId", operationId}, {"capability", "clip.set_range"},
          {"target", QJsonObject{{"kind", "clip"}, {"id", "clip-1"}, {"resolvedFrom", "selected_clip"}}},
          {"args", QJsonObject{{"durationFrames", 56}}},
          {"policies", QJsonObject{{"collision", "fail"}, {"trackPlacement", "specified"}, {"linkedMedia", "preserve"}, {"marker", "preserve"}}},
          {"dependsOn", QJsonArray{}}, {"preconditions", QJsonArray{QJsonObject{{"kind", "object_version"}}}},
          {"readSet", QJsonArray{"clip:clip-1"}}, {"writeSet", QJsonArray{"clip:clip-1"}},
          {"resolutionEvidence", QJsonObject{{"referenceSnapshotId", "ref-1"}}}};
}
void testRejectsUnknownOperationAndUnexpectedPayload() {
  QString error;
  auto invalid = plan({{"type", "unknown_operation"}});
  assert(!edward::ai::ActionPlan::parse(invalid, &error));
  invalid = plan(); invalid.insert("unexpectedPayload", QJsonObject{});
  assert(!edward::ai::ActionPlan::parse(invalid, &error));
  invalid = plan(); invalid.insert("schemaVersion", "edward.action-plan.v1");
  assert(!edward::ai::ActionPlan::parse(invalid, &error));
}
void testRejectsStaleRevision() {
  const auto parsed = edward::ai::ActionPlan::parse(plan(boundOperation())); assert(parsed);
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

QJsonObject intent(QJsonObject operation = {
    {"capability", "clip.set_range"},
    {"targetRef", QJsonObject{{"selector", "selected_clip"}}},
    {"arguments", QJsonObject{{"endAt", QJsonObject{{"markerScope", "timeline"}, {"label", 3}}}}}}) {
  return {{"schemaVersion", "edward.intent-plan.v1"}, {"requestId", "intent-1"}, {"intents", QJsonArray{operation}}};
}

void testIntentRejectsInternalFactsAndUnknownFields() {
  QString error;
  auto invalid = intent();
  invalid["intents"] = QJsonArray{QJsonObject{
      {"capability", "clip.set_range"},
      {"targetRef", QJsonObject{{"selector", "selected_clip"}}},
      {"arguments", QJsonObject{{"targetId", "c_123"}}}}};
  assert(!edward::ai::IntentPlan::parse(invalid, &error));
  assert(error.startsWith("MODEL_OUTPUT_UNTRUSTED"));

  invalid = intent();
  invalid.insert("baseProjectRevision", 42);
  assert(!edward::ai::IntentPlan::parse(invalid, &error));
  assert(error.startsWith("MODEL_OUTPUT_UNTRUSTED"));

  invalid = intent();
  invalid["intents"] = QJsonArray{QJsonObject{
      {"capability", "clip.set_range"},
      {"targetRef", QJsonObject{{"selector", "selected_clip"}}},
      {"arguments", QJsonObject{{"sourcePath", "/Users/test/clip.mov"}}}}};
  assert(!edward::ai::IntentPlan::parse(invalid, &error));
  assert(error.startsWith("MODEL_OUTPUT_UNTRUSTED"));

  invalid = intent();
  invalid["intents"] = QJsonArray{QJsonObject{
      {"capability", "clip.set_range"},
      {"targetRef", QJsonObject{{"selector", "selected_clip"}}},
      {"arguments", QJsonObject{}},
      {"reason", "read /Users/test/clip.mov"}}};
  assert(!edward::ai::IntentPlan::parse(invalid, &error));
  assert(error.startsWith("MODEL_OUTPUT_UNTRUSTED"));

  invalid = intent();
  invalid["intents"] = QJsonArray{QJsonObject{
      {"capability", "clip.set_range"},
      {"targetRef", QJsonObject{{"selector", "selected_clip"}}},
      {"arguments", QJsonObject{}},
      {"unexpected", true}}};
  assert(!edward::ai::IntentPlan::parse(invalid, &error));
  assert(error.startsWith("MODEL_OUTPUT_UNTRUSTED"));

  const auto parsed = edward::ai::IntentPlan::parse(intent(), &error);
  assert(parsed);
  edward::ai::CapabilityView capabilities{{"media.insert"}};
  assert(!parsed->validate(capabilities, &error));
  assert(error.startsWith("INTENT_INVALID"));

  invalid = intent();
  QJsonArray tooMany;
  for (int i = 0; i < 33; ++i) tooMany.append(intent().value("intents").toArray().first());
  invalid["intents"] = tooMany;
  assert(!edward::ai::IntentPlan::parse(invalid, &error));
  assert(error.startsWith("MODEL_OUTPUT_UNTRUSTED"));
}

void testIntentAcceptsSymbolicReferences() {
  QString error;
  for (const auto& selector : {QStringLiteral("selected_clip"), QStringLiteral("playhead_clip"), QStringLiteral("global_marker(3)"), QStringLiteral("global_marker")}) {
    QJsonObject ref{{"selector", selector}};
    if (selector == QStringLiteral("global_marker")) ref.insert("label", 3);
    auto object = intent({{"capability", "clip.set_range"}, {"targetRef", ref}, {"arguments", QJsonObject{}}});
    const auto parsed = edward::ai::IntentPlan::parse(object, &error);
    assert(parsed);
    edward::ai::CapabilityView capabilities{{"clip.set_range"}};
    assert(parsed->validate(capabilities, &error));
  }
}

void testIntentValidateRepeatsSafetyChecks() {
  QString error;
  edward::ai::IntentPlan forged{"edward.intent-plan.v1", "forged", QJsonArray{QJsonObject{
      {"capability", "clip.set_range"}, {"targetRef", QJsonObject{{"selector", "selected_clip"}}},
      {"arguments", QJsonObject{{"targetId", "internal"}}}}}};
  edward::ai::CapabilityView capabilities{{"clip.set_range"}};
  assert(!forged.validate(capabilities, &error));
  assert(error.startsWith("INTENT_INVALID"));
}

void testIntentRejectsNonIntegerMarkerReferences() {
  QString error;
  auto invalid = intent({{"capability", "clip.set_range"},
                         {"targetRef", QJsonObject{{"selector", "global_marker"}, {"label", 3.5}}},
                         {"arguments", QJsonObject{}}});
  assert(!edward::ai::IntentPlan::parse(invalid, &error));
  invalid = intent({{"capability", "clip.set_range"},
                    {"targetRef", QJsonObject{{"selector", "global_marker(3)"}, {"label", 3}}},
                    {"arguments", QJsonObject{}}});
  assert(!edward::ai::IntentPlan::parse(invalid, &error));
}

void testRejectsFlatOrIncompleteBoundPlans() {
  QString error;
  assert(!edward::ai::ActionPlan::parse(plan({{"type", "move_clip"}, {"targetId", "clip-1"}}), &error));
  auto invalid = plan(boundOperation());
  invalid["operations"] = QJsonArray{QJsonObject{{"operationId", "op-1"}, {"capability", "clip.set_range"}}};
  assert(!edward::ai::ActionPlan::parse(invalid, &error));
  auto cyclic = plan(boundOperation("op-1"));
  auto second = boundOperation("op-2");
  cyclic["operations"] = QJsonArray{QJsonObject(boundOperation("op-1")), QJsonObject(second)};
  auto operations = cyclic.value("operations").toArray();
  auto first = operations.at(0).toObject();
  first["dependsOn"] = QJsonArray{"op-2"};
  operations[0] = first;
  auto secondWithDependency = operations.at(1).toObject();
  secondWithDependency["dependsOn"] = QJsonArray{"op-1"};
  operations[1] = secondWithDependency;
  cyclic["operations"] = operations;
  assert(!edward::ai::ActionPlan::parse(cyclic, &error));
}
}
int main() {
  testRejectsUnknownOperationAndUnexpectedPayload();
  testRejectsStaleRevision();
  testRequiresExplicitExport();
  testRejectsPartialOrUnexpectedOperations();
  testIntentRejectsInternalFactsAndUnknownFields();
  testIntentAcceptsSymbolicReferences();
  testIntentValidateRepeatsSafetyChecks();
  testIntentRejectsNonIntegerMarkerReferences();
  testRejectsFlatOrIncompleteBoundPlans();
}
