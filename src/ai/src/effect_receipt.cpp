#include "edward/ai/effect_receipt.hpp"

#include <QJsonArray>

#include <algorithm>

namespace edward::ai {
namespace {

QStringList operationSet(const BoundActionPlan& plan, const QString& field) {
  QStringList values;
  for (const auto& value : plan.operations) {
    const auto array = value.toObject().value(field).toArray();
    for (const auto& item : array)
      if (item.isString() && !item.toString().isEmpty()) values.push_back(item.toString());
  }
  values.removeDuplicates();
  std::sort(values.begin(), values.end());
  return values;
}

QStringList affectedSet(const BoundActionPlan& plan) {
  QStringList values;
  for (const auto& value : plan.operations) {
    const auto operation = value.toObject();
    const auto target = operation.value(QStringLiteral("target")).toObject().value(QStringLiteral("id")).toString();
    if (!target.isEmpty()) values.push_back(target);
    const auto args = operation.value(QStringLiteral("args")).toObject();
    for (const auto key : {QStringLiteral("id"), QStringLiteral("markerId"), QStringLiteral("clipId")}) {
      const auto id = args.value(key).toString();
      if (!id.isEmpty()) values.push_back(id);
    }
  }
  values.removeDuplicates();
  std::sort(values.begin(), values.end());
  return values;
}

}  // namespace

QJsonObject EffectReceipt::toJson() const {
  return {{QStringLiteral("requestId"), requestId},
          {QStringLiteral("transactionId"), transactionId},
          {QStringLiteral("revisionBefore"), revisionBefore},
          {QStringLiteral("revisionAfter"), revisionAfter},
          {QStringLiteral("operationCount"), operationCount},
          {QStringLiteral("affectedIds"), QJsonArray::fromStringList(affectedIds)},
          {QStringLiteral("readSet"), QJsonArray::fromStringList(readSet)},
          {QStringLiteral("writeSet"), QJsonArray::fromStringList(writeSet)},
          {QStringLiteral("previewVerified"), previewVerified},
          {QStringLiteral("exportVerified"), exportVerified},
          {QStringLiteral("reversible"), reversible},
          {QStringLiteral("ok"), ok},
          {QStringLiteral("error"), error}};
}

ProjectJournal::ProjectJournal(int limit) : limit_(std::max(1, limit)) {}

void ProjectJournal::record(const ProjectState& before, const ProjectState& after, const EffectReceipt& receipt) {
  undo_.push_back({before, after, receipt});
  while (static_cast<int>(undo_.size()) > limit_) undo_.pop_front();
  redo_.clear();
}

bool ProjectJournal::undo(ProjectState& state) {
  if (undo_.empty()) return false;
  auto entry = undo_.back();
  undo_.pop_back();
  redo_.push_back(entry);
  const auto revision = state.revision + 1;
  state = entry.before;
  state.revision = revision;
  return true;
}

bool ProjectJournal::redo(ProjectState& state) {
  if (redo_.empty()) return false;
  auto entry = redo_.back();
  redo_.pop_back();
  undo_.push_back(entry);
  const auto revision = state.revision + 1;
  state = entry.after;
  state.revision = revision;
  return true;
}

std::optional<EffectReceipt> ProjectJournal::receiptForRequest(const QString& requestId) const {
  if (requestId.isEmpty()) return std::nullopt;
  for (auto it = undo_.crbegin(); it != undo_.crend(); ++it)
    if (it->receipt.requestId == requestId) return it->receipt;
  for (auto it = redo_.crbegin(); it != redo_.crend(); ++it)
    if (it->receipt.requestId == requestId) return it->receipt;
  return std::nullopt;
}

PreparedTransaction TransactionExecutor::prepare(const BoundActionPlan& plan, const ProjectState& state) const {
  PreparedTransaction prepared{plan, state, state, {}, false};
  ProjectSnapshot snapshot{state.revision, state.objects.keys(), {}, {}, state.capabilitySetVersion,
                           state.capabilitySetHash, state.referenceSnapshotId};
  if (!plan.validate(snapshot, &prepared.error)) return prepared;

  for (const auto& value : plan.operations) {
    const auto operation = value.toObject();
    const auto capability = operation.value(QStringLiteral("capability")).toString();
    const auto targetId = operation.value(QStringLiteral("target")).toObject().value(QStringLiteral("id")).toString();
    const auto args = operation.value(QStringLiteral("args")).toObject();
    if (capability == QStringLiteral("clip.remove") || capability == QStringLiteral("remove_clip")) {
      if (!prepared.after.objects.contains(targetId)) { prepared.error = QStringLiteral("operation target disappeared"); return prepared; }
      prepared.after.objects.remove(targetId);
    } else if (capability == QStringLiteral("component.set_props") || capability == QStringLiteral("set_component_props") ||
               capability == QStringLiteral("set_clip_props") || capability == QStringLiteral("set_audio_props")) {
      if (!prepared.after.objects.contains(targetId)) { prepared.error = QStringLiteral("operation target disappeared"); return prepared; }
      auto object = prepared.after.objects.value(targetId).toObject();
      const auto props = args.value(QStringLiteral("props")).toObject();
      for (auto it = props.begin(); it != props.end(); ++it) object.insert(it.key(), it.value());
      prepared.after.objects.insert(targetId, object);
    } else if (capability == QStringLiteral("component.insert_native") || capability == QStringLiteral("insert_native_component")) {
      const auto id = args.value(QStringLiteral("id")).toString();
      if (id.isEmpty() || prepared.after.objects.contains(id)) { prepared.error = QStringLiteral("component id is missing or already exists"); return prepared; }
      prepared.after.objects.insert(id, args);
    } else {
      prepared.error = QStringLiteral("unsupported transaction capability");
      return prepared;
    }
  }
  prepared.valid = true;
  return prepared;
}

EffectReceipt TransactionExecutor::commit(PreparedTransaction&& prepared, ProjectState& state) {
  EffectReceipt receipt;
  receipt.requestId = prepared.plan.requestId;
  if (const auto previous = journal_.receiptForRequest(receipt.requestId)) return *previous;
  receipt.revisionBefore = state.revision;
  receipt.operationCount = prepared.plan.operations.size();
  receipt.affectedIds = affectedSet(prepared.plan);
  receipt.readSet = operationSet(prepared.plan, QStringLiteral("readSet"));
  receipt.writeSet = operationSet(prepared.plan, QStringLiteral("writeSet"));
  receipt.transactionId = QStringLiteral("txn-%1-%2").arg(receipt.requestId).arg(receipt.revisionBefore);
  if (!prepared.valid) {
    receipt.error = prepared.error.isEmpty() ? QStringLiteral("transaction is not prepared") : prepared.error;
    return receipt;
  }
  const auto before = state;
  state = prepared.after;
  state.revision = before.revision + 1;
  receipt.revisionAfter = state.revision;
  receipt.previewVerified = true;
  receipt.exportVerified = false;
  receipt.reversible = true;
  receipt.ok = true;
  journal_.record(before, state, receipt);
  return receipt;
}

}  // namespace edward::ai
