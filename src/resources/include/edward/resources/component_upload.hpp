#pragma once

#include "edward/resources/component_package.hpp"

#include <QJsonObject>
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

class ComponentUploadClient final {
 public:
  static std::optional<ComponentUploadRequest> buildRequest(const QString& endpoint,
                                                             const ComponentPackage& package,
                                                             const AuthSession& session,
                                                             QString* error = nullptr);
};

}  // namespace edward::resources
