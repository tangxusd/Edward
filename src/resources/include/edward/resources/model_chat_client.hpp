#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace edward::resources {

struct ModelChatConfig final {
  QString endpoint;
  QString apiKey;
  QString model;
};

struct ModelChatRequest final {
  QString endpoint;
  QString apiKey;
  QJsonObject body;
};

class ModelChatClient final {
 public:
  static std::optional<ModelChatRequest> buildRequest(const ModelChatConfig& config,
                                                       const QString& systemPrompt,
                                                       const QString& userPrompt,
                                                       QString* error = nullptr);
  static std::optional<QString> extractAssistantText(const QJsonObject& response,
                                                      QString* error = nullptr);
};

}  // namespace edward::resources
