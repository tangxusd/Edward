#include "edward/desktop/execution_ledger.hpp"

#include <QTemporaryDir>
#include <QFile>
#include <cassert>
#include <thread>
#include <vector>

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

  // Two workers beginning the same request must converge on one WAL entry.
  const RequestIdentity concurrentIdentity{QStringLiteral("project-2"), QStringLiteral("request-2"), QStringLiteral("hash-c")};
  std::vector<LedgerEntry> results(8);
  std::vector<std::thread> workers;
  for (size_t i = 0; i < results.size(); ++i) {
    workers.emplace_back([&, i] {
      ExecutionLedger worker(directory.filePath(QStringLiteral("execution.wal")));
      QString workerError;
      results[i] = worker.begin(concurrentIdentity, &workerError);
      assert(workerError.isEmpty());
    });
  }
  for (auto& worker : workers) worker.join();
  for (const auto& result : results) assert(result.transactionId == results.front().transactionId);

  // A truncated tail is a hard recovery error, never silently committed.
  QFile wal(directory.filePath(QStringLiteral("execution.wal")));
  assert(wal.open(QIODevice::WriteOnly | QIODevice::Append));
  wal.write("{\"projectId\":\"broken\"");
  wal.close();
  const auto brokenRecovery = ledger.recover(QStringLiteral("broken"));
  assert(!brokenRecovery.readable && !brokenRecovery.error.isEmpty());
}
