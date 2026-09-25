#include "edward/resources/catalog_cache.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace edward::resources {
namespace {

bool fail(QString* error, const QSqlQuery& query) {
  if (error) *error = query.lastError().text();
  return false;
}

QString typeName(CatalogType type) { return type == CatalogType::Color ? QStringLiteral("color") : QStringLiteral("font"); }

}  // namespace

CatalogCache::CatalogCache(QString databasePath) : databasePath_(std::move(databasePath)) {
  if (databasePath_.isEmpty()) {
    databasePath_ = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/catalog-cache.sqlite");
  }
  connectionName_ = QStringLiteral("orbit-catalog-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
}

CatalogCache::~CatalogCache() {
  if (database_.isOpen()) database_.close();
  database_ = {};
  QSqlDatabase::removeDatabase(connectionName_);
}

bool CatalogCache::open(QString* error) {
  if (database_.isOpen()) return true;
  const auto pathInfo = QFileInfo(databasePath_);
  if (!QDir().mkpath(pathInfo.absolutePath())) {
    if (error) *error = QStringLiteral("无法创建目录");
    return false;
  }
  database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
  database_.setDatabaseName(databasePath_);
  if (!database_.open()) {
    if (error) *error = database_.lastError().text();
    return false;
  }
  QSqlQuery pragma(database_);
  pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
  pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
  return ensureSchema(error);
}

bool CatalogCache::ensureSchema(QString* error) {
  QSqlQuery query(database_);
  const QStringList statements{
      QStringLiteral("CREATE TABLE IF NOT EXISTS catalog_sessions (session_key TEXT PRIMARY KEY, account_id TEXT NOT NULL, installation_id TEXT NOT NULL, project_id TEXT NOT NULL, catalog_type TEXT NOT NULL, revision TEXT NOT NULL DEFAULT '', last_sync_attempt TEXT, last_sync_success TEXT)"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS catalog_colors (session_key TEXT NOT NULL, semantic_path TEXT NOT NULL, value_type TEXT NOT NULL, value_json TEXT NOT NULL, content_hash TEXT NOT NULL, protected INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(session_key, semantic_path))"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS catalog_fonts (session_key TEXT NOT NULL, font_id TEXT NOT NULL, display_name TEXT NOT NULL, family_name TEXT NOT NULL, style_name TEXT NOT NULL, weight INTEGER NOT NULL, preview_object_key TEXT NOT NULL, install_object_key TEXT NOT NULL, content_hash TEXT NOT NULL, resource_version TEXT NOT NULL, protected INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(session_key, font_id))"),
      QStringLiteral("CREATE INDEX IF NOT EXISTS catalog_fonts_hash_idx ON catalog_fonts(content_hash)"),
  };
  for (const auto& statement : statements) {
    if (!query.exec(statement)) return fail(error, query);
  }
  return true;
}

QString CatalogCache::key(const CatalogSession& session) const {
  return session.accountId + QLatin1Char('/') + session.installationId + QLatin1Char('/') + session.projectId + QLatin1Char('/') + typeName(session.type);
}

CatalogSession CatalogCache::openSession(const QString& accountId, const QString& projectId, CatalogType type) {
  CatalogSession session{accountId, QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation), projectId, type, {}};
  if (!open()) return session;
  QSqlQuery query(database_);
  query.prepare(QStringLiteral("INSERT OR IGNORE INTO catalog_sessions(session_key, account_id, installation_id, project_id, catalog_type) VALUES(?,?,?,?,?)"));
  query.addBindValue(key(session)); query.addBindValue(session.accountId); query.addBindValue(session.installationId); query.addBindValue(session.projectId); query.addBindValue(typeName(type));
  query.exec();
  query.prepare(QStringLiteral("SELECT revision FROM catalog_sessions WHERE session_key=?"));
  query.addBindValue(key(session));
  if (query.exec() && query.next()) session.revision = query.value(0).toString();
  return session;
}

CatalogSyncResult CatalogCache::checkRevisionOnce(CatalogSession& session, const QJsonObject& manifest) {
  CatalogSyncResult result;
  if (!open(&result.error)) return result;
  const auto revision = manifest.value(QStringLiteral("revision"));
  if (!revision.isString() || revision.toString().isEmpty()) { result.error = QStringLiteral("目录版本无效"); return result; }
  const auto newRevision = revision.toString();
  result.revision = newRevision;
  result.changed = newRevision != session.revision;
  QSqlQuery query(database_);
  query.prepare(QStringLiteral("UPDATE catalog_sessions SET revision=?, last_sync_attempt=?, last_sync_success=? WHERE session_key=?"));
  query.addBindValue(newRevision); query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)); query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)); query.addBindValue(key(session));
  if (!query.exec()) { result.error = query.lastError().text(); return result; }
  session.revision = newRevision;
  result.ok = true;
  return result;
}

bool CatalogCache::storeColor(const CatalogSession& session, const ColorValue& color) {
  if (!open() || color.semanticPath.isEmpty() || color.contentHash.isEmpty()) return false;
  QSqlQuery query(database_);
  query.prepare(QStringLiteral("INSERT OR REPLACE INTO catalog_colors(session_key, semantic_path, value_type, value_json, content_hash) VALUES(?,?,?,?,?)"));
  query.addBindValue(key(session)); query.addBindValue(color.semanticPath); query.addBindValue(color.valueType); query.addBindValue(QString::fromUtf8(QJsonDocument(color.value).toJson(QJsonDocument::Compact))); query.addBindValue(color.contentHash);
  return query.exec();
}

bool CatalogCache::storeFont(const CatalogSession& session, const FontRecord& font) {
  if (!open() || font.fontId.isEmpty() || font.contentHash.isEmpty()) return false;
  QSqlQuery query(database_);
  query.prepare(QStringLiteral("INSERT OR REPLACE INTO catalog_fonts(session_key, font_id, display_name, family_name, style_name, weight, preview_object_key, install_object_key, content_hash, resource_version) VALUES(?,?,?,?,?,?,?,?,?,?)"));
  query.addBindValue(key(session)); query.addBindValue(font.fontId); query.addBindValue(font.displayName); query.addBindValue(font.familyName); query.addBindValue(font.styleName); query.addBindValue(font.weight); query.addBindValue(font.previewObjectKey); query.addBindValue(font.installObjectKey); query.addBindValue(font.contentHash); query.addBindValue(font.resourceVersion);
  return query.exec();
}

std::optional<ColorValue> CatalogCache::readColor(const CatalogSession& session, const QString& semanticPath) const {
  if (!database_.isOpen()) return std::nullopt;
  QSqlQuery query(database_); query.prepare(QStringLiteral("SELECT value_type,value_json,content_hash FROM catalog_colors WHERE session_key=? AND semantic_path=?")); query.addBindValue(key(session)); query.addBindValue(semanticPath);
  if (!query.exec() || !query.next()) return std::nullopt;
  return ColorValue{semanticPath, query.value(0).toString(), QJsonDocument::fromJson(query.value(1).toByteArray()).object(), query.value(2).toString()};
}

std::optional<FontRecord> CatalogCache::readFont(const CatalogSession& session, const QString& fontId) const {
  if (!database_.isOpen()) return std::nullopt;
  QSqlQuery query(database_); query.prepare(QStringLiteral("SELECT display_name,family_name,style_name,weight,preview_object_key,install_object_key,content_hash,resource_version FROM catalog_fonts WHERE session_key=? AND font_id=?")); query.addBindValue(key(session)); query.addBindValue(fontId);
  if (!query.exec() || !query.next()) return std::nullopt;
  return FontRecord{fontId, query.value(0).toString(), query.value(1).toString(), query.value(2).toString(), query.value(3).toInt(), query.value(4).toString(), query.value(5).toString(), query.value(6).toString(), query.value(7).toString()};
}

bool CatalogCache::protectProjectReference(const CatalogSession& session, const QString& contentHash) {
  if (!open()) return false;
  QSqlQuery query(database_); query.prepare(QStringLiteral("UPDATE catalog_fonts SET protected=1 WHERE session_key=? AND content_hash=?")); query.addBindValue(key(session)); query.addBindValue(contentHash); return query.exec();
}

int CatalogCache::clearUnreferenced(int maxRows) {
  if (!open() || maxRows <= 0) return 0;
  QSqlQuery query(database_); query.prepare(QStringLiteral("DELETE FROM catalog_fonts WHERE protected=0 AND rowid IN (SELECT rowid FROM catalog_fonts WHERE protected=0 LIMIT ?)")); query.addBindValue(maxRows); if (!query.exec()) return 0; return query.numRowsAffected();
}

}  // namespace edward::resources
