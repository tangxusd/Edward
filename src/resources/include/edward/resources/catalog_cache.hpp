#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace edward::resources {

enum class CatalogType { Color, Font };

struct CatalogSession {
  QString accountId;
  QString installationId;
  QString projectId;
  CatalogType type = CatalogType::Color;
  QString revision;
};

struct ColorValue {
  QString semanticPath;
  QString valueType;
  QJsonObject value;
  QString contentHash;
};

struct FontRecord {
  QString fontId;
  QString displayName;
  QString familyName;
  QString styleName;
  int weight = 400;
  QString previewObjectKey;
  QString installObjectKey;
  QString contentHash;
  QString resourceVersion;
};

struct CatalogSyncResult {
  bool ok = false;
  bool changed = false;
  QString revision;
  QString error;
};

class CatalogCache final {
 public:
  explicit CatalogCache(QString databasePath = {});
  ~CatalogCache();

  bool open(QString* error = nullptr);
  CatalogSession openSession(const QString& accountId, const QString& projectId, CatalogType type);
  CatalogSyncResult checkRevisionOnce(CatalogSession& session, const QJsonObject& manifest);
  bool storeColor(const CatalogSession& session, const ColorValue& color);
  bool storeFont(const CatalogSession& session, const FontRecord& font);
  std::optional<ColorValue> readColor(const CatalogSession& session, const QString& semanticPath) const;
  std::optional<FontRecord> readFont(const CatalogSession& session, const QString& fontId) const;
  bool protectProjectReference(const CatalogSession& session, const QString& contentHash);
  int clearUnreferenced(int maxRows = 64);
  QString databasePath() const { return databasePath_; }

 private:
  bool ensureSchema(QString* error);
  QString key(const CatalogSession& session) const;
  QString databasePath_;
  QString connectionName_;
  QSqlDatabase database_;
};

}  // namespace edward::resources
