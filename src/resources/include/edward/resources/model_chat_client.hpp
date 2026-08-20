#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
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

class ModelChatClient final : public QObject {
  Q_OBJECT
 public:
  explicit ModelChatClient(QObject* parent = nullptr) : QObject(parent) {}
  static std::optional<ModelChatRequest> buildRequest(const ModelChatConfig& config,
                                                       const QString& systemPrompt,
                                                       const QString& userPrompt,
                                                       QString* error = nullptr);
  static std::optional<QString> extractAssistantText(const QJsonObject& response,
                                                      QString* error = nullptr);
  bool request(const ModelChatConfig& config, const QString& systemPrompt,
               const QString& userPrompt);

 signals:
  void completed(bool success, QString text);

 private:
  QNetworkAccessManager network_;
};

}  // namespace edward::resources
