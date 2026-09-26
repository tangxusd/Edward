#pragma once

#include "edward/ai/action_plan.hpp"
#include "edward/ai/effect_receipt.hpp"

#include <QJsonObject>

#include <deque>

namespace edward::ai {

struct TransactionResult final {
  bool ok = false;
  QString error;
  EffectReceipt receipt;
  static TransactionResult success() { return {true, {}}; }
  static TransactionResult failure(QString message) { return {false, std::move(message)}; }
};

class AiTransaction final {
 public:
  TransactionResult apply(const ActionPlan& plan, ProjectState& state);
  TransactionResult undoLast(ProjectState& state);
  TransactionResult redo(ProjectState& state);
  [[nodiscard]] int undoDepth() const { return executor_.journal().undoDepth(); }
  [[nodiscard]] int redoDepth() const { return executor_.journal().redoDepth(); }

 private:
  TransactionExecutor executor_;
};

}  // namespace edward::ai
