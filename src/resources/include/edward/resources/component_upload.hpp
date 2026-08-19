#pragma once

#include "edward/resources/component_package.hpp"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

namespace edward::resources {

struct AuthSession final {
  QString userId;
  QString username;
  QString accessToken;
};

struct ComponentUploadRequest final {
  QString endpoint;
  QString bearerToken;
  QJsonObject body;
};

class ComponentUploadClient final : public QObject {
  Q_OBJECT
 public:
  explicit ComponentUploadClient(QObject* parent = nullptr) : QObject(parent) {}
  static std::optional<ComponentUploadRequest> buildRequest(const QString& endpoint,
                                                             const ComponentPackage& package,
                                                             const AuthSession& session,
                                                             QString* error = nullptr);
  bool submit(const QString& endpoint, const ComponentPackage& package, const AuthSession& session);

 signals:
  void completed(bool success, QString message, QJsonObject response);

 private:
  QNetworkAccessManager network_;
};

}  // namespace edward::resources
