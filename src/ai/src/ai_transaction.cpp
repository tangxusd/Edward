#include "edward/ai/ai_transaction.hpp"

namespace edward::ai {
TransactionResult AiTransaction::apply(const ActionPlan& plan, ProjectState& state) {
  ProjectSnapshot snapshot{state.revision, state.objects.keys(), {}, {}, state.capabilitySetVersion,
                           state.capabilitySetHash, state.referenceSnapshotId};
  QString error;
  if (!plan.validate(snapshot, &error)) return TransactionResult::failure(error);
  const auto before = state;
  for (const auto& item : plan.operations) {
    const auto operation = item.toObject();
    const auto capability = operation.value("capability").toString();
    const auto targetId = operation.value("target").toObject().value("id").toString();
    const auto args = operation.value("args").toObject();
    if (capability == QStringLiteral("clip.remove")) state.objects.remove(targetId);
    else if (capability == QStringLiteral("component.set_props")) state.objects.insert(targetId, args.value("props").toObject());
    else if (capability == QStringLiteral("component.insert_native")) {
      const auto id = args.value("id").toString();
      if (id.isEmpty() || state.objects.contains(id)) { state = before; return TransactionResult::failure(QStringLiteral("component id is missing or already exists")); }
      state.objects.insert(id, args);
    } else if (!targetId.isEmpty() && !state.objects.contains(targetId)) {
      state = before;
      return TransactionResult::failure(QStringLiteral("operation target disappeared"));
    } else {
      state = before;
      return TransactionResult::failure(QStringLiteral("unsupported transaction capability"));
    }
  }
  undo_.push_back(before);
  while (undo_.size() > 10) undo_.pop_front();
  ++state.revision;
  return TransactionResult::success();
}

TransactionResult AiTransaction::undoLast(ProjectState& state) {
  if (undo_.empty()) return TransactionResult::failure(QStringLiteral("no AI transaction to undo"));
  state = undo_.back();
  undo_.pop_back();
  return TransactionResult::success();
}
}  // namespace edward::ai
