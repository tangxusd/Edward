#include "edward/resources/supabase_auth_client.hpp"

#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

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
  QUrlQuery redirectQuery; redirectQuery.addQueryItem(QStringLiteral("redirect_to"), QStringLiteral("https://auth-recovery.vercel.app/")); endpoint.setQuery(redirectQuery);
  QNetworkRequest request{endpoint}; request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json")); request.setRawHeader("apikey", config.anonKey.toUtf8());
  auto* reply = network_.post(request, QJsonDocument(QJsonObject{{"email", email.trimmed()}, {"redirect_to", "https://auth-recovery.vercel.app/"}, {"options", QJsonObject{{"redirectTo", "https://auth-recovery.vercel.app/"}}}}).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    emit completed(ok, ok ? QStringLiteral("重置密码邮件已发送，请检查邮箱") : (status == 429 ? QStringLiteral("邮件发送过于频繁，请稍后重试") : QStringLiteral("密码找回失败，请检查邮箱配置")));
    reply->deleteLater();
  });
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
                                             {QStringLiteral("registration_unavailable"), QStringLiteral("注册失败，请稍后重试")},
                                             {QStringLiteral("email_already_registered"), QStringLiteral("该邮箱已完成注册，请直接登录")},
                                             {QStringLiteral("email_confirmation_required"), QStringLiteral("当前未启用邮箱确认，注册未完成，请在 Supabase Auth 中启用邮箱确认")}};
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

bool SupabaseAuthClient::createNativePayment(const SupabaseAuthConfig& config, const AuthSession& session,
                                             double amount, const QString& goodsDesc,
                                             const QString& channel) {
  if (session.accessToken.isEmpty() || amount < 0.01) { emit paymentCompleted(false, {}, {}, QStringLiteral("支付参数无效")); return false; }
  QUrl endpoint(config.projectUrl); endpoint.setPath(QStringLiteral("/functions/v1/huifu-native-create"));
  QNetworkRequest request{endpoint}; request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setTransferTimeout(30000);
  request.setRawHeader("apikey", config.anonKey.toUtf8()); request.setRawHeader("Authorization", (QStringLiteral("Bearer ") + session.accessToken).toUtf8());
  const auto body = QJsonObject{{"amount", amount}, {"goodsDesc", goodsDesc}, {"channel", channel}, {"idempotencyKey", QUuid::createUuid().toString(QUuid::WithoutBraces)}};
  auto* reply = network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto object = QJsonDocument::fromJson(reply->readAll()).object();
    const bool ok = reply->error() == QNetworkReply::NoError && !object.value("qrCode").toString().isEmpty();
    const auto detailObject = object.value("detail").toObject();
    const auto upstreamStatus = object.value("upstreamStatus").toInt();
    const auto upstreamBody = object.value("upstreamBody").toString();
    const auto detail = detailObject.value("resp_desc").toString().isEmpty() ? object.value("detail").toString() : detailObject.value("resp_desc").toString();
    const auto code = detailObject.value("resp_code").toString();
    auto failure = detail.isEmpty() ? object.value("error").toString(QStringLiteral("支付下单失败")) : QStringLiteral("支付下单失败：%1%2").arg(detail, code.isEmpty() ? QString() : QStringLiteral("（%1）").arg(code));
    if (failure == QStringLiteral("payment_config_error")) failure = QStringLiteral("支付配置错误，请检查汇付密钥");
    if (failure == QStringLiteral("payment_provider_error")) failure = QStringLiteral("汇付服务暂不可用，请稍后重试");
    if (upstreamStatus > 0) failure = QStringLiteral("汇付接口 HTTP %1：%2").arg(upstreamStatus).arg(upstreamBody.isEmpty() ? failure : upstreamBody);
    emit paymentCompleted(ok, object.value("orderId").toString(), object.value("qrCode").toString(), ok ? QStringLiteral("请扫码完成支付") : failure);
    reply->deleteLater();
  });
  return true;
}

bool SupabaseAuthClient::createAlipayPayment(const SupabaseAuthConfig& config, const AuthSession& session, const QString& planKey) {
  if (session.accessToken.isEmpty() || planKey.isEmpty()) { emit paymentCompleted(false, {}, {}, QStringLiteral("支付参数无效")); return false; }
  QUrl endpoint(config.projectUrl); endpoint.setPath(QStringLiteral("/functions/v1/alipay-create-order"));
  QNetworkRequest request{endpoint}; request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setTransferTimeout(30000);
  request.setRawHeader("apikey", config.anonKey.toUtf8()); request.setRawHeader("Authorization", (QStringLiteral("Bearer ") + session.accessToken).toUtf8());
  const auto body = QJsonObject{{"planKey", planKey}, {"idempotencyKey", QUuid::createUuid().toString(QUuid::WithoutBraces)}};
  auto* reply = network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto object = QJsonDocument::fromJson(reply->readAll()).object(); const auto qr = object.value("qrCode").toString();
    const bool ok = reply->error() == QNetworkReply::NoError && !qr.isEmpty();
    const auto detail = object.value("providerMessage").toString();
    const auto providerCode = object.value("providerCode").toString();
    const auto httpStatus = object.value("upstreamStatus").toInt();
    QString failure = detail.isEmpty() ? object.value("error").toString(QStringLiteral("支付宝下单失败")) : QStringLiteral("支付宝下单失败：%1").arg(detail);
    if (!providerCode.isEmpty()) failure += QStringLiteral("（业务码 %1）").arg(providerCode);
    if (httpStatus > 0) failure += QStringLiteral(" [HTTP %1]").arg(httpStatus);
    if (reply->error() != QNetworkReply::NoError) failure += QStringLiteral(" [%1]").arg(reply->errorString());
    emit paymentCompleted(ok, object.value("orderId").toString(), qr, ok ? QStringLiteral("请使用支付宝扫描二维码") : failure);
    reply->deleteLater();
  });
  return true;
}

bool SupabaseAuthClient::queryAlipayOrder(const SupabaseAuthConfig& config, const AuthSession& session, const QString& orderId) {
  if (session.accessToken.isEmpty() || orderId.isEmpty()) return false;
  QUrl endpoint(config.projectUrl); endpoint.setPath(QStringLiteral("/functions/v1/alipay-query-order"));
  QNetworkRequest request{endpoint}; request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json")); request.setTransferTimeout(15000);
  request.setRawHeader("apikey", config.anonKey.toUtf8()); request.setRawHeader("Authorization", (QStringLiteral("Bearer ") + session.accessToken).toUtf8());
  auto* reply = network_.post(request, QJsonDocument(QJsonObject{{"orderId", orderId}}).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply, orderId] { const auto object = QJsonDocument::fromJson(reply->readAll()).object(); emit paymentStatus(orderId, object.value("status").toString()); reply->deleteLater(); });
  return true;
}

}  // namespace edward::resources
