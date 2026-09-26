#include "edward/desktop/ai_memory_store.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>

namespace edward::desktop {
AiMemoryStore::AiMemoryStore(QString userRoot, QString projectRoot, QString sessionRoot)
    : userRoot_(std::move(userRoot)), projectRoot_(std::move(projectRoot)), sessionRoot_(std::move(sessionRoot)) {}
QString AiMemoryStore::path(MemoryScope scope) const {
  if (scope == MemoryScope::User) return QDir(userRoot_).filePath("agents.md");
  if (scope == MemoryScope::Project) return QDir(projectRoot_).filePath(".edward/memory.md");
  return QDir(sessionRoot_).filePath("memory.md");
}
bool AiMemoryStore::safeText(const QString& text) {
  const auto v = text.toLower();
  return !v.contains("api_key") && !v.contains("api-key") && !v.contains("access_token") &&
         !v.contains("access-token") && !v.contains("password") && !v.contains("authorization") &&
         !v.contains("bearer ") && !v.contains("secret") && !v.contains("credential") &&
         !v.contains("capability") && !v.contains("permission");
}
QString AiMemoryStore::read(MemoryScope scope, QString* error) const {
  QFile file(path(scope));
  if (!file.exists()) return {};
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { if (error) *error = file.errorString(); return {}; }
  return QString::fromUtf8(file.readAll());
}
bool AiMemoryStore::append(MemoryScope scope, const QString& text, QString* error) {
  if (text.isEmpty() || !safeText(text)) { if (error) *error = "memory contains protected fields"; return false; }
  const auto target = path(scope); if (!QDir().mkpath(QFileInfo(target).absolutePath())) { if (error) *error = "memory directory cannot be created"; return false; }
  QSaveFile file(target); if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = file.errorString(); return false; }
  const auto previous = read(scope); file.write(previous.toUtf8()); if (!previous.isEmpty() && !previous.endsWith('\n')) file.write("\n"); file.write(text.toUtf8()); file.write("\n");
  if (!file.commit()) { if (error) *error = file.errorString(); return false; } return true;
}
bool AiMemoryStore::clear(MemoryScope scope, QString* error) { QFile file(path(scope)); if (!file.exists()) return true; if (!file.remove()) { if (error) *error = file.errorString(); return false; } return true; }
bool AiMemoryStore::upsert(const MemoryEntry& entry, QString* error) {
  if (entry.id.isEmpty()) { if (error) *error = "memory id is required"; return false; }
  return append(entry.scope, QStringLiteral("[%1] %2").arg(entry.id, entry.value), error);
}
bool AiMemoryStore::remove(const QString& id, QString* error) {
  if (id.isEmpty()) { if (error) *error = "memory id is required"; return false; }
  for (const auto scopeValue : {MemoryScope::User, MemoryScope::Project, MemoryScope::Session}) {
    const auto content = read(scopeValue, error);
    if (!error || error->isEmpty()) {
      QStringList kept;
      for (const auto& line : content.split('\n')) if (!line.startsWith(QStringLiteral("[%1] ").arg(id))) kept.push_back(line);
      const auto target = path(scopeValue);
      if (!QFileInfo::exists(target)) continue;
      QSaveFile file(target); if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = file.errorString(); return false; }
      file.write(kept.join('\n').toUtf8()); if (!file.commit()) { if (error) *error = file.errorString(); return false; }
    }
  }
  return true;
}
}  // namespace edward::desktop
