#include <edward/ai/capability_registry.hpp>

#include <QJsonDocument>

#include <cassert>

using edward::ai::CapabilityContract;
using edward::ai::CapabilityRegistry;
using edward::ai::RuntimeFacts;

namespace {
CapabilityContract contract(QString id, QString executor = QStringLiteral("executor"),
                            QString verifier = QStringLiteral("verify")) {
  return {std::move(id), QStringLiteral("1"), QJsonObject{{"type", "object"}},
          {QStringLiteral("clip")}, QStringLiteral("project.write"), QStringLiteral("transaction"),
          QStringLiteral("fail"), QStringLiteral("specified"), QStringLiteral("preserve"),
          QStringLiteral("synchronous"), std::move(verifier), std::move(executor), {}};
}

RuntimeFacts facts() {
  return {{QStringLiteral("timeline.executor")}, {QStringLiteral("timeline.visible")}, {}, 3};
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
                         "taskPolicy", "verificationAdapter"}) {
    assert(first.contains(QLatin1String(key)));
  }
}
}  // namespace

int main() {
  testStableSnapshotHash();
  testRegistryRejectsDuplicateAndMissingRuntimeParts();
  testReplacementCyclesAreRejected();
  testModelViewContainsRequiredContractFields();
}
