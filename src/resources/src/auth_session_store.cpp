#include "edward/resources/auth_session_store.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QSettings>

namespace edward::resources {

namespace {
QSettings sessionSettings() {
  const auto settingsPath = qEnvironmentVariable("EDWARD_SETTINGS_PATH");
  if (!settingsPath.isEmpty()) return QSettings(settingsPath, QSettings::IniFormat);
  const auto settingsDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QDir().mkpath(settingsDirectory);
  return QSettings(QDir(settingsDirectory).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
}
}  // namespace

AuthSessionStore::AuthSessionStore(QObject* parent) : QObject(parent) {
  auto settings = sessionSettings();
  session_.userId = settings.value(QStringLiteral("auth/userId")).toString();
  session_.username = settings.value(QStringLiteral("auth/username")).toString();
  session_.accessToken = settings.value(QStringLiteral("auth/accessToken")).toString();
  session_.refreshToken = settings.value(QStringLiteral("auth/refreshToken")).toString();
}

bool AuthSessionStore::authenticated() const {
  return !session_.userId.isEmpty() && !session_.username.isEmpty() && !session_.accessToken.isEmpty();
}

QString AuthSessionStore::userId() const { return session_.userId; }

QString AuthSessionStore::username() const { return session_.username; }

void AuthSessionStore::setSession(AuthSession session) {
  session_ = std::move(session);
  auto settings = sessionSettings();
  settings.setValue(QStringLiteral("auth/userId"), session_.userId);
  settings.setValue(QStringLiteral("auth/username"), session_.username);
  settings.setValue(QStringLiteral("auth/accessToken"), session_.accessToken);
  settings.setValue(QStringLiteral("auth/refreshToken"), session_.refreshToken);
  emit changed();
}

void AuthSessionStore::clear() {
  session_ = {};
  auto settings = sessionSettings();
  settings.remove(QStringLiteral("auth"));
  emit changed();
}

}  // namespace edward::resources
