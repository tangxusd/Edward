#pragma once

#include "edward/resources/auth_session_store.hpp"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

#include <optional>

namespace edward::resources {

struct SupabaseAuthConfig final {
  QString projectUrl;
  QString anonKey;
};

struct SupabaseSignInRequest final {
  QString endpoint;
  QString anonKey;
  QJsonObject body;
};

class SupabaseAuthClient final : public QObject {
  Q_OBJECT

 public:
  explicit SupabaseAuthClient(QObject* parent = nullptr) : QObject(parent) {}
  static std::optional<SupabaseSignInRequest> buildPasswordSignInRequest(
      const SupabaseAuthConfig& config, const QString& email, const QString& password,
      QString* error = nullptr);
  bool signInWithPassword(const SupabaseAuthConfig& config, const QString& email,
                          const QString& password, AuthSessionStore* sessions);
  bool signUpWithPassword(const SupabaseAuthConfig& config, const QString& email,
                          const QString& password);

 signals:
  void completed(bool success, QString message);

 private:
  QNetworkAccessManager network_;
};

}  // namespace edward::resources
