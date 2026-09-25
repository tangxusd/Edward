#include <edward/ai/capability_registry.hpp>

#include <QJsonDocument>

#include <cassert>

using edward::ai::CapabilityContract;
using edward::ai::CapabilityRegistry;
using edward::ai::RuntimeFacts;

namespace {
CapabilityContract contract(QString id, QString executor = QStringLiteral("executor"),
                            QString verifier = QStringLiteral("verify")) {
  CapabilityContract value;
  value.id = std::move(id);
  value.version = QStringLiteral("1");
  value.inputSchema = QJsonObject{{"type", "object"}};
  value.targetTypes = {QStringLiteral("clip")};
  value.permissionCategory = QStringLiteral("project.write");
  value.undoScope = QStringLiteral("transaction");
  value.collisionPolicy = QStringLiteral("fail");
  value.trackPlacementPolicy = QStringLiteral("specified");
  value.linkedMediaPolicy = QStringLiteral("preserve");
  value.taskPolicy = QStringLiteral("synchronous");
  value.executionMode = QStringLiteral("local_transaction");
  value.reversibility = QStringLiteral("undoable");
  value.allowedPolicies = {QStringLiteral("collision")};
  value.markerPolicy = QStringLiteral("preserve");
  value.validate = QStringLiteral("validate");
  value.execute = QStringLiteral("execute");
  value.postconditions = {QStringLiteral("verified")};
  value.preview = QStringLiteral("preview");
  value.render = QStringLiteral("render");
  value.verificationAdapter = std::move(verifier);
  value.executor = std::move(executor);
  return value;
}

RuntimeFacts facts() {
  return {{QStringLiteral("timeline.executor")}, {QStringLiteral("timeline.visible")},
          {QStringLiteral("playhead"), QStringLiteral("selected_component"), QStringLiteral("selected_clip"),
           QStringLiteral("selected_audio"), QStringLiteral("timeline"), QStringLiteral("selected_clip"),
           QStringLiteral("marker"), QStringLiteral("track")}, 3};
}

void testStableSnapshotHash() {
  const auto registry = CapabilityRegistry::builtIn();
  const auto first = registry.snapshot(facts());
  const auto second = registry.snapshot(facts());
  assert(first.valid);
  assert(first.hash == second.hash);
  assert(first.modelCapabilities == second.modelCapabilities);
}

void testRegistryRejectsDuplicateAndMissingRuntimeParts() {
  CapabilityRegistry duplicate({contract(QStringLiteral("same")), contract(QStringLiteral("same"))});
  assert(!duplicate.isValid());
  assert(duplicate.validationError().contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));

  CapabilityRegistry missingExecutor({contract(QStringLiteral("missing"), {})});
  assert(!missingExecutor.isValid());
  CapabilityRegistry missingVerifier({contract(QStringLiteral("missing"), QStringLiteral("executor"), {})});
  assert(!missingVerifier.isValid());
}

void testReplacementCyclesAreRejected() {
  auto a = contract(QStringLiteral("a"));
  auto b = contract(QStringLiteral("b"));
  a.replacementOf = {QStringLiteral("b")};
  b.replacementOf = {QStringLiteral("a")};
  CapabilityRegistry registry({a, b});
  assert(!registry.isValid());
  assert(registry.validationError().contains(QStringLiteral("cycle"), Qt::CaseInsensitive));
}

void testModelViewContainsRequiredContractFields() {
  const auto registry = CapabilityRegistry::builtIn();
  const auto snapshot = registry.snapshot(facts());
  assert(snapshot.valid);
  const auto view = registry.modelView(snapshot);
  assert(!view.isEmpty());
  const auto first = view.at(0).toObject();
  for (const auto key : {"id", "version", "inputSchema", "targetTypes", "permissionCategory",
                         "undoScope", "collisionPolicy", "trackPlacementPolicy", "linkedMediaPolicy",
                         "taskPolicy", "executionMode", "reversibility", "allowedPolicies", "markerPolicy",
                         "validate", "execute", "postconditions", "preview", "render", "verificationAdapter"}) {
    assert(first.contains(QLatin1String(key)));
  }
}

void testRuntimeTargetTypesAreRequired() {
  auto runtime = facts();
  runtime.targetTypes = {QStringLiteral("clip")};
  const auto snapshot = CapabilityRegistry::builtIn().snapshot(runtime);
  assert(!snapshot.valid);
  assert(snapshot.error.contains(QStringLiteral("target type"), Qt::CaseInsensitive));
}
}  // namespace

int main() {
  testStableSnapshotHash();
  testRegistryRejectsDuplicateAndMissingRuntimeParts();
  testReplacementCyclesAreRejected();
  testModelViewContainsRequiredContractFields();
  testRuntimeTargetTypesAreRequired();
}
