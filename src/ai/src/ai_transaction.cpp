#include "edward/ai/ai_transaction.hpp"

#include <QFileInfo>

namespace edward::ai {
TransactionResult AiTransaction::apply(const ActionPlan& plan, ProjectState& state) {
  ProjectSnapshot snapshot{state.revision, state.objects.keys(), {}};
  QString error;
  if (!plan.validate(snapshot, &error)) return TransactionResult::failure(error);
  const auto before = state;
  for (const auto& item : plan.operations) {
    const auto operation = item.toObject();
    const auto type = operation.value("type").toString();
    const auto targetId = operation.value("targetId").toString();
    if (type == QStringLiteral("export_timeline")) {
      const auto outputPath = operation.value("outputPath").toString();
      if (outputPath.isEmpty() || QFileInfo::exists(outputPath)) { state = before; return TransactionResult::failure(QStringLiteral("export output already exists or is invalid")); }
      continue;
    }
    if (type == QStringLiteral("remove_clip")) state.objects.remove(targetId);
    else if (type == QStringLiteral("set_component_props")) state.objects.insert(targetId, operation.value("props").toObject());
    else if (type == QStringLiteral("insert_native_component") || type == QStringLiteral("insert_resource_component")) {
      const auto id = operation.value("id").toString();
      if (id.isEmpty() || state.objects.contains(id)) { state = before; return TransactionResult::failure(QStringLiteral("component id is missing or already exists")); }
      state.objects.insert(id, operation);
    } else if (!targetId.isEmpty() && !state.objects.contains(targetId)) { state = before; return TransactionResult::failure(QStringLiteral("operation target disappeared")); }
  }
  undo_.push_back(before);
  while (undo_.size() > 5) undo_.pop_front();
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
