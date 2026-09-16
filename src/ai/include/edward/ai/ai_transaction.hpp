#pragma once

#include "edward/ai/action_plan.hpp"

#include <QJsonObject>

#include <deque>

namespace edward::ai {

struct ProjectState final {
  qint64 revision = 0;
  QJsonObject objects;
};

struct TransactionResult final {
  bool ok = false;
  QString error;
  static TransactionResult success() { return {true, {}}; }
  static TransactionResult failure(QString message) { return {false, std::move(message)}; }
};

class AiTransaction final {
 public:
  TransactionResult apply(const ActionPlan& plan, ProjectState& state);
  TransactionResult undoLast(ProjectState& state);
  [[nodiscard]] int undoDepth() const { return static_cast<int>(undo_.size()); }

 private:
  std::deque<ProjectState> undo_;
};

}  // namespace edward::ai
