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

  Q_INVOKABLE QVariantMap creationPreferences(const QVariantMap& identity, int rank = 0) const;
  Q_INVOKABLE bool recordConfirmedPropertyChange(const QVariantMap& observation);
  Q_INVOKABLE bool flushPendingPreferences();
  Q_INVOKABLE bool compilePreferences();
  Q_INVOKABLE QVariantMap status() const;
  Q_INVOKABLE QVariantList exportFacts();
  Q_INVOKABLE bool importFacts(const QVariantList& facts);

  void setAccountScope(const QString& accountId);
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
  QVariantList readAllEvents() const;
  bool writeProfile(const QString& identityKey, const QVariantList& profiles) const;
  void closeDatabase() const;

  mutable QString databasePathOverride_;
  mutable QString connectionName_;
  mutable QVariantList pendingEvents_;
  mutable bool profileDirty_ = false;
  mutable qlonglong revision_ = 0;
  mutable QString accountScope_ = QStringLiteral("anonymous");
};

}  // namespace edward::desktop
