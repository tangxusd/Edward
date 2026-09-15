#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace edward::desktop {

class PreferenceStore final : public QObject {
  Q_OBJECT

 public:
  explicit PreferenceStore(QObject* parent = nullptr);
  ~PreferenceStore() override;

  QVariantMap creationPreferences(const QVariantMap& identity) const;
  bool recordConfirmedPropertyChange(const QVariantMap& observation);
  bool flushPendingPreferences();
  bool compilePreferences();
  QVariantMap status() const;

  void setDatabasePathForTests(const QString& path);

 private:
  bool openDatabase() const;
  bool ensureSchema() const;
  QString databasePath() const;
  QString installationId() const;
  QString makeIdentityKey(const QVariantMap& identity) const;
  bool isValidObservation(const QVariantMap& observation, QString* error) const;
  QVariantList compileTopProfiles(const QList<QVariantMap>& events) const;
  QVariantMap systemDefaults(const QVariantMap& identity) const;
  QVariantList readEvents(const QString& identityKey) const;
  bool writeProfile(const QString& identityKey, const QVariantList& profiles) const;
  void closeDatabase() const;

  mutable QString databasePathOverride_;
  mutable QString connectionName_;
  mutable QVariantList pendingEvents_;
  mutable bool profileDirty_ = false;
  mutable qlonglong revision_ = 0;
};

}  // namespace edward::desktop
