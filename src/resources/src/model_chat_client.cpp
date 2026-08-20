#include "edward/resources/model_chat_client.hpp"

#include <QJsonArray>
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

}  // namespace edward::resources
