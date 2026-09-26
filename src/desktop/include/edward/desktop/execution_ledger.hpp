#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace edward::desktop {

struct RequestIdentity final {
  QString projectId;
  QString requestId;
  QString inputHash;
};

enum class LedgerState { Begun, Prepared, Committed, RolledBack, Failed };

struct LedgerEntry final {
  RequestIdentity identity;
  QString transactionId;
  LedgerState state = LedgerState::Begun;
  QString summary;
  qint64 timestampMs = 0;
};

struct LedgerLookup final {
  enum class Kind { Missing, Match, Conflict } kind = Kind::Missing;
  LedgerEntry entry;
};

struct RecoveryResult final {
  QVector<LedgerEntry> rollback;
  QVector<LedgerEntry> committed;
  bool readable = true;
  QString error;
};

class ExecutionLedger final {
 public:
  explicit ExecutionLedger(QString path);
  LedgerEntry begin(const RequestIdentity& identity, QString* error = nullptr);
  bool append(const LedgerEntry& entry, QString* error = nullptr);
  [[nodiscard]] LedgerLookup lookup(const RequestIdentity& identity) const;
  [[nodiscard]] RecoveryResult recover(const QString& projectId) const;

 private:
  QString path_;
  static QString stateName(LedgerState state);
  static LedgerState stateFrom(const QString& value);
  static QJsonObject toJson(const LedgerEntry& entry);
  static LedgerEntry fromJson(const QJsonObject& object);
  QVector<LedgerEntry> readAll(QString* error = nullptr) const;
  bool appendUnlocked(const LedgerEntry& entry, QString* error = nullptr);
};

}  // namespace edward::desktop
