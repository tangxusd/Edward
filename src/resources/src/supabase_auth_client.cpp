#include "edward/resources/supabase_auth_client.hpp"

#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>

namespace edward::resources {

std::optional<SupabaseSignInRequest> SupabaseAuthClient::buildPasswordSignInRequest(
    const SupabaseAuthConfig& config, const QString& email, const QString& password, QString* error) {
  const auto fail = [error](const QString& message) -> std::optional<SupabaseSignInRequest> {
    if (error) *error = message;
    return std::nullopt;
  };
  const QUrl projectUrl(config.projectUrl);
  if (!projectUrl.isValid() || projectUrl.scheme() != QStringLiteral("https") ||
      projectUrl.host().isEmpty() || (!projectUrl.path().isEmpty() && projectUrl.path() != QStringLiteral("/")))
    return fail(QStringLiteral("Supabase project URL must be an HTTPS origin"));
  if (config.anonKey.isEmpty()) return fail(QStringLiteral("Supabase anon key is required"));
  if (email.isEmpty() || password.isEmpty()) return fail(QStringLiteral("identifier and password are required"));
  QUrl endpoint(projectUrl);
  endpoint.setPath(QStringLiteral("/functions/v1/auth-login"));
  return SupabaseSignInRequest{endpoint.toString(), config.anonKey,
                               QJsonObject{{"identifier", email}, {"password", password}}};
}

bool SupabaseAuthClient::signInWithPassword(const SupabaseAuthConfig& config, const QString& email,
                                            const QString& password, AuthSessionStore* sessions) {
  if (!sessions) {
    emit completed(false, QStringLiteral("登录会话不可用"));
    return false;
  }
  QString error;
  const auto requestData = buildPasswordSignInRequest(config, email, password, &error);
  if (!requestData) {
    emit completed(false, error);
    return false;
  }
  QNetworkRequest request{QUrl(requestData->endpoint)};
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setRawHeader("apikey", requestData->anonKey.toUtf8());
  auto* reply = network_.post(request, QJsonDocument(requestData->body).toJson(QJsonDocument::Compact));
  QPointer<AuthSessionStore> sessionStore(sessions);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sessionStore] {
    const auto body = reply->readAll();
    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(body, &parseError);
    const auto object = response.isObject() ? response.object() : QJsonObject{};
    const auto userId = object.value("user").toObject().value("id").toString();
    const auto username = object.value("user").toObject().value("email").toString();
    const auto accessToken = object.value("access_token").toString();
    const auto refreshToken = object.value("refresh_token").toString();
    const bool success = reply->error() == QNetworkReply::NoError && !userId.isEmpty() &&
                         !username.isEmpty() && !accessToken.isEmpty();
    if (success && sessionStore) sessionStore->setSession({userId, username, accessToken, refreshToken});
    const auto code = object.value(QStringLiteral("error")).toString();
    const auto message = success ? QStringLiteral("登录成功")
                                 : (!code.isEmpty() ? (code == QStringLiteral("invalid_credentials") ? QStringLiteral("邮箱/账号名或密码错误") : QStringLiteral("登录失败：%1").arg(code))
                                                    : (reply->error() == QNetworkReply::NoError ? QStringLiteral("登录响应缺少会话字段") : reply->errorString()));
    emit completed(success && !sessionStore.isNull(),
                   sessionStore ? message : QStringLiteral("登录会话已关闭"));
    reply->deleteLater();
  });
  return true;
}

bool SupabaseAuthClient::sendPasswordReset(const SupabaseAuthConfig& config, const QString& email) {
  if (email.trimmed().isEmpty() || !email.contains(QLatin1Char('@'))) { emit completed(false, QStringLiteral("请输入注册邮箱")); return false; }
  QUrl endpoint(config.projectUrl); endpoint.setPath(QStringLiteral("/auth/v1/recover"));
  QNetworkRequest request{endpoint}; request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json")); request.setRawHeader("apikey", config.anonKey.toUtf8());
  auto* reply = network_.post(request, QJsonDocument(QJsonObject{{"email", email.trimmed()}}).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] { const bool ok = reply->error() == QNetworkReply::NoError; emit completed(ok, ok ? QStringLiteral("重置密码邮件已发送，请检查邮箱") : QStringLiteral("密码找回失败，请稍后重试")); reply->deleteLater(); });
  return true;
}

bool SupabaseAuthClient::signUpWithPassword(const SupabaseAuthConfig& config, const QString& email,
                                            const QString& password) {
  if (email.isEmpty() || password.isEmpty()) { emit completed(false, QStringLiteral("邮箱和密码不能为空")); return false; }
  QUrl endpoint(config.projectUrl); endpoint.setPath(QStringLiteral("/functions/v1/auth-register"));
  QNetworkRequest request{endpoint}; request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json")); request.setRawHeader("apikey", config.anonKey.toUtf8());
  auto* reply = network_.post(request, QJsonDocument(QJsonObject{{"email", email}, {"password", password}, {"username", email.section('@', 0, 0)}}).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto raw = reply->readAll();
    const auto object = QJsonDocument::fromJson(raw).object();
    const auto code = object.value(QStringLiteral("error")).toString();
    const bool ok = reply->error() == QNetworkReply::NoError && code.isEmpty();
    QString message = ok ? QStringLiteral("注册请求已提交，请检查邮箱") : reply->errorString();
    if (!code.isEmpty()) {
      const QHash<QString, QString> messages{{QStringLiteral("invalid_registration"), QStringLiteral("请使用有效邮箱，密码至少 8 位，用户名 3～32 个字符")},
                                             {QStringLiteral("rate_limited"), QStringLiteral("注册请求过于频繁，请稍后再试")},
                                             {QStringLiteral("registration_unavailable"), QStringLiteral("注册失败，该邮箱或账号名可能已存在")}};
      message = messages.value(code, QStringLiteral("注册失败：%1").arg(code));
    }
    emit completed(ok, message);
    reply->deleteLater();
  });
  return true;
}

bool SupabaseAuthClient::refreshSession(const SupabaseAuthConfig& config, AuthSessionStore* sessions) {
  if (!sessions || sessions->session().refreshToken.isEmpty()) {
    emit sessionRefreshed(false, QStringLiteral("登录会话已过期"));
    return false;
  }
  QUrl endpoint(config.projectUrl);
  endpoint.setPath(QStringLiteral("/auth/v1/token"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
  endpoint.setQuery(query);
  QNetworkRequest request{endpoint};
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setRawHeader("apikey", config.anonKey.toUtf8());
  const auto previous = sessions->session();
  auto* reply = network_.post(request, QJsonDocument(QJsonObject{{"refresh_token", previous.refreshToken}})
                                         .toJson(QJsonDocument::Compact));
  QPointer<AuthSessionStore> sessionStore(sessions);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sessionStore, previous] {
    const auto object = QJsonDocument::fromJson(reply->readAll()).object();
    const auto accessToken = object.value(QStringLiteral("access_token")).toString();
    const auto refreshToken = object.value(QStringLiteral("refresh_token")).toString();
    const bool success = reply->error() == QNetworkReply::NoError && !accessToken.isEmpty() &&
                         !sessionStore.isNull();
    if (success) {
      sessionStore->setSession({previous.userId, previous.username, accessToken,
                                refreshToken.isEmpty() ? previous.refreshToken : refreshToken});
    }
    emit sessionRefreshed(success, success ? QStringLiteral("登录会话已续期")
                                           : QStringLiteral("登录会话已过期，请重新登录"));
    reply->deleteLater();
  });
  return true;
}

bool SupabaseAuthClient::fetchEntitlement(const SupabaseAuthConfig& config, const AuthSession& session) {
  if (session.accessToken.isEmpty()) return false;
  QUrl endpoint(config.projectUrl); endpoint.setPath(QStringLiteral("/functions/v1/auth-entitlement"));
  QNetworkRequest request{endpoint}; request.setRawHeader("apikey", config.anonKey.toUtf8()); request.setRawHeader("Authorization", (QStringLiteral("Bearer ") + session.accessToken).toUtf8());
  auto* reply = network_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto object = QJsonDocument::fromJson(reply->readAll()).object();
    const auto trial = object.value("trial").toObject();
    const auto subscriptions = object.value("subscriptions").toArray();
    QString status = trial.value("status").toString(); QString expires = trial.value("ends_at").toString();
    if (!subscriptions.isEmpty()) { const auto current = subscriptions.first().toObject(); status = current.value("status").toString(); expires = current.value("current_period_end").toString(); }
    qint64 credits = 0; for (const auto& value : object.value("credits").toArray()) credits += value.toObject().value("amount").toVariant().toLongLong();
    emit entitlementCompleted(reply->error() == QNetworkReply::NoError, status, expires, credits); reply->deleteLater();
  });
  return true;
}

}  // namespace edward::resources
