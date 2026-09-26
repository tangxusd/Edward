#include "edward/ai/ai_transaction.hpp"

namespace edward::ai {

TransactionResult AiTransaction::apply(const ActionPlan& plan, ProjectState& state) {
  const auto prepared = executor_.prepare(plan, state);
  auto receipt = executor_.commit(PreparedTransaction(prepared), state);
  if (!receipt.ok) return {false, receipt.error, receipt};
  return {true, {}, receipt};
}

TransactionResult AiTransaction::undoLast(ProjectState& state) {
  if (!executor_.journal().undo(state)) return TransactionResult::failure(QStringLiteral("no transaction to undo"));
  return TransactionResult::success();
}

TransactionResult AiTransaction::redo(ProjectState& state) {
  if (!executor_.journal().redo(state)) return TransactionResult::failure(QStringLiteral("no transaction to redo"));
  return TransactionResult::success();
}

}  // namespace edward::ai
