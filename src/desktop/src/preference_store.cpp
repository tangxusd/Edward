#include "edward/desktop/preference_store.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>

namespace edward::desktop {

namespace {
constexpr int kMaxPendingEvents = 512;
constexpr int kMaxEventsPerIdentity = 2048;

QString jsonString(const QVariant& value) {
  const auto json = QJsonValue::fromVariant(value);
  if (json.isArray() || json.isObject()) {
    return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
  }
  return QString::fromUtf8(QJsonDocument(QJsonArray{json}).toJson(QJsonDocument::Compact));
}

QVariant fromJsonString(const QString& value) {
  const auto document = QJsonDocument::fromJson(value.toUtf8());
  return document.isNull() ? QVariant{} : document.toVariant();
}

double scoreEvent(const QVariantMap& event, qint64 now) {
  const auto created = event.value(QStringLiteral("createdAt")).toLongLong();
  const auto ageDays = std::max(0.0, (now - created) / 86400000.0);
  const auto recency = 1.0 / (1.0 + ageDays);
  return 0.8 + recency * 0.2;
}

}  // namespace

PreferenceStore::PreferenceStore(QObject* parent) : QObject(parent) {}

PreferenceStore::~PreferenceStore() { closeDatabase(); }

QString PreferenceStore::databasePath() const {
  if (!databasePathOverride_.isEmpty()) return databasePathOverride_;
  const auto root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                    QStringLiteral("/preferences");
  QDir().mkpath(root);
  const auto storagePath = root + QStringLiteral("/preferences.store");
  const auto legacyPath = root + QStringLiteral("/preferences.sqlite");
  if (!QFileInfo::exists(storagePath) && QFileInfo::exists(legacyPath)) {
    QFile::rename(legacyPath, storagePath);
    for (const auto& suffix : {QStringLiteral("-wal"), QStringLiteral("-shm")})
      QFile::rename(legacyPath + suffix, storagePath + suffix);
  }
  return storagePath;
}

QString PreferenceStore::installationId() const {
  static const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  return id;
}

void PreferenceStore::setDatabasePathForTests(const QString& path) {
  closeDatabase();
  databasePathOverride_ = path;
}

bool PreferenceStore::openDatabase() const {
  if (connectionName_.isEmpty())
    connectionName_ = QStringLiteral("edward_preferences_%1").arg(reinterpret_cast<quintptr>(this));
  if (QSqlDatabase::contains(connectionName_)) {
    const auto database = QSqlDatabase::database(connectionName_);
    if (database.isOpen()) return ensureSchema();
  }
  auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
  database.setDatabaseName(databasePath());
  if (!database.open()) return false;
  QSqlQuery pragma(database);
  if (!pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"))) return false;
  return ensureSchema();
}

bool PreferenceStore::ensureSchema() const {
  const auto database = QSqlDatabase::database(connectionName_);
  if (!database.isOpen()) return false;
  QSqlQuery query(database);
  const QStringList statements{
      QStringLiteral("CREATE TABLE IF NOT EXISTS preference_events ("
                     "event_id TEXT PRIMARY KEY, account_scope TEXT NOT NULL DEFAULT 'anonymous', installation_id TEXT NOT NULL, identity_key TEXT NOT NULL, "
                     "component_id TEXT NOT NULL, component_family TEXT NOT NULL, component_version TEXT NOT NULL, "
                     "manifest_hash TEXT NOT NULL, semantic_path TEXT NOT NULL, property_path TEXT NOT NULL, "
                     "value_type TEXT NOT NULL, value_json TEXT NOT NULL, creation_session_id TEXT NOT NULL, profile_rank INTEGER NOT NULL DEFAULT 0, "
                     "source TEXT NOT NULL, created_at INTEGER NOT NULL)"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS preference_profiles ("
                     "identity_key TEXT PRIMARY KEY, profile_json TEXT NOT NULL, revision INTEGER NOT NULL, "
                     "updated_at INTEGER NOT NULL)"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS preference_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)")};
  for (const auto& statement : statements)
    if (!query.exec(statement)) return false;
  bool hasProfileRank = false;
  if (!query.exec(QStringLiteral("PRAGMA table_info(preference_events)"))) return false;
  while (query.next()) hasProfileRank = hasProfileRank || query.value(1).toString() == QStringLiteral("profile_rank");
  if (!hasProfileRank && !query.exec(QStringLiteral("ALTER TABLE preference_events ADD COLUMN profile_rank INTEGER NOT NULL DEFAULT 0"))) return false;
  bool hasAccountScope = false;
  if (!query.exec(QStringLiteral("PRAGMA table_info(preference_events)"))) return false;
  while (query.next()) hasAccountScope = hasAccountScope || query.value(1).toString() == QStringLiteral("account_scope");
  if (!hasAccountScope && !query.exec(QStringLiteral("ALTER TABLE preference_events ADD COLUMN account_scope TEXT NOT NULL DEFAULT 'anonymous'"))) return false;
  if (!query.exec(QStringLiteral("UPDATE preference_events SET identity_key = 'anonymous' || char(30) || identity_key "
                                "WHERE account_scope = 'anonymous' AND instr(identity_key, char(30)) = 0"))) return false;
  if (!query.exec(QStringLiteral("UPDATE preference_events SET event_id = 'anonymous' || char(31) || event_id "
                                "WHERE account_scope = 'anonymous' AND instr(event_id, char(31)) = 0"))) return false;
  if (!query.exec(QStringLiteral("UPDATE preference_profiles SET identity_key = 'anonymous' || char(30) || identity_key "
                                "WHERE instr(identity_key, char(30)) = 0"))) return false;
  return true;
}

void PreferenceStore::closeDatabase() const {
  if (connectionName_.isEmpty()) return;
  const auto connection = connectionName_;
  if (QSqlDatabase::contains(connection)) {
    {
      auto database = QSqlDatabase::database(connection);
      if (database.isOpen()) database.close();
    }
    QSqlDatabase::removeDatabase(connection);
  }
  connectionName_.clear();
}

void PreferenceStore::setAccountScope(const QString& accountId) {
  const auto normalized = accountId.trimmed().isEmpty() ? QStringLiteral("anonymous") : accountId.trimmed();
  if (accountScope_ == normalized) return;
  flushPendingPreferences();
  pendingEvents_.clear();
  profileDirty_ = false;
  accountScope_ = normalized;
}

QString PreferenceStore::makeIdentityKey(const QVariantMap& identity) const {
  const QStringList keys{QStringLiteral("componentId"), QStringLiteral("componentFamily"),
                         QStringLiteral("componentVersion"), QStringLiteral("manifestHash"),
                         QStringLiteral("semanticPath"), QStringLiteral("propertyPath"),
                         QStringLiteral("valueType")};
  QStringList values;
  for (const auto& key : keys) values.append(identity.value(key).toString().trimmed());
  return accountScope_ + QLatin1Char('\x1e') + values.join(QLatin1Char('\x1f'));
}

bool PreferenceStore::isValidObservation(const QVariantMap& observation, QString* error) const {
  const QStringList required{QStringLiteral("eventId"), QStringLiteral("componentId"),
                             QStringLiteral("componentFamily"), QStringLiteral("componentVersion"),
                             QStringLiteral("manifestHash"), QStringLiteral("semanticPath"),
                             QStringLiteral("propertyPath"), QStringLiteral("valueType"),
                             QStringLiteral("value"), QStringLiteral("creationSessionId")};
  for (const auto& key : required) {
    if (!observation.contains(key) || observation.value(key).toString().trimmed().isEmpty()) {
      if (error) *error = QStringLiteral("missing_%1").arg(key);
      return false;
    }
  }
  const auto source = observation.value(QStringLiteral("source")).toString();
  if (source != QStringLiteral("user-confirmed")) {
    if (error) *error = QStringLiteral("source_not_confirmed");
    return false;
  }
  const auto profileRank = observation.value(QStringLiteral("profileRank"), 0).toInt();
  if (profileRank < 0 || profileRank > 2) {
    if (error) *error = QStringLiteral("profile_rank_invalid");
    return false;
  }
  if (observation.contains(QStringLiteral("projectId")) || observation.contains(QStringLiteral("projectPath"))) {
    if (error) *error = QStringLiteral("project_data_forbidden");
    return false;
  }
  return true;
}

bool PreferenceStore::recordConfirmedPropertyChange(const QVariantMap& observation) {
  QString error;
  if (!isValidObservation(observation, &error)) return false;
  auto event = observation;
  event.insert(QStringLiteral("installationId"), installationId());
  event.insert(QStringLiteral("createdAt"), QDateTime::currentMSecsSinceEpoch());
  event.insert(QStringLiteral("identityKey"), makeIdentityKey(observation));
  event.insert(QStringLiteral("profileRank"), observation.value(QStringLiteral("profileRank"), 0).toInt());
  if (pendingEvents_.size() >= kMaxPendingEvents) pendingEvents_.removeFirst();
  pendingEvents_.append(event);
  profileDirty_ = true;
  return true;
}

bool PreferenceStore::flushPendingPreferences() {
  if (pendingEvents_.isEmpty()) return openDatabase();
  if (!openDatabase()) return false;
  auto database = QSqlDatabase::database(connectionName_);
  if (!database.transaction()) return false;
  QSqlQuery query(database);
  query.prepare(QStringLiteral(
      "INSERT OR IGNORE INTO preference_events "
      "(event_id, account_scope, installation_id, identity_key, component_id, component_family, component_version, "
      "manifest_hash, semantic_path, property_path, value_type, value_json, creation_session_id, profile_rank, source, created_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  for (const auto& value : pendingEvents_) {
    const auto event = value.toMap();
    query.bindValue(0, accountScope_ + QLatin1Char('\x1f') + event.value(QStringLiteral("eventId")).toString());
    query.bindValue(1, accountScope_);
    query.bindValue(2, event.value(QStringLiteral("installationId")));
    query.bindValue(3, event.value(QStringLiteral("identityKey")));
    query.bindValue(4, event.value(QStringLiteral("componentId")));
    query.bindValue(5, event.value(QStringLiteral("componentFamily")));
    query.bindValue(6, event.value(QStringLiteral("componentVersion")));
    query.bindValue(7, event.value(QStringLiteral("manifestHash")));
    query.bindValue(8, event.value(QStringLiteral("semanticPath")));
    query.bindValue(9, event.value(QStringLiteral("propertyPath")));
    query.bindValue(10, event.value(QStringLiteral("valueType")));
    query.bindValue(11, jsonString(event.value(QStringLiteral("value"))));
    query.bindValue(12, event.value(QStringLiteral("creationSessionId")));
    query.bindValue(13, event.value(QStringLiteral("profileRank")));
    query.bindValue(14, event.value(QStringLiteral("source")));
    query.bindValue(15, event.value(QStringLiteral("createdAt")));
    if (!query.exec()) {
      database.rollback();
      return false;
    }
    query.finish();
  }
  if (!database.commit()) return false;
  pendingEvents_.clear();
  ++revision_;
  return true;
}

QVariantList PreferenceStore::readEvents(const QString& identityKey) const {
  QVariantList events;
  if (!openDatabase()) return events;
  QSqlQuery query(QSqlDatabase::database(connectionName_));
  query.prepare(QStringLiteral("SELECT event_id, value_json, creation_session_id, profile_rank, created_at "
                               "FROM preference_events WHERE account_scope = ? AND identity_key = ? "
                               "ORDER BY created_at DESC LIMIT ?"));
  query.bindValue(0, accountScope_);
  query.bindValue(1, identityKey);
  query.bindValue(2, kMaxEventsPerIdentity);
  if (!query.exec()) return events;
  while (query.next()) {
    const auto storedValue = fromJsonString(query.value(1).toString()).toList();
    const auto storedEventId = query.value(0).toString();
    const auto separator = storedEventId.indexOf(QLatin1Char('\x1f'));
    events.append(QVariantMap{{QStringLiteral("eventId"), separator >= 0 ? storedEventId.mid(separator + 1) : storedEventId},
                              {QStringLiteral("value"), storedValue.isEmpty() ? QVariant{} : storedValue.first()},
                              {QStringLiteral("creationSessionId"), query.value(2)},
                              {QStringLiteral("profileRank"), query.value(3)},
                              {QStringLiteral("createdAt"), query.value(4)}});
  }
  return events;
}

QVariantList PreferenceStore::readAllEvents() const {
  QVariantList events;
  if (!openDatabase()) return events;
  QSqlQuery query(QSqlDatabase::database(connectionName_));
  query.prepare(QStringLiteral("SELECT event_id, installation_id, component_id, component_family, component_version, "
                               "manifest_hash, semantic_path, property_path, value_type, value_json, "
                               "creation_session_id, profile_rank, source, created_at FROM preference_events WHERE account_scope = ? ORDER BY created_at ASC"));
  query.bindValue(0, accountScope_);
  if (!query.exec())
    return events;
  while (query.next()) {
    const auto values = fromJsonString(query.value(9).toString()).toList();
    const auto storedEventId = query.value(0).toString();
    const auto separator = storedEventId.indexOf(QLatin1Char('\x1f'));
    events.append(QVariantMap{{QStringLiteral("eventId"), separator >= 0 ? storedEventId.mid(separator + 1) : storedEventId},
                              {QStringLiteral("installationId"), query.value(1)},
                              {QStringLiteral("componentId"), query.value(2)},
                              {QStringLiteral("componentFamily"), query.value(3)},
                              {QStringLiteral("componentVersion"), query.value(4)},
                              {QStringLiteral("manifestHash"), query.value(5)},
                              {QStringLiteral("semanticPath"), query.value(6)},
                              {QStringLiteral("propertyPath"), query.value(7)},
                              {QStringLiteral("valueType"), query.value(8)},
                              {QStringLiteral("value"), values.isEmpty() ? QVariant{} : values.first()},
                              {QStringLiteral("creationSessionId"), query.value(10)},
                              {QStringLiteral("profileRank"), query.value(11)},
                              {QStringLiteral("source"), query.value(12)},
                              {QStringLiteral("createdAt"), query.value(13)}});
  }
  return events;
}

QVariantList PreferenceStore::exportFacts() {
  if (!flushPendingPreferences()) return {};
  return readAllEvents();
}

bool PreferenceStore::importFacts(const QVariantList& facts) {
  for (const auto& value : facts) {
    const auto fact = value.toMap();
    if (!recordConfirmedPropertyChange(fact)) return false;
  }
  return flushPendingPreferences() && compilePreferences();
}

QVariantList PreferenceStore::compileTopProfiles(const QList<QVariantMap>& events) const {
  struct Candidate {
    QVariantMap values;
    QSet<QString> sessions;
    double score = 0;
  };
  const auto now = QDateTime::currentMSecsSinceEpoch();
  QVariantList profiles;
  for (int profileRank = 0; profileRank < 3; ++profileRank) {
    QHash<QString, Candidate> candidates;
    for (const auto& event : events) {
      if (event.value(QStringLiteral("profileRank"), 0).toInt() != profileRank) continue;
      const auto valueKey = jsonString(event.value(QStringLiteral("value")));
      auto& candidate = candidates[valueKey];
      candidate.values.insert(QStringLiteral("value"), event.value(QStringLiteral("value")));
      candidate.sessions.insert(event.value(QStringLiteral("creationSessionId")).toString());
      candidate.score += scoreEvent(event, now);
    }
    if (candidates.isEmpty()) continue;
    QList<Candidate> ranked;
    for (auto it = candidates.cbegin(); it != candidates.cend(); ++it) ranked.append(it.value());
    std::sort(ranked.begin(), ranked.end(), [](const Candidate& left, const Candidate& right) {
      if (left.score != right.score) return left.score > right.score;
      return left.values.value(QStringLiteral("value")).toString() < right.values.value(QStringLiteral("value")).toString();
    });
    auto profile = ranked.first().values;
    profile.insert(QStringLiteral("rank"), profileRank);
    profiles.append(profile);
  }
  return profiles;
}

bool PreferenceStore::writeProfile(const QString& identityKey, const QVariantList& profiles) const {
  if (!openDatabase()) return false;
  QSqlQuery query(QSqlDatabase::database(connectionName_));
  query.prepare(QStringLiteral("INSERT INTO preference_profiles(identity_key, profile_json, revision, updated_at) "
                               "VALUES (?, ?, ?, ?) ON CONFLICT(identity_key) DO UPDATE SET "
                               "profile_json=excluded.profile_json, revision=excluded.revision, updated_at=excluded.updated_at"));
  query.bindValue(0, identityKey);
  query.bindValue(1, jsonString(profiles));
  query.bindValue(2, revision_);
  query.bindValue(3, QDateTime::currentMSecsSinceEpoch());
  const auto ok = query.exec();
  return ok;
}

QVariantMap PreferenceStore::systemDefaults(const QVariantMap& identity) const {
  QVariantMap result;
  const auto type = identity.value(QStringLiteral("valueType")).toString();
  if (type == QStringLiteral("color")) result.insert(QStringLiteral("value"), QStringLiteral("#ffffff"));
  else if (type == QStringLiteral("integer")) result.insert(QStringLiteral("value"), 0);
  else if (type == QStringLiteral("float")) result.insert(QStringLiteral("value"), 0.0);
  return result;
}

QVariantMap PreferenceStore::creationPreferences(const QVariantMap& identity, int rank) const {
  const auto key = makeIdentityKey(identity);
  if (!openDatabase()) return systemDefaults(identity);
  QSqlQuery query(QSqlDatabase::database(connectionName_));
  query.prepare(QStringLiteral("SELECT profile_json FROM preference_profiles WHERE identity_key = ?"));
  query.bindValue(0, key);
  if (query.exec() && query.next()) {
    const auto profiles = fromJsonString(query.value(0).toString()).toList();
    for (const auto& profile : profiles) {
      const auto candidate = profile.toMap();
      if (candidate.value(QStringLiteral("rank")).toInt() == rank) return candidate;
    }
  }
  const auto events = readEvents(key);
  QList<QVariantMap> maps;
  for (const auto& event : events) maps.append(event.toMap());
  const auto profiles = compileTopProfiles(maps);
  for (const auto& profile : profiles) {
    const auto candidate = profile.toMap();
    if (candidate.value(QStringLiteral("rank")).toInt() == rank) return candidate;
  }
  return systemDefaults(identity);
}

bool PreferenceStore::compilePreferences() {
  if (!flushPendingPreferences()) return false;
  if (!profileDirty_) return true;
  if (!openDatabase()) return false;
  QSqlQuery identities(QSqlDatabase::database(connectionName_));
  identities.prepare(QStringLiteral("SELECT DISTINCT identity_key FROM preference_events WHERE account_scope = ?"));
  identities.bindValue(0, accountScope_);
  if (!identities.exec()) return false;
  while (identities.next()) {
    const auto key = identities.value(0).toString();
    const auto events = readEvents(key);
    QList<QVariantMap> maps;
    for (const auto& event : events) maps.append(event.toMap());
    if (!writeProfile(key, compileTopProfiles(maps))) return false;
  }
  profileDirty_ = false;
  return true;
}

QVariantMap PreferenceStore::status() const {
  return QVariantMap{{QStringLiteral("pending"), pendingEvents_.size()},
                     {QStringLiteral("dirty"), profileDirty_},
                     {QStringLiteral("revision"), revision_},
                     {QStringLiteral("databasePath"), databasePath()}};
}

}  // namespace edward::desktop
