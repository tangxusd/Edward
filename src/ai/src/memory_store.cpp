#include "edward/ai/memory_store.hpp"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>

namespace edward::ai { namespace {
QString scope(MemoryScope value) { return value == MemoryScope::Global ? "global" : value == MemoryScope::Project ? "project" : "session"; }
MemoryScope scopeFrom(const QString& value) { return value == "global" ? MemoryScope::Global : value == "project" ? MemoryScope::Project : MemoryScope::Session; }
QString projectId(const QString& value) { return value.isEmpty() ? QStringLiteral("_") : value; }
QSqlDatabase database(const QString& path, QString* name) { *name = QStringLiteral("edward-memory-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)); auto db = QSqlDatabase::addDatabase("QSQLITE", *name); db.setDatabaseName(path); return db; }
}
MemoryStore::MemoryStore(QString databasePath) : databasePath_(std::move(databasePath)) {}
MemoryResult MemoryStore::remember(const MemoryEntry& entry, bool userConfirmed) { if (!userConfirmed) return MemoryResult::success(); if (entry.nameSpace.isEmpty() || entry.key.isEmpty()) return MemoryResult::failure("memory namespace and key are required"); if (entry.scope == MemoryScope::Session) { session_.push_back(entry); return MemoryResult::success(); } pending_.push_back(entry); return MemoryResult::success(); }
MemoryResult MemoryStore::forget(const MemoryEntry& entry) { auto tombstone = entry; tombstone.deleted = true; return remember(tombstone, true); }
QVector<MemoryEntry> MemoryStore::query(const MemoryQuery& request) const { QVector<MemoryEntry> result; const auto collect = [&](const QVector<MemoryEntry>& entries) { for (const auto& entry : entries) if (entry.scope == request.scope && entry.projectId == request.projectId && entry.nameSpace == request.nameSpace && entry.key == request.key && !entry.deleted) result.push_back(entry); };
  collect(session_); if (request.scope == MemoryScope::Session) return result; QString name; auto db = database(databasePath_, &name); if (!db.open()) return result; { QSqlQuery query(db); query.prepare("SELECT scope,project_id,namespace,memory_key,value,deleted FROM ai_memory WHERE scope=? AND project_id=? AND namespace=? AND memory_key=? ORDER BY id DESC LIMIT 1"); query.addBindValue(scope(request.scope)); query.addBindValue(projectId(request.projectId)); query.addBindValue(request.nameSpace); query.addBindValue(request.key); if (query.exec() && query.next() && !query.value(5).toBool()) result.push_back({scopeFrom(query.value(0).toString()), query.value(1).toString() == "_" ? QString{} : query.value(1).toString(), query.value(2).toString(), query.value(3).toString(), query.value(4).toString(), false}); } db.close(); db = {}; QSqlDatabase::removeDatabase(name); return result; }
MemoryResult MemoryStore::flush(FlushReason) { if (pending_.isEmpty()) return MemoryResult::success(); QString name; auto db = database(databasePath_, &name); if (!db.open()) return MemoryResult::failure(db.lastError().text()); { QSqlQuery schema(db); if (!schema.exec("CREATE TABLE IF NOT EXISTS ai_memory (id INTEGER PRIMARY KEY, scope TEXT, project_id TEXT, namespace TEXT, memory_key TEXT, value TEXT, deleted INTEGER)")) return MemoryResult::failure(schema.lastError().text()); QSqlQuery insert(db); insert.prepare("INSERT INTO ai_memory(scope,project_id,namespace,memory_key,value,deleted) VALUES(?,?,?,?,?,?)"); for (const auto& entry : pending_) { insert.addBindValue(scope(entry.scope)); insert.addBindValue(projectId(entry.projectId)); insert.addBindValue(entry.nameSpace); insert.addBindValue(entry.key); insert.addBindValue(entry.value); insert.addBindValue(entry.deleted); if (!insert.exec()) return MemoryResult::failure(insert.lastError().text()); } } pending_.clear(); db.close(); db = {}; QSqlDatabase::removeDatabase(name); return MemoryResult::success(); }
}  // namespace edward::ai
