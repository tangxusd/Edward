#include "edward/resources/model_chat_client.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace edward::resources {

std::optional<ModelChatRequest> ModelChatClient::buildRequest(const ModelChatConfig& config,
                                                              const QString& systemPrompt,
                                                              const QString& userPrompt,
                                                              QString* error) {
  const auto fail = [error](const QString& message) -> std::optional<ModelChatRequest> {
    if (error) *error = message;
    return std::nullopt;
  };
  const QUrl endpoint(config.endpoint);
  if (!endpoint.isValid() || endpoint.scheme() != QStringLiteral("https") || endpoint.host().isEmpty())
    return fail(QStringLiteral("AI 模型端点必须是 HTTPS URL"));
  if (config.apiKey.isEmpty()) return fail(QStringLiteral("AI 模型 API 密钥不能为空"));
  if (config.model.isEmpty()) return fail(QStringLiteral("AI 模型 ID 不能为空"));
  if (systemPrompt.isEmpty() || userPrompt.isEmpty())
    return fail(QStringLiteral("AI 请求提示词不能为空"));
  return ModelChatRequest{config.endpoint, config.apiKey,
                          QJsonObject{{"model", config.model},
                                      {"messages", QJsonArray{
                                          QJsonObject{{"role", "system"}, {"content", systemPrompt}},
                                          QJsonObject{{"role", "user"}, {"content", userPrompt}},
                                      }},
                                      {"temperature", 0.2}}};
}

std::optional<QString> ModelChatClient::extractAssistantText(const QJsonObject& response, QString* error) {
  const auto choices = response.value(QStringLiteral("choices")).toArray();
  if (choices.isEmpty()) {
    if (error) *error = QStringLiteral("AI 响应缺少 choices");
    return std::nullopt;
  }
  const auto content = choices.first().toObject().value(QStringLiteral("message")).toObject()
                           .value(QStringLiteral("content"));
  if (!content.isString() || content.toString().isEmpty()) {
    if (error) *error = QStringLiteral("AI 响应缺少 assistant 文本");
    return std::nullopt;
  }
  return content.toString();
}

bool ModelChatClient::request(const ModelChatConfig& config, const QString& systemPrompt,
                             const QString& userPrompt) {
  QString error;
  const auto requestData = buildRequest(config, systemPrompt, userPrompt, &error);
  if (!requestData) {
    emit completed(false, error);
    return false;
  }
  QNetworkRequest request{QUrl(requestData->endpoint)};
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setRawHeader("Authorization", QByteArray("Bearer ") + requestData->apiKey.toUtf8());
  auto* reply = network_.post(request, QJsonDocument(requestData->body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto body = reply->readAll();
    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(body, &parseError);
    if (reply->error() != QNetworkReply::NoError) {
      emit completed(false, reply->errorString());
    } else if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
      emit completed(false, QStringLiteral("AI 响应不是有效 JSON"));
    } else {
      QString error;
      const auto text = extractAssistantText(response.object(), &error);
      emit completed(text.has_value(), text ? *text : error);
    }
    reply->deleteLater();
  });
  return true;
}

}  // namespace edward::resources
