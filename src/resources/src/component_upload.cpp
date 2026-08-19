#include "edward/resources/component_upload.hpp"

#include <QUrl>
#include <QJsonArray>

namespace edward::resources {

std::optional<ComponentUploadRequest> ComponentUploadClient::buildRequest(const QString& endpoint,
                                                                            const ComponentPackage& package,
                                                                            const AuthSession& session,
                                                                            QString* error) {
  const auto fail = [error](const QString& message) -> std::optional<ComponentUploadRequest> {
    if (error) *error = message;
    return std::nullopt;
  };
  const QUrl url(endpoint);
  if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty())
    return fail(QStringLiteral("upload endpoint must use HTTPS"));
  if (session.userId.isEmpty() || session.username.isEmpty() || session.accessToken.isEmpty())
    return fail(QStringLiteral("authenticated user session is required"));
  if (!package.validate(error)) return std::nullopt;
  QJsonArray assets;
  for (const auto& asset : package.assets) assets.append(asset);
  return ComponentUploadRequest{
      endpoint,
      session.accessToken,
      QJsonObject{{"resourceId", package.resourceId}, {"displayName", package.displayName},
                  {"userId", session.userId}, {"username", session.username},
                  {"component", package.component.toJson()}, {"pluginId", package.pluginId},
                  {"pluginVersion", package.pluginVersion}, {"thumbnail", package.thumbnail},
                  {"assets", assets}},
  };
}

}  // namespace edward::resources
