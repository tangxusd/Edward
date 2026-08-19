#include "edward/resources/component_upload.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

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

bool ComponentUploadClient::submit(const QString& endpoint, const ComponentPackage& package,
                                   const AuthSession& session) {
  QString error;
  const auto requestData = buildRequest(endpoint, package, session, &error);
  if (!requestData) {
    emit completed(false, error, {});
    return false;
  }
  QNetworkRequest request{QUrl(requestData->endpoint)};
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setRawHeader("Authorization", QByteArray("Bearer ") + requestData->bearerToken.toUtf8());
  auto* reply = network_.post(request, QJsonDocument(requestData->body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto body = reply->readAll();
    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(body, &parseError);
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    const auto message = success ? QStringLiteral("组件上传成功")
                                 : (reply->error() == QNetworkReply::NoError
                                        ? QStringLiteral("上传服务返回 HTTP %1").arg(status)
                                        : reply->errorString());
    emit completed(success, message, response.isObject() ? response.object() : QJsonObject{});
    reply->deleteLater();
  });
  return true;
}

}  // namespace edward::resources
