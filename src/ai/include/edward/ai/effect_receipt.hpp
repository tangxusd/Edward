#pragma once

#include "edward/ai/action_plan.hpp"

#include <QJsonObject>
#include <QStringList>

#include <deque>
#include <optional>

namespace edward::ai {

using BoundActionPlan = ActionPlan;

struct EffectReceipt final {
  QString requestId;
  QString transactionId;
  qint64 revisionBefore = 0;
  qint64 revisionAfter = 0;
  int operationCount = 0;
  QStringList affectedIds;
  QStringList readSet;
  QStringList writeSet;
  bool previewVerified = false;
  bool exportVerified = false;
  bool reversible = false;
  bool ok = false;
  QString error;

  [[nodiscard]] QJsonObject toJson() const;
};

struct ProjectState final {
  qint64 revision = 0;
  QJsonObject objects;
  qint64 capabilitySetVersion = -1;
  QString capabilitySetHash;
  QString referenceSnapshotId;
};

struct PreparedTransaction final {
  BoundActionPlan plan;
  ProjectState before;
  ProjectState after;
  QString error;
  bool valid = false;
};

class ProjectJournal final {
 public:
  explicit ProjectJournal(int limit = 10);

  void record(const ProjectState& before, const ProjectState& after, const EffectReceipt& receipt);
  [[nodiscard]] bool undo(ProjectState& state);
  [[nodiscard]] bool redo(ProjectState& state);
  [[nodiscard]] int undoDepth() const { return static_cast<int>(undo_.size()); }
  [[nodiscard]] int redoDepth() const { return static_cast<int>(redo_.size()); }
  [[nodiscard]] std::optional<EffectReceipt> receiptForRequest(const QString& requestId) const;

 private:
  struct Entry final {
    ProjectState before;
    ProjectState after;
    EffectReceipt receipt;
  };
  std::deque<Entry> undo_;
  std::deque<Entry> redo_;
  int limit_ = 10;
};

class TransactionExecutor final {
 public:
  [[nodiscard]] PreparedTransaction prepare(const BoundActionPlan& plan, const ProjectState& state) const;
  [[nodiscard]] EffectReceipt commit(PreparedTransaction&& prepared, ProjectState& state);
  [[nodiscard]] ProjectJournal& journal() { return journal_; }
  [[nodiscard]] const ProjectJournal& journal() const { return journal_; }

 private:
  ProjectJournal journal_;
};

}  // namespace edward::ai
