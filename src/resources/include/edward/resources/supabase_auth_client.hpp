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
  bool sendPasswordReset(const SupabaseAuthConfig& config, const QString& email);
  bool refreshSession(const SupabaseAuthConfig& config, AuthSessionStore* sessions);
  bool fetchEntitlement(const SupabaseAuthConfig& config, const AuthSession& session);
  bool createNativePayment(const SupabaseAuthConfig& config, const AuthSession& session,
                           double amount, const QString& goodsDesc,
                           const QString& channel = QStringLiteral("wechat"));
  bool createAlipayPayment(const SupabaseAuthConfig& config, const AuthSession& session, const QString& planKey);

 signals:
  void completed(bool success, QString message);
  void entitlementCompleted(bool success, QString status, QString expiresAt, qint64 credits);
  void paymentCompleted(bool success, QString orderId, QString qrCode, QString message);
  void sessionRefreshed(bool success, QString message);

 private:
  QNetworkAccessManager network_;
};

}  // namespace edward::resources
