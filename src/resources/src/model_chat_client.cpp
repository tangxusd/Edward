#include "edward/resources/model_chat_client.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>
#include <QTimer>
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

bool ModelChatClient::requestStreaming(const ModelChatConfig& config, const QString& systemPrompt,
                                       const QString& userPrompt) {
  cancelStreaming();
  const auto requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  QString error;
  auto requestData = buildRequest(config, systemPrompt, userPrompt, &error);
  if (!requestData) {
    emit streamError(requestId, QStringLiteral("REQUEST_INVALID"), error);
    emit completed(false, error);
    return false;
  }
  emit streamStarted(requestId);
  auto body = requestData->body;
  body.insert(QStringLiteral("stream"), true);
  QNetworkRequest request{QUrl(requestData->endpoint)};
  request.setTransferTimeout(180000);
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  for (auto it = requestData->headers.cbegin(); it != requestData->headers.cend(); ++it)
    request.setRawHeader(it.key(), it.value());
  auto* reply = network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  activeStreamingReply_ = reply;
  activeStreamingRequestId_ = requestId;
  activeStreamingCancelled_ = false;
  auto* buffer = new QByteArray;
  auto* aggregate = new QString;
  auto* sequence = new qint64(0);
  auto* upstreamSequence = new qint64(0);
  auto* terminal = new bool(false);
  auto* eventCount = new int(0);
  auto* firstByte = new bool(false);
  auto* overallTimer = new QTimer(reply);
  auto* firstByteTimer = new QTimer(reply);
  auto* idleTimer = new QTimer(reply);
  overallTimer->setSingleShot(true);
  firstByteTimer->setSingleShot(true);
  idleTimer->setSingleShot(true);
  overallTimer->setInterval(180000);
  firstByteTimer->setInterval(15000);
  idleTimer->setInterval(30000);
  overallTimer->start();
  firstByteTimer->start();
  idleTimer->start();
  connect(overallTimer, &QTimer::timeout, this, [this, reply, terminal, requestId] {
    if (*terminal) return;
    *terminal = true;
    emit streamError(requestId, QStringLiteral("TOTAL_TIMEOUT"), QStringLiteral("AI 流式请求超过总时限"));
    emit completed(false, QStringLiteral("AI 流式请求超过总时限"));
    reply->abort();
  });
  connect(firstByteTimer, &QTimer::timeout, this, [this, reply, terminal, requestId] {
    if (*terminal) return;
    *terminal = true;
    emit streamError(requestId, QStringLiteral("FIRST_BYTE_TIMEOUT"), QStringLiteral("AI 模型未在首字节时限内响应"));
    emit completed(false, QStringLiteral("AI 模型未在首字节时限内响应"));
    reply->abort();
  });
  connect(idleTimer, &QTimer::timeout, this, [this, reply, terminal, requestId] {
    if (*terminal) return;
    *terminal = true;
    emit streamError(requestId, QStringLiteral("IDLE_TIMEOUT"), QStringLiteral("AI 流式响应空闲超时"));
    emit completed(false, QStringLiteral("AI 流式响应空闲超时"));
    reply->abort();
  });
  connect(reply, &QNetworkReply::readyRead, this, [this, reply, buffer, aggregate, sequence, upstreamSequence, terminal,
                                                     eventCount, firstByte, firstByteTimer, idleTimer,
                                                     requestId, protocol = config.protocol] {
    if (*terminal) return;
    const auto incoming = reply->readAll();
    if (incoming.isEmpty()) return;
    if (!*firstByte) { *firstByte = true; firstByteTimer->stop(); }
    idleTimer->start();
    *buffer += incoming;
    while (true) {
      const auto end = buffer->indexOf('\n');
      if (end < 0) break;
      auto line = buffer->left(end);
      buffer->remove(0, end + 1);
      if (line.endsWith('\r')) line.chop(1);
      line = line.trimmed();
      if (!line.startsWith("data:")) continue;
      line = line.mid(5).trimmed();
      if (line == "[DONE]") continue;
      if (line.startsWith('{') && line.contains("\"error\"")) {
        *terminal = true;
        emit streamError(requestId, QStringLiteral("MODEL_ERROR"), QStringLiteral("AI 流式响应返回错误"));
        emit completed(false, QStringLiteral("AI 流式响应返回错误"));
        reply->abort();
        return;
      }
      QJsonParseError parseError;
      const auto document = QJsonDocument::fromJson(line, &parseError);
      if (parseError.error != QJsonParseError::NoError || !document.isObject()) continue;
      const auto event = document.object();
      if (++(*eventCount) > 4096) {
        *terminal = true;
        emit streamError(requestId, QStringLiteral("EVENT_LIMIT"), QStringLiteral("AI 流式事件数量超过限制"));
        emit completed(false, QStringLiteral("AI 流式事件数量超过限制"));
        reply->abort();
        return;
      }
      const auto incomingSequence = event.value(QStringLiteral("sequence")).toVariant().toLongLong() > 0
          ? event.value(QStringLiteral("sequence")).toVariant().toLongLong()
          : event.value(QStringLiteral("index")).toVariant().toLongLong();
      if (incomingSequence > 0) {
        if (incomingSequence <= *upstreamSequence) continue;
        *upstreamSequence = incomingSequence;
      }
      QString delta;
      if (protocol == QStringLiteral("anthropic-messages")) {
        if (event.value(QStringLiteral("type")).toString() == QStringLiteral("content_block_delta"))
          delta = event.value(QStringLiteral("delta")).toObject().value(QStringLiteral("text")).toString();
      } else if (protocol == QStringLiteral("openai-responses")) {
        delta = event.value(QStringLiteral("delta")).toString();
        if (delta.isEmpty() && event.value(QStringLiteral("type")).toString() == QStringLiteral("response.output_text.delta"))
          delta = event.value(QStringLiteral("delta")).toString();
      } else {
        const auto choices = event.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) delta = choices.first().toObject().value(QStringLiteral("delta")).toObject().value(QStringLiteral("content")).toString();
      }
      if (delta.isEmpty()) continue;
      if (aggregate->size() + delta.size() > 256 * 1024) {
        *terminal = true;
        emit streamError(requestId, QStringLiteral("RESPONSE_TOO_LARGE"), QStringLiteral("AI 流式响应超过大小限制"));
        emit completed(false, QStringLiteral("AI 流式响应超过大小限制"));
        reply->abort();
        return;
      }
      aggregate->append(delta);
      emit streamDelta(requestId, ++(*sequence), delta);
      emit chunk(delta);
    }
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply, buffer, aggregate, sequence, upstreamSequence, terminal,
                                                   eventCount, firstByte, overallTimer, firstByteTimer, idleTimer,
                                                   requestId, protocol = config.protocol] {
    if (!reply->isFinished()) return;
    const auto superseded = activeStreamingReply_ != reply || activeStreamingRequestId_ != requestId;
    const auto wasTerminal = *terminal || activeStreamingCancelled_ || superseded;
    if (activeStreamingReply_ == reply) {
      activeStreamingReply_ = nullptr;
      activeStreamingRequestId_.clear();
      activeStreamingCancelled_ = false;
    }
    if (wasTerminal) {
      overallTimer->stop(); firstByteTimer->stop(); idleTimer->stop();
      delete buffer; delete aggregate; delete sequence; delete upstreamSequence; delete terminal; delete eventCount; delete firstByte; reply->deleteLater();
      return;
    }
    if (!buffer->isEmpty()) {
      const auto tail = buffer->trimmed();
      if (tail.startsWith("data:")) {
        const auto payload = tail.mid(5).trimmed();
        if (payload != "[DONE]") {
          QJsonParseError parseError;
          const auto document = QJsonDocument::fromJson(payload, &parseError);
          if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const auto event = document.object();
            QString delta;
            if (event.value(QStringLiteral("type")).toString() == QStringLiteral("content_block_delta"))
              delta = event.value(QStringLiteral("delta")).toObject().value(QStringLiteral("text")).toString();
            if (delta.isEmpty()) delta = event.value(QStringLiteral("delta")).toString();
            if (delta.isEmpty() && !event.value(QStringLiteral("choices")).toArray().isEmpty())
              delta = event.value(QStringLiteral("choices")).toArray().first().toObject().value(QStringLiteral("delta")).toObject().value(QStringLiteral("content")).toString();
            if (!delta.isEmpty() && aggregate->size() + delta.size() <= 256 * 1024) {
              aggregate->append(delta);
              emit streamDelta(requestId, ++(*sequence), delta);
              emit chunk(delta);
            }
          }
        }
      }
    }
    const auto errorString = reply->error() == QNetworkReply::NoError ? QString{} : reply->errorString();
    if (!errorString.isEmpty()) {
      emit streamError(requestId, QStringLiteral("NETWORK_ERROR"), errorString);
      emit completed(false, errorString);
    } else if (aggregate->isEmpty()) {
      const auto message = QStringLiteral("AI 流式响应为空");
      emit streamError(requestId, QStringLiteral("EMPTY_RESPONSE"), message);
      emit completed(false, message);
    } else {
      emit streamDone(requestId, *aggregate);
      emit completed(true, *aggregate);
    }
    overallTimer->stop(); firstByteTimer->stop(); idleTimer->stop();
    delete buffer; delete aggregate; delete sequence; delete upstreamSequence; delete terminal; delete eventCount; delete firstByte; reply->deleteLater();
  });
  return true;
}

void ModelChatClient::cancelStreaming() {
  if (!activeStreamingReply_) return;
  const auto requestId = activeStreamingRequestId_;
  activeStreamingCancelled_ = true;
  emit streamCancelled(requestId);
  activeStreamingReply_->abort();
  activeStreamingReply_ = nullptr;
  activeStreamingRequestId_.clear();
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
