#pragma once

#include <QString>
#include <QVector>

namespace edward::ai {
enum class MemoryScope { Global, Project, Session };
enum class FlushReason { Idle, ProjectOpen, Export };
struct MemoryEntry { MemoryScope scope = MemoryScope::Session; QString projectId; QString nameSpace; QString key; QString value; bool deleted = false; };
struct MemoryQuery { MemoryScope scope = MemoryScope::Session; QString projectId; QString nameSpace; QString key; };
struct MemoryResult { bool ok = false; QString error; static MemoryResult success() { return {true, {}}; } static MemoryResult failure(QString value) { return {false, std::move(value)}; } };
class MemoryStore final {
 public:
  explicit MemoryStore(QString databasePath);
  MemoryResult remember(const MemoryEntry& entry, bool userConfirmed);
  MemoryResult forget(const MemoryEntry& entry);
  QVector<MemoryEntry> query(const MemoryQuery& query) const;
  MemoryResult flush(FlushReason reason);
 private: QString databasePath_; QVector<MemoryEntry> pending_; QVector<MemoryEntry> session_; };
}  // namespace edward::ai
