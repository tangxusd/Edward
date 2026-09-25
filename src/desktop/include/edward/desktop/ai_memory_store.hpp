#pragma once

#include <QString>

namespace edward::desktop {
enum class MemoryScope { User, Project, Session };
struct MemoryEntry final { QString id; MemoryScope scope = MemoryScope::Session; QString value; };

class AiMemoryStore final {
 public:
  AiMemoryStore(QString userRoot, QString projectRoot, QString sessionRoot);
  [[nodiscard]] QString read(MemoryScope scope, QString* error = nullptr) const;
  bool append(MemoryScope scope, const QString& text, QString* error = nullptr);
  bool clear(MemoryScope scope, QString* error = nullptr);
  bool upsert(const MemoryEntry& entry, QString* error = nullptr);
  bool remove(const QString& id, QString* error = nullptr);

 private:
  QString path(MemoryScope scope) const;
  static bool safeText(const QString& text);
  QString userRoot_, projectRoot_, sessionRoot_;
};
}  // namespace edward::desktop
