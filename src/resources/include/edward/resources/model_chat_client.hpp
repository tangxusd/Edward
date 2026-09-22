#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

namespace edward::resources {

struct ModelChatConfig final {
  QString endpoint;
  QString apiKey;
  QString model;
  QString protocol = QStringLiteral("openai-completions");
};

struct ModelChatRequest final {
  QString endpoint;
  QString apiKey;
  QJsonObject body;
  QHash<QByteArray, QByteArray> headers;
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
                                                      const QString& protocol = QStringLiteral("openai-completions"),
                                                      QString* error = nullptr);
  static QString modelsEndpoint(const QString& chatEndpoint);
  static QStringList extractModelIds(const QJsonObject& response, QString* error = nullptr);
  bool request(const ModelChatConfig& config, const QString& systemPrompt,
               const QString& userPrompt);
  bool requestModels(const ModelChatConfig& config);

 signals:
  void completed(bool success, QString text);
  void modelsCompleted(bool success, QStringList modelIds, QString message);

 private:
  QNetworkAccessManager network_;
};

}  // namespace edward::resources
