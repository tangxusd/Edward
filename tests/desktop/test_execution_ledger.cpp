#include "edward/desktop/execution_ledger.hpp"

#include <QTemporaryDir>
#include <cassert>

using namespace edward::desktop;

int main() {
  QTemporaryDir directory;
  assert(directory.isValid());
  ExecutionLedger ledger(directory.filePath(QStringLiteral("execution.wal")));
  const RequestIdentity identity{QStringLiteral("project-1"), QStringLiteral("request-1"), QStringLiteral("hash-a")};
  QString error;
  const auto begun = ledger.begin(identity, &error);
  assert(!begun.transactionId.isEmpty() && error.isEmpty());
  assert(ledger.lookup(identity).kind == LedgerLookup::Kind::Match);
  const auto conflict = ledger.lookup({identity.projectId, identity.requestId, QStringLiteral("hash-b")});
  assert(conflict.kind == LedgerLookup::Kind::Conflict);
  auto prepared = begun;
  prepared.state = LedgerState::Prepared;
  assert(ledger.append(prepared, &error));
  auto recovery = ledger.recover(identity.projectId);
  assert(recovery.rollback.size() == 1 && recovery.committed.isEmpty());
  auto committed = prepared;
  committed.state = LedgerState::Committed;
  assert(ledger.append(committed, &error));
  recovery = ledger.recover(identity.projectId);
  assert(recovery.rollback.isEmpty() && recovery.committed.size() == 1);
}
