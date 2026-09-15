#include "edward/resources/preference_sync_client.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace edward::resources {
namespace {

bool nonEmpty(const QVariantMap& value, const char* key) {
  return !value.value(QString::fromLatin1(key)).toString().trimmed().isEmpty();
}

bool fail(QString* error, const QString& message) {
  if (error) *error = message;
  return false;
}

}  // namespace

bool PreferenceSyncClient::validateFact(const QVariantMap& fact, QString* error) {
  static const char* required[] = {"eventId", "installationId", "componentId", "componentFamily",
                                   "componentVersion", "manifestHash", "semanticPath", "propertyPath",
                                   "valueType", "value", "creationSessionId", "source"};
  for (const auto* key : required) {
    if (!nonEmpty(fact, key)) return fail(error, QStringLiteral("偏好事实缺少字段：%1").arg(QString::fromLatin1(key)));
  }
  if (fact.value(QStringLiteral("source")).toString() != QStringLiteral("user-confirmed"))
    return fail(error, QStringLiteral("偏好事实来源无效"));
  for (const auto& forbidden : {QStringLiteral("projectId"), QStringLiteral("projectPath"),
                               QStringLiteral("projectName"), QStringLiteral("timelineId")}) {
    if (fact.contains(forbidden)) return fail(error, QStringLiteral("偏好事实不得包含项目字段"));
  }
  const auto type = fact.value(QStringLiteral("valueType")).toString();
  const auto value = fact.value(QStringLiteral("value"));
  if (type == QStringLiteral("color")) {
    const auto color = value.toString();
    if (!color.startsWith(QLatin1Char('#')) || (color.size() != 7 && color.size() != 9))
      return fail(error, QStringLiteral("颜色值无效"));
  } else if (type == QStringLiteral("integer")) {
    if (!value.canConvert<int>()) return fail(error, QStringLiteral("整数值无效"));
  } else if (type == QStringLiteral("float")) {
    if (!value.canConvert<double>()) return fail(error, QStringLiteral("浮点值无效"));
  } else if (type != QStringLiteral("string") && type != QStringLiteral("boolean")) {
    return fail(error, QStringLiteral("偏好值类型无效"));
  }
  return true;
}

QVariantList PreferenceSyncClient::mergeFacts(const QVariantList& localFacts,
                                              const QVariantList& remoteFacts) {
  QVariantList merged;
  QSet<QString> ids;
  const auto append = [&merged, &ids](const QVariant& value) {
    const auto fact = value.toMap();
    QString error;
    if (!PreferenceSyncClient::validateFact(fact, &error)) return;
    const auto id = fact.value(QStringLiteral("eventId")).toString();
    if (ids.contains(id)) return;
    ids.insert(id);
    merged.push_back(fact);
  };
  for (const auto& value : localFacts) append(value);
  for (const auto& value : remoteFacts) append(value);
  return merged;
}

QJsonObject PreferenceSyncClient::buildUploadPayload(const QVariantList& facts) {
  QJsonArray array;
  for (const auto& fact : mergeFacts({}, facts)) array.push_back(QJsonObject::fromVariantMap(fact.toMap()));
  return QJsonObject{{"schemaVersion", 1}, {"manifestVersion", 1}, {"facts", array}};
}

QVariantList PreferenceSyncClient::parseDownloadPayload(const QJsonObject& payload, QString* error) {
  if (payload.value(QStringLiteral("schemaVersion")).toInt() != 1 ||
      payload.value(QStringLiteral("manifestVersion")).toInt() != 1)
    return fail(error, QStringLiteral("偏好同步版本不兼容")), QVariantList{};
  const auto facts = payload.value(QStringLiteral("facts"));
  if (!facts.isArray()) return fail(error, QStringLiteral("偏好同步载荷缺少 facts")), QVariantList{};
  QVariantList result;
  for (const auto& value : facts.toArray()) {
    const auto fact = value.toObject().toVariantMap();
    if (!validateFact(fact, error)) return {};
    result.push_back(fact);
  }
  return mergeFacts({}, result);
}

bool PreferenceSyncClient::upload(const QString& projectUrl, const QString& anonKey,
                                  const QString& accessToken, const QVariantList& facts) {
  if (projectUrl.trimmed().isEmpty() || anonKey.isEmpty() || accessToken.isEmpty()) {
    emit uploadCompleted(false, QStringLiteral("偏好上传需要登录会话"));
    return false;
  }
  QUrl endpoint(projectUrl.trimmed());
  endpoint.setPath(QStringLiteral("/functions/v1/preference-sync"));
  QNetworkRequest request(endpoint);
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setRawHeader("apikey", anonKey.toUtf8());
  request.setRawHeader("Authorization", (QStringLiteral("Bearer ") + accessToken).toUtf8());
  auto* reply = network_.post(request, QJsonDocument(buildUploadPayload(facts)).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const bool ok = reply->error() == QNetworkReply::NoError &&
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 200 &&
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 300;
    emit uploadCompleted(ok, ok ? QStringLiteral("偏好已上传") : QStringLiteral("偏好上传失败：%1").arg(reply->errorString()));
    reply->deleteLater();
  });
  return true;
}

bool PreferenceSyncClient::download(const QString& projectUrl, const QString& anonKey,
                                    const QString& accessToken) {
  if (projectUrl.trimmed().isEmpty() || anonKey.isEmpty() || accessToken.isEmpty()) {
    emit downloadCompleted(false, {}, QStringLiteral("偏好下载需要登录会话"));
    return false;
  }
  QUrl endpoint(projectUrl.trimmed());
  endpoint.setPath(QStringLiteral("/functions/v1/preference-sync"));
  QNetworkRequest request(endpoint);
  request.setRawHeader("apikey", anonKey.toUtf8());
  request.setRawHeader("Authorization", (QStringLiteral("Bearer ") + accessToken).toUtf8());
  auto* reply = network_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    QString error;
    const auto object = QJsonDocument::fromJson(reply->readAll()).object();
    const auto facts = reply->error() == QNetworkReply::NoError ? parseDownloadPayload(object, &error) : QVariantList{};
    const bool ok = reply->error() == QNetworkReply::NoError && error.isEmpty();
    emit downloadCompleted(ok, facts, ok ? QStringLiteral("偏好已下载") : QStringLiteral("偏好下载失败：%1").arg(error.isEmpty() ? reply->errorString() : error));
    reply->deleteLater();
  });
  return true;
}

}  // namespace edward::resources
