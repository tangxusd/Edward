#pragma once

#include <QJsonObject>
#include <QObject>
#include <QNetworkAccessManager>
#include <QVariantList>

namespace edward::resources {

class PreferenceSyncClient final : public QObject {
  Q_OBJECT
 public:
  explicit PreferenceSyncClient(QObject* parent = nullptr) : QObject(parent) {}
  static bool validateFact(const QVariantMap& fact, QString* error = nullptr);
  static QVariantList mergeFacts(const QVariantList& localFacts,
                                 const QVariantList& remoteFacts);
  static QJsonObject buildUploadPayload(const QVariantList& facts);
  static QVariantList parseDownloadPayload(const QJsonObject& payload,
                                           QString* error = nullptr);
  bool upload(const QString& projectUrl, const QString& anonKey, const QString& accessToken,
              const QVariantList& facts);
  bool download(const QString& projectUrl, const QString& anonKey, const QString& accessToken);

 signals:
  void uploadCompleted(bool success, QString message);
  void downloadCompleted(bool success, QVariantList facts, QString message);

 private:
  QNetworkAccessManager network_;
};

}  // namespace edward::resources
