#include "edward/resources/model_chat_client.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <algorithm>

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
  if (config.protocol == QStringLiteral("openai-completions")) {
    return ModelChatRequest{config.endpoint, config.apiKey,
                            QJsonObject{{"model", config.model},
                                        {"messages", QJsonArray{
                                            QJsonObject{{"role", "system"}, {"content", systemPrompt}},
                                            QJsonObject{{"role", "user"}, {"content", userPrompt}},
                                        }},
                                        {"temperature", 0.2}},
                            {{"Authorization", QByteArray("Bearer ") + config.apiKey.toUtf8()}}};
  }
  if (config.protocol == QStringLiteral("openai-responses")) {
    return ModelChatRequest{config.endpoint, config.apiKey,
                            QJsonObject{{"model", config.model}, {"instructions", systemPrompt},
                                        {"input", userPrompt}},
                            {{"Authorization", QByteArray("Bearer ") + config.apiKey.toUtf8()}}};
  }
  if (config.protocol == QStringLiteral("anthropic-messages")) {
    return ModelChatRequest{config.endpoint, config.apiKey,
                            QJsonObject{{"model", config.model}, {"max_tokens", 256},
                                        {"system", systemPrompt}, {"messages", QJsonArray{
                                            QJsonObject{{"role", "user"}, {"content", userPrompt}},
                                        }}},
                            {{"x-api-key", config.apiKey.toUtf8()}, {"anthropic-version", "2023-06-01"}}};
  }
  return fail(QStringLiteral("不支持的 AI API 协议"));
}

std::optional<QString> ModelChatClient::extractAssistantText(const QJsonObject& response,
                                                             const QString& protocol, QString* error) {
  if (protocol == QStringLiteral("openai-responses")) {
    const auto outputText = response.value(QStringLiteral("output_text")).toString();
    if (!outputText.isEmpty()) return outputText;
    for (const auto& output : response.value(QStringLiteral("output")).toArray()) {
      for (const auto& content : output.toObject().value(QStringLiteral("content")).toArray()) {
        const auto text = content.toObject().value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) return text;
      }
    }
    if (error) *error = QStringLiteral("Responses 响应缺少输出文本");
    return std::nullopt;
  }
  if (protocol == QStringLiteral("anthropic-messages")) {
    for (const auto& content : response.value(QStringLiteral("content")).toArray()) {
      const auto text = content.toObject().value(QStringLiteral("text")).toString();
      if (!text.isEmpty()) return text;
    }
    if (error) *error = QStringLiteral("Anthropic 响应缺少文本块");
    return std::nullopt;
  }
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

QString ModelChatClient::modelsEndpoint(const QString& chatEndpoint) {
  QUrl url(chatEndpoint.trimmed());
  auto path = url.path();
  const auto suffixes = {QStringLiteral("/chat/completions"), QStringLiteral("/responses"),
                         QStringLiteral("/completions"), QStringLiteral("/messages")};
  for (const auto& suffix : suffixes) {
    if (path.endsWith(suffix)) { path.chop(suffix.size()); break; }
  }
  if (!path.endsWith('/')) path += '/';
  url.setPath(path + QStringLiteral("models"));
  url.setQuery({});
  return url.toString();
}

QStringList ModelChatClient::extractModelIds(const QJsonObject& response, QString* error) {
  const auto data = response.value(QStringLiteral("data")).toArray();
  if (data.isEmpty()) {
    if (error) *error = QStringLiteral("模型列表响应缺少 data");
    return {};
  }
  QStringList ids;
  for (const auto& value : data) {
    const auto id = value.toObject().value(QStringLiteral("id")).toString().trimmed();
    if (!id.isEmpty()) ids.append(id);
  }
  ids.removeDuplicates();
  std::sort(ids.begin(), ids.end());
  if (ids.isEmpty() && error) *error = QStringLiteral("模型列表没有有效模型 ID");
  return ids;
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
  // 复杂的组件生成可能需要较长推理时间；在真正接入 SSE 分片前，
  // 给完整响应保留 180 秒上限，避免 60 秒时误判为失败。
  request.setTransferTimeout(180000);
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  for (auto it = requestData->headers.cbegin(); it != requestData->headers.cend(); ++it)
    request.setRawHeader(it.key(), it.value());
  auto* reply = network_.post(request, QJsonDocument(requestData->body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply, protocol = config.protocol] {
    const auto body = reply->readAll();
    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(body, &parseError);
    if (reply->error() != QNetworkReply::NoError) {
      emit completed(false, reply->errorString());
    } else if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
      emit completed(false, QStringLiteral("AI 响应不是有效 JSON"));
    } else {
      QString error;
      const auto text = extractAssistantText(response.object(), protocol, &error);
      emit completed(text.has_value(), text ? *text : error);
    }
    reply->deleteLater();
  });
  return true;
}

bool ModelChatClient::requestModels(const ModelChatConfig& config) {
  const QUrl endpoint(modelsEndpoint(config.endpoint));
  if (!endpoint.isValid() || endpoint.scheme() != QStringLiteral("https") || endpoint.host().isEmpty() || config.apiKey.isEmpty()) {
    emit modelsCompleted(false, {}, QStringLiteral("模型列表请求需要 HTTPS 端点和 API Key"));
    return false;
  }
  QNetworkRequest request{endpoint};
  if (config.protocol == QStringLiteral("anthropic-messages")) {
    request.setRawHeader("x-api-key", config.apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
  } else {
    request.setRawHeader("Authorization", QByteArray("Bearer ") + config.apiKey.toUtf8());
  }
  auto* reply = network_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto body = reply->readAll();
    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(body, &parseError);
    if (reply->error() != QNetworkReply::NoError) {
      emit modelsCompleted(false, {}, reply->errorString());
    } else if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
      emit modelsCompleted(false, {}, QStringLiteral("模型列表响应不是有效 JSON"));
    } else {
      QString error;
      const auto ids = extractModelIds(response.object(), &error);
      emit modelsCompleted(!ids.isEmpty(), ids, ids.isEmpty() ? error : QStringLiteral("模型列表已更新"));
    }
    reply->deleteLater();
  });
  return true;
}

}  // namespace edward::resources
