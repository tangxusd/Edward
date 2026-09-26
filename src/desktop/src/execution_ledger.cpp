#include "edward/desktop/execution_ledger.hpp"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QHash>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>

namespace edward::desktop {

ExecutionLedger::ExecutionLedger(QString path) : path_(std::move(path)) {}

QString ExecutionLedger::stateName(LedgerState state) {
  switch (state) {
    case LedgerState::Begun: return QStringLiteral("begun");
    case LedgerState::Prepared: return QStringLiteral("prepared");
    case LedgerState::Committed: return QStringLiteral("committed");
    case LedgerState::RolledBack: return QStringLiteral("rolled_back");
    case LedgerState::Failed: return QStringLiteral("failed");
  }
  return QStringLiteral("failed");
}

LedgerState ExecutionLedger::stateFrom(const QString& value) {
  if (value == QStringLiteral("prepared")) return LedgerState::Prepared;
  if (value == QStringLiteral("committed")) return LedgerState::Committed;
  if (value == QStringLiteral("rolled_back")) return LedgerState::RolledBack;
  if (value == QStringLiteral("failed")) return LedgerState::Failed;
  return LedgerState::Begun;
}

QJsonObject ExecutionLedger::toJson(const LedgerEntry& entry) {
  return {{QStringLiteral("projectId"), entry.identity.projectId},
          {QStringLiteral("requestId"), entry.identity.requestId},
          {QStringLiteral("inputHash"), entry.identity.inputHash},
          {QStringLiteral("transactionId"), entry.transactionId},
          {QStringLiteral("state"), stateName(entry.state)},
          {QStringLiteral("summary"), entry.summary.left(256)},
          {QStringLiteral("timestampMs"), entry.timestampMs}};
}

LedgerEntry ExecutionLedger::fromJson(const QJsonObject& object) {
  return {{object.value(QStringLiteral("projectId")).toString(),
           object.value(QStringLiteral("requestId")).toString(),
           object.value(QStringLiteral("inputHash")).toString()},
          object.value(QStringLiteral("transactionId")).toString(),
          stateFrom(object.value(QStringLiteral("state")).toString()),
          object.value(QStringLiteral("summary")).toString(),
          object.value(QStringLiteral("timestampMs")).toInteger()};
}

QVector<LedgerEntry> ExecutionLedger::readAll(QString* error) const {
  QVector<LedgerEntry> entries;
  QLockFile lock(path_ + QStringLiteral(".lock"));
  lock.setStaleLockTime(30000);
  if (!lock.tryLock(1000)) {
    if (error) *error = QStringLiteral("execution ledger is locked by another process");
    return {};
  }
  QFile file(path_);
  if (!file.exists()) return entries;
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error) *error = file.errorString();
    return {};
  }
  while (!file.atEnd()) {
    const auto line = file.readLine().trimmed();
    if (line.isEmpty()) continue;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
      if (error) *error = QStringLiteral("execution ledger contains invalid JSON");
      return {};
    }
    entries.append(fromJson(document.object()));
  }
  return entries;
}

bool ExecutionLedger::append(const LedgerEntry& entry, QString* error) {
  QLockFile lock(path_ + QStringLiteral(".lock"));
  lock.setStaleLockTime(30000);
  if (!lock.tryLock(1000)) {
    if (error) *error = QStringLiteral("execution ledger is locked by another process");
    return false;
  }
  return appendUnlocked(entry, error);
}

bool ExecutionLedger::appendUnlocked(const LedgerEntry& entry, QString* error) {
  QFile file(path_);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    if (error) *error = file.errorString();
    return false;
  }
  const auto line = QJsonDocument(toJson(entry)).toJson(QJsonDocument::Compact) + '\n';
  if (file.write(line) != line.size() || !file.flush()) {
    if (error) *error = file.errorString();
    return false;
  }
  return true;
}

LedgerEntry ExecutionLedger::begin(const RequestIdentity& identity, QString* error) {
  LedgerEntry entry{identity, QStringLiteral("tx-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)),
                    LedgerState::Begun, {}, QDateTime::currentMSecsSinceEpoch()};
  QLockFile lock(path_ + QStringLiteral(".lock"));
  lock.setStaleLockTime(30000);
  if (!lock.tryLock(1000)) {
    if (error) *error = QStringLiteral("execution ledger is locked by another process");
    entry.transactionId.clear();
    return entry;
  }
  QFile existing(path_);
  if (existing.exists() && existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
    while (!existing.atEnd()) {
      const auto line = existing.readLine().trimmed();
      if (line.isEmpty()) continue;
      QJsonParseError parseError;
      const auto document = QJsonDocument::fromJson(line, &parseError);
      if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("execution ledger contains invalid JSON");
        entry.transactionId.clear();
        return entry;
      }
      const auto prior = fromJson(document.object());
      if (prior.identity.projectId == identity.projectId && prior.identity.requestId == identity.requestId) {
        if (prior.identity.inputHash != identity.inputHash) {
          if (error) *error = QStringLiteral("request identity conflicts with existing ledger entry");
          entry.transactionId.clear();
          return entry;
        }
        return prior;
      }
    }
  }
  if (!appendUnlocked(entry, error)) entry.transactionId.clear();
  return entry;
}

LedgerLookup ExecutionLedger::lookup(const RequestIdentity& identity) const {
  QString error;
  const auto entries = readAll(&error);
  if (!error.isEmpty()) return {LedgerLookup::Kind::Missing, {}};
  LedgerLookup result;
  for (const auto& entry : entries) {
    if (entry.identity.projectId != identity.projectId || entry.identity.requestId != identity.requestId) continue;
    if (entry.identity.inputHash != identity.inputHash) return {LedgerLookup::Kind::Conflict, entry};
    result = {LedgerLookup::Kind::Match, entry};
  }
  return result;
}

RecoveryResult ExecutionLedger::recover(const QString& projectId) const {
  QString error;
  const auto entries = readAll(&error);
  if (!error.isEmpty()) return {{}, {}, false, error};
  QHash<QString, LedgerEntry> latest;
  for (const auto& entry : entries) if (entry.identity.projectId == projectId) latest.insert(entry.transactionId, entry);
  RecoveryResult result;
  for (const auto& entry : latest) {
    if (entry.state == LedgerState::Begun || entry.state == LedgerState::Prepared) result.rollback.append(entry);
    if (entry.state == LedgerState::Committed) result.committed.append(entry);
  }
  return result;
}

}  // namespace edward::desktop
