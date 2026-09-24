#include "edward/desktop/workbench_runtime.hpp"

#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QVariantMap>

namespace edward::desktop {

namespace {
const QStringList kFablecutPathKeys{
    QStringLiteral("projectRoot"), QStringLiteral("cacheRoot"), QStringLiteral("exportRoot"),
    QStringLiteral("componentDownloadRoot"), QStringLiteral("mediaDownloadRoot"),
    QStringLiteral("pluginDownloadRoot"), QStringLiteral("pluginRuntimeRoot"),
    QStringLiteral("proxyRoot"), QStringLiteral("prerenderRoot")};

const QStringList kDerivedFablecutPathKeys{
    QStringLiteral("cacheRoot"), QStringLiteral("componentDownloadRoot"),
    QStringLiteral("mediaDownloadRoot"), QStringLiteral("proxyRoot"), QStringLiteral("prerenderRoot")};

QSettings desktopSettings() {
  const auto settingsPath = qEnvironmentVariable("EDWARD_SETTINGS_PATH");
  if (!settingsPath.isEmpty()) return QSettings(settingsPath, QSettings::IniFormat);
  const auto path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QDir().mkpath(path);
  return QSettings(QDir(path).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
}

QString defaultFablecutPath(const QString& key) {
  const auto dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (key == QStringLiteral("projectRoot")) return QString::fromUtf8(EDWARD_SOURCE_DIR) + QStringLiteral("/third_party/FableCut");
  static const QHash<QString, QString> suffixes{
      {QStringLiteral("cacheRoot"), QStringLiteral("cache")},
      {QStringLiteral("exportRoot"), QStringLiteral("exports")},
      {QStringLiteral("componentDownloadRoot"), QStringLiteral("components")},
      {QStringLiteral("mediaDownloadRoot"), QStringLiteral("media")},
      {QStringLiteral("pluginDownloadRoot"), QStringLiteral("plugins/downloads")},
      {QStringLiteral("pluginRuntimeRoot"), QStringLiteral("plugins/runtime")},
      {QStringLiteral("proxyRoot"), QStringLiteral("proxies")},
      {QStringLiteral("prerenderRoot"), QStringLiteral("prerenders")}};
  return QDir(dataRoot).filePath(suffixes.value(key));
}

QVariantMap fablecutPaths(QSettings& settings) {
  QVariantMap paths;
  for (const auto& key : kFablecutPathKeys)
    paths.insert(key, QDir(settings.value(QStringLiteral("paths/") + key, defaultFablecutPath(key)).toString()).absolutePath());
  return paths;
}

bool copyFileVerified(const QString& source, const QString& destination) {
  const QFileInfo sourceInfo(source);
  if (!sourceInfo.exists() || !sourceInfo.isFile()) return false;
  QDir().mkpath(QFileInfo(destination).absolutePath());
  if (QFileInfo::exists(destination) && !QFile::remove(destination)) return false;
  return QFile::copy(source, destination) && QFileInfo(destination).size() == sourceInfo.size();
}

bool copyTreeVerified(const QString& source, const QString& destination) {
  const QDir sourceDir(source);
  if (!sourceDir.exists()) return true;
  QDir().mkpath(destination);
  QDirIterator it(source, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const auto sourceFile = it.next();
    const auto relative = sourceDir.relativeFilePath(sourceFile);
    if (!copyFileVerified(sourceFile, QDir(destination).filePath(relative))) return false;
  }
  return true;
}

bool clearDirectoryContents(const QString& path) {
  QDir directory(path);
  if (!directory.exists()) return true;
  for (const auto& entry : directory.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
    if (entry.isDir()) {
      if (!QDir(entry.absoluteFilePath()).removeRecursively()) return false;
    } else if (!QFile::remove(entry.absoluteFilePath())) {
      return false;
    }
  }
  return true;
}

bool isPreferencePackagePath(const QString& path) {
  return QFileInfo(path).suffix().compare(QStringLiteral("edwardprefs"), Qt::CaseInsensitive) == 0;
}

QString diagnosticLogPath() {
  const auto overriddenPath = qEnvironmentVariable("EDWARD_DIAGNOSTIC_LOG_PATH").trimmed();
  if (!overriddenPath.isEmpty()) return overriddenPath;
  const auto dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  return QDir(dataRoot).filePath(QStringLiteral("logs/edward.log"));
}

QString sanitizeDiagnosticText(QString text) {
  static const QRegularExpression bearerPattern(
      QStringLiteral("(bearer\\s+)[^\\s\\\"']+"), QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression secretPattern(
      QStringLiteral("((?:api[_ -]?key|access[_ -]?token|authorization|password)\\s*[:=]\\s*)[^\\s,;]+"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression unixPathPattern(QStringLiteral("/Users/[^\\s\\\"']+"));
  static const QRegularExpression windowsPathPattern(QStringLiteral("[A-Za-z]:\\\\[^\\s\\\"']+"));
  text.replace(bearerPattern, QStringLiteral("credential=<redacted>"));
  text.replace(secretPattern, QStringLiteral("credential=<redacted>"));
  text.replace(unixPathPattern, QStringLiteral("<local-path>"));
  text.replace(windowsPathPattern, QStringLiteral("<local-path>"));
  return text;
}

void appendDiagnosticLog(const QString& event) {
  const auto path = diagnosticLogPath();
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (file.exists() && file.size() > 256 * 1024 && file.open(QIODevice::ReadOnly)) {
    const auto retained = file.readAll().right(128 * 1024);
    file.close();
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      file.write(retained);
      file.close();
    }
  }
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) return;
  const auto line = QStringLiteral("%1 %2\n")
                        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                             sanitizeDiagnosticText(event).left(512));
  file.write(line.toUtf8());
}
}  // namespace

WorkbenchRuntime::WorkbenchRuntime(QObject* parent) : QObject(parent), preferenceStore_(this), modelChatClient_(this) {
  appendDiagnosticLog(QStringLiteral("desktop_runtime_initialized"));
  connect(&authClient_, &edward::resources::SupabaseAuthClient::completed, this,
          [this](bool success, const QString& message) {
            const auto operation = pendingAuthenticationOperation_;
            pendingAuthenticationOperation_.clear();
            emit settingsOperationCompleted(operation, success, message);
          });
  connect(&modelChatClient_, &edward::resources::ModelChatClient::completed, this,
          [this](bool success, const QString& message) {
            if (aiChatRequestActive_) {
              aiChatRequestActive_ = false;
              aiRequestBusy_ = false;
              if (!success) {
                aiConversation_.append(QStringLiteral("AI：请求失败：%1\n").arg(message));
              } else {
                const auto result = aiOrchestrator_.handle(message, pendingAiProject_);
                const auto marker = aiConversation_.lastIndexOf(QStringLiteral("AI："));
                if (result.kind == edward::ai::AiResult::Kind::ActionPlan) {
                  QJsonObject normalizedPlan{
                      {QStringLiteral("schemaVersion"), result.plan->schemaVersion},
                      {QStringLiteral("requestId"), result.plan->requestId},
                      {QStringLiteral("baseProjectRevision"), static_cast<double>(result.plan->baseProjectRevision)},
                      {QStringLiteral("operations"), result.plan->operations}};
                  pendingAiActionPlan_ = QString::fromUtf8(QJsonDocument(normalizedPlan).toJson(QJsonDocument::Compact));
                  if (marker >= 0) aiConversation_.replace(marker + 3, aiConversation_.size() - marker - 3, QStringLiteral("已生成剪辑操作方案，请确认后应用。\n"));
                  else aiConversation_.append(QStringLiteral("AI：已生成剪辑操作方案，请确认后应用。\n"));
                } else {
                  pendingAiActionPlan_.clear();
                  const auto finalText = result.text.trimmed().isEmpty() ? message.trimmed() : result.text.trimmed();
                  if (marker >= 0) aiConversation_.replace(marker + 3, aiConversation_.size() - marker - 3, finalText + QStringLiteral("\n"));
                  else aiConversation_.append(QStringLiteral("AI：%1\n").arg(finalText));
                }
              }
              appendDiagnosticLog(QStringLiteral("desktop_ai_assistant success=%1 message=%2")
                                      .arg(success ? QStringLiteral("true") : QStringLiteral("false"), message));
              emit timelineChanged();
              return;
            }
            appendDiagnosticLog(QStringLiteral("desktop_model_connection success=%1 message=%2")
                                    .arg(success ? QStringLiteral("true") : QStringLiteral("false"), message));
            emit settingsOperationCompleted(QStringLiteral("testProvider"), success,
                                            success ? QStringLiteral("模型连接成功") : message);
  });
  connect(&modelChatClient_, &edward::resources::ModelChatClient::chunk, this,
          [this](const QString& text) {
            if (!aiChatRequestActive_) return;
            aiStreamingText_.append(text);
            const auto marker = aiConversation_.lastIndexOf(QStringLiteral("AI："));
            if (marker >= 0) aiConversation_.replace(marker + 3, aiConversation_.size() - marker - 3, aiStreamingText_);
            emit timelineChanged();
          });
  connect(&modelChatClient_, &edward::resources::ModelChatClient::modelsCompleted, this,
          [this](bool success, const QStringList& models, const QString& message) {
            if (success && !models.isEmpty()) {
              auto settings = desktopSettings();
              settings.setValue(QStringLiteral("ai/modelCatalog"), models);
              settings.sync();
            }
            appendDiagnosticLog(QStringLiteral("desktop_model_catalog success=%1 models=%2 message=%3")
                                    .arg(success ? QStringLiteral("true") : QStringLiteral("false"))
                                    .arg(models.size())
                                    .arg(message));
            emit settingsOperationCompleted(QStringLiteral("fetchModels"), success, message, models);
          });
}

QJsonObject WorkbenchRuntime::nativeRuntimeProps() const { return component_ ? component_->props : QJsonObject{}; }

QString WorkbenchRuntime::nativeRuntimePreviewEntry() const {
  if (!component_ || !manifest_) return {};
  return QUrl::fromLocalFile(QDir(component_->packageRoot).filePath(manifest_->previewEntry)).toString();
}

QJsonObject WorkbenchRuntime::nativeRuntimeHostMessage() const {
  auto message = host_.message(QStringLiteral("setFrame"));
  message.insert(QStringLiteral("protocol"), QStringLiteral("edward.web-runtime.host-message.v1"));
  return message;
}

QVariantList WorkbenchRuntime::clips() const {
  if (!component_ || clipId_ == 0) return {};
  return {QVariantMap{{QStringLiteral("id"), clipId_}, {QStringLiteral("runtime"), component_->runtime}}};
}

bool WorkbenchRuntime::mount(const QString& packageRoot, const QJsonObject& props) {
  QFile file(QDir(packageRoot).filePath(QStringLiteral("edward-runtime.json")));
  if (!file.open(QIODevice::ReadOnly)) return false;
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
  QString error;
  auto manifest = edward::runtime::RuntimeManifest::parse(document.object(), &error);
  if (!manifest) return false;
  manifest_ = std::move(manifest);
  if (const auto result = host_.mount(*manifest_, packageRoot, props); !result.ok) {
    manifest_.reset();
    return false;
  }
  component_ = edward::core::NativeRuntimeComponent{QFileInfo(packageRoot).canonicalFilePath(),
      QStringLiteral("edward-runtime.json"), manifest_->runtime, props};
  clipId_ = 1;
  emit runtimeChanged();
  return true;
}

bool WorkbenchRuntime::addNativeRuntimePackage(const QString& packageRoot, const QJsonObject& props) {
  return mount(packageRoot, props);
}

bool WorkbenchRuntime::setNativeRuntimeProps(const QJsonObject& props) {
  if (!component_ || !host_.setProps(props).ok) return false;
  component_->props = props;
  emit runtimeChanged();
  return true;
}

bool WorkbenchRuntime::selectClip(qlonglong id) { return component_ && id == clipId_; }

bool WorkbenchRuntime::saveProject(const QString& path) const {
  if (!component_ || path.isEmpty()) return false;
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  const QJsonObject native{{QStringLiteral("packageRoot"), component_->packageRoot},
      {QStringLiteral("manifestPath"), component_->manifestPath}, {QStringLiteral("runtime"), component_->runtime},
      {QStringLiteral("props"), component_->props}};
  file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), QStringLiteral("0.7.0")},
      {QStringLiteral("clips"), QJsonArray{QJsonObject{{QStringLiteral("id"), clipId_}, {QStringLiteral("nativeRuntime"), native}}}}}).toJson(QJsonDocument::Indented));
  return file.commit();
}

bool WorkbenchRuntime::loadProject(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return false;
  const auto document = QJsonDocument::fromJson(file.readAll());
  const auto clips = document.object().value(QStringLiteral("clips")).toArray();
  if (clips.size() != 1) return false;
  const auto native = clips.first().toObject().value(QStringLiteral("nativeRuntime")).toObject();
  return mount(native.value(QStringLiteral("packageRoot")).toString(), native.value(QStringLiteral("props")).toObject());
}

bool WorkbenchRuntime::flushPreferencesForProjectClose() { return preferenceStore_.flushPendingPreferences(); }

bool WorkbenchRuntime::flushPreferencesForExport() { return preferenceStore_.flushPendingPreferences(); }

bool WorkbenchRuntime::compilePreferencesNow() { return preferenceStore_.compilePreferences(); }

QVariantMap WorkbenchRuntime::preferenceStoreStatus() const { return preferenceStore_.status(); }

QVariantMap WorkbenchRuntime::fablecutSettings() const {
  auto settings = desktopSettings();
  const auto preferenceStatus = preferenceStore_.status();
  const auto providerId = settings.value(QStringLiteral("ai/providerId"), QStringLiteral("deepseek")).toString();
  auto endpoint = settings.value(QStringLiteral("ai/endpoint"), QStringLiteral("https://api.deepseek.com/chat/completions")).toString();
  if (providerId == QStringLiteral("deepseek") && endpoint == QStringLiteral("https://api.deepseek.com/v1")) {
    endpoint = QStringLiteral("https://api.deepseek.com/chat/completions");
    settings.setValue(QStringLiteral("ai/endpoint"), endpoint);
    settings.sync();
  }
  return {{QStringLiteral("provider"), settings.value(QStringLiteral("ai/provider"), QStringLiteral("DeepSeek")).toString()},
          {QStringLiteral("providerId"), providerId},
          {QStringLiteral("protocol"), settings.value(QStringLiteral("ai/protocol"), QStringLiteral("openai-completions")).toString()},
          {QStringLiteral("endpoint"), endpoint},
          {QStringLiteral("model"), settings.value(QStringLiteral("ai/model"), QStringLiteral("deepseek-chat")).toString()},
          {QStringLiteral("models"), settings.value(QStringLiteral("ai/modelCatalog")).toStringList()},
          {QStringLiteral("exportDirectory"), settings.value(QStringLiteral("paths/exportRoot"), defaultFablecutPath(QStringLiteral("exportRoot"))).toString()},
          {QStringLiteral("paths"), fablecutPaths(settings)},
          {QStringLiteral("preferenceStorage"), QStringLiteral("本地偏好存储已启用")},
          {QStringLiteral("preferencePending"), preferenceStatus.value(QStringLiteral("pending"))}};
}

QVariantMap WorkbenchRuntime::fablecutAuthSession() const {
  const auto settings = desktopSettings();
  const auto access = settings.value(QStringLiteral("auth/accessToken")).toString();
  const auto refresh = settings.value(QStringLiteral("auth/refreshToken")).toString();
  if (access.isEmpty() || refresh.isEmpty()) return {};
  return {{QStringLiteral("access_token"), access},
          {QStringLiteral("refresh_token"), refresh},
          {QStringLiteral("expires_in"), settings.value(QStringLiteral("auth/expiresIn")).toInt()},
          {QStringLiteral("user"), settings.value(QStringLiteral("auth/userJson")).toString()}};
}

void WorkbenchRuntime::setSupabaseAuthConfig(const edward::resources::SupabaseAuthConfig& config) { authConfig_ = config; }

bool WorkbenchRuntime::submitFablecutRegistration(const QString& email, const QString& password, const QString& username) {
  const auto device = edward::resources::collectDeviceIdentity();
  if (!device.valid()) { emit settingsOperationCompleted(QStringLiteral("authRegistration"), false, device.error); return false; }
  pendingAuthenticationOperation_ = QStringLiteral("authRegistration");
  return authClient_.signUpWithDevice(authConfig_, email, password, username, device);
}

bool WorkbenchRuntime::submitFablecutPasswordRecovery(const QString& email) {
  const auto device = edward::resources::collectDeviceIdentity();
  if (!device.valid()) { emit settingsOperationCompleted(QStringLiteral("authRecovery"), false, device.error); return false; }
  pendingAuthenticationOperation_ = QStringLiteral("authRecovery");
  return authClient_.beginPasswordRecovery(authConfig_, email, device);
}

bool WorkbenchRuntime::submitFablecutDeviceEnrollment(const QString& accessToken) {
  const auto device = edward::resources::collectDeviceIdentity();
  if (!device.valid()) { emit settingsOperationCompleted(QStringLiteral("authDeviceEnrollment"), false, device.error); return false; }
  pendingAuthenticationOperation_ = QStringLiteral("authDeviceEnrollment");
  return authClient_.enrollDevice(authConfig_, accessToken, device);
}

void WorkbenchRuntime::setFablecutAuthState(bool authenticated) {
  emit fablecutAuthStateChanged(authenticated);
}

bool WorkbenchRuntime::saveFablecutAuthSession(const QVariantMap& session) {
  const auto access = session.value(QStringLiteral("access_token")).toString().trimmed();
  const auto refresh = session.value(QStringLiteral("refresh_token")).toString().trimmed();
  if (access.isEmpty() || refresh.isEmpty()) return false;
  auto settings = desktopSettings();
  settings.setValue(QStringLiteral("auth/accessToken"), access);
  settings.setValue(QStringLiteral("auth/refreshToken"), refresh);
  settings.setValue(QStringLiteral("auth/expiresIn"), session.value(QStringLiteral("expires_in")).toInt());
  const auto user = session.value(QStringLiteral("user"));
  const auto userObject = user.typeId() == QMetaType::QString ? QJsonDocument::fromJson(user.toString().toUtf8()).object() : QJsonObject::fromVariantMap(user.toMap());
  preferenceStore_.setAccountScope(userObject.value(QStringLiteral("id")).toString());
  settings.setValue(QStringLiteral("auth/userJson"), user.typeId() == QMetaType::QString ? user.toString() : QString::fromUtf8(QJsonDocument::fromVariant(user).toJson(QJsonDocument::Compact)));
  settings.sync();
  return settings.status() == QSettings::NoError;
}

bool WorkbenchRuntime::clearFablecutAuthSession() {
  auto settings = desktopSettings();
  settings.remove(QStringLiteral("auth"));
  settings.sync();
  preferenceStore_.setAccountScope(QStringLiteral("anonymous"));
  return settings.status() == QSettings::NoError;
}

bool WorkbenchRuntime::saveFablecutSettings(const QString& providerId, const QString& provider,
                                            const QString& endpoint, const QString& apiKey, const QString& model,
                                            const QString& protocol,
                                            const QString& exportDirectory) {
  static const QRegularExpression providerIdPattern(QStringLiteral("^[a-z][a-z0-9-]{1,63}$"));
  const auto normalizedProviderId = providerId.trimmed();
  const auto normalizedProtocol = protocol.trimmed();
  const QStringList supportedProtocols{QStringLiteral("openai-completions"),
                                       QStringLiteral("openai-responses"),
                                       QStringLiteral("anthropic-messages")};
  if (!providerIdPattern.match(normalizedProviderId).hasMatch() ||
      !supportedProtocols.contains(normalizedProtocol)) return false;
  const auto normalizedEndpoint = endpoint.trimmed();
  const auto normalizedModel = model.trimmed();
  if ((!normalizedEndpoint.isEmpty() || !normalizedModel.isEmpty()) &&
      (normalizedEndpoint.isEmpty() || normalizedModel.isEmpty())) return false;
  if (!normalizedEndpoint.isEmpty()) {
    const QUrl url(normalizedEndpoint);
    if (!url.isValid() || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0 || url.host().isEmpty())
      return false;
  }
  const auto normalizedDirectory = exportDirectory.trimmed();
  if (!normalizedDirectory.isEmpty() && !QDir(normalizedDirectory).exists()) return false;

  auto settings = desktopSettings();
  settings.setValue(QStringLiteral("ai/providerId"), normalizedProviderId);
  settings.setValue(QStringLiteral("ai/provider"), provider.trimmed());
  settings.setValue(QStringLiteral("ai/protocol"), normalizedProtocol);
  settings.setValue(QStringLiteral("ai/endpoint"), normalizedEndpoint);
  settings.setValue(QStringLiteral("ai/model"), normalizedModel);
  if (!apiKey.trimmed().isEmpty()) settings.setValue(QStringLiteral("ai/apiKey"), apiKey.trimmed());
  if (!normalizedDirectory.isEmpty()) settings.setValue(QStringLiteral("paths/exportRoot"), QDir(normalizedDirectory).absolutePath());
  settings.sync();
  return settings.status() == QSettings::NoError;
}

QString WorkbenchRuntime::chooseFablecutExportDirectory(const QString& currentDirectory) {
  const auto selected = QFileDialog::getExistingDirectory(nullptr, QStringLiteral("选择导出目录"), currentDirectory);
  return selected.isEmpty() ? QString{} : QDir(selected).absolutePath();
}

bool WorkbenchRuntime::saveFablecutPathSettings(const QVariantMap& paths) {
  if (paths.size() != kFablecutPathKeys.size()) return false;
  auto settings = desktopSettings();
  for (const auto& key : kFablecutPathKeys) {
    const auto raw = paths.value(key).toString().trimmed();
    if (raw.isEmpty() || !QDir::isAbsolutePath(raw)) return false;
    if (!QDir().mkpath(raw)) return false;
    settings.setValue(QStringLiteral("paths/") + key, QDir(raw).absolutePath());
  }
  settings.sync();
  return settings.status() == QSettings::NoError;
}

bool WorkbenchRuntime::migrateFablecutPath(const QString& key, const QString& destination) {
  if (!kFablecutPathKeys.contains(key) || destination.trimmed().isEmpty() || !QDir::isAbsolutePath(destination)) return false;
  auto settings = desktopSettings();
  const auto source = fablecutPaths(settings).value(key).toString();
  const auto target = QDir(destination).absolutePath();
  const auto cleanSource = QDir::cleanPath(source);
  const auto cleanTarget = QDir::cleanPath(target);
  if (cleanSource == cleanTarget) return true;
  const auto separator = QDir::separator();
  if (cleanTarget.startsWith(cleanSource + separator) || cleanSource.startsWith(cleanTarget + separator)) return false;
  if (!QDir().mkpath(target)) return false;
  bool copied = false;
  if (key == QStringLiteral("projectRoot")) {
    const auto sourceProject = QDir(source).filePath(QStringLiteral("project.json"));
    const auto targetProject = QDir(target).filePath(QStringLiteral("project.json"));
    copied = !QFileInfo::exists(sourceProject) || copyFileVerified(sourceProject, targetProject);
    if (copied && QFileInfo::exists(sourceProject) && !QFile::remove(sourceProject)) return false;
  } else {
    copied = copyTreeVerified(source, target);
    if (copied && QDir(source).exists() && !QDir(source).removeRecursively()) return false;
  }
  if (!copied) return false;
  settings.setValue(QStringLiteral("paths/") + key, target);
  settings.sync();
  return settings.status() == QSettings::NoError;
}

bool WorkbenchRuntime::clearDerivedFablecutPath(const QString& key) {
  if (!kDerivedFablecutPathKeys.contains(key)) return false;
  auto settings = desktopSettings();
  return clearDirectoryContents(fablecutPaths(settings).value(key).toString());
}

bool WorkbenchRuntime::exportFablecutPreferences(const QString& path) {
  if (path.trimmed().isEmpty() || !isPreferencePackagePath(path)) return false;
  const auto facts = preferenceStore_.exportFacts();
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  const QJsonObject document{{QStringLiteral("format"), QStringLiteral("edward-preferences-v1")},
                             {QStringLiteral("facts"), QJsonArray::fromVariantList(facts)}};
  file.write(QJsonDocument(document).toJson(QJsonDocument::Indented));
  return file.commit();
}

bool WorkbenchRuntime::importFablecutPreferences(const QString& path) {
  if (!isPreferencePackagePath(path)) return false;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return false;
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject() ||
      document.object().value(QStringLiteral("format")).toString() != QStringLiteral("edward-preferences-v1")) return false;
  return preferenceStore_.importFacts(document.object().value(QStringLiteral("facts")).toArray().toVariantList());
}

QString WorkbenchRuntime::chooseFablecutPreferencesExportPath() {
  return QFileDialog::getSaveFileName(nullptr, QStringLiteral("导出偏好包"),
                                      QStringLiteral("edward-preferences.edwardprefs"),
                                      QStringLiteral("Edward Preferences (*.edwardprefs)"));
}

QString WorkbenchRuntime::chooseFablecutPreferencesImportPath() {
  return QFileDialog::getOpenFileName(nullptr, QStringLiteral("导入偏好包"), {},
                                      QStringLiteral("Edward Preferences (*.edwardprefs)"));
}

QVariantList WorkbenchRuntime::fablecutPreferenceFacts() { return preferenceStore_.exportFacts(); }

bool WorkbenchRuntime::importFablecutPreferenceFacts(const QVariantList& facts) {
  return preferenceStore_.importFacts(facts);
}

QVariantMap WorkbenchRuntime::fablecutDiagnostics() {
  appendDiagnosticLog(QStringLiteral("diagnostic_snapshot_requested coordinate_contract=canvas-center:x-right-positive:y-up-positive"));
  QFile file(diagnosticLogPath());
  QByteArray content;
  if (file.open(QIODevice::ReadOnly)) content = file.readAll().right(24 * 1024);
  return {{QStringLiteral("appVersion"), QStringLiteral("0.7.0")},
          {QStringLiteral("platform"), QSysInfo::prettyProductName()},
          {QStringLiteral("log"), sanitizeDiagnosticText(QString::fromUtf8(content))}};
}

bool WorkbenchRuntime::recordFablecutDiagnostic(const QString& event) {
  const auto normalized = event.trimmed();
  if (normalized.isEmpty() || normalized.size() > 512) return false;
  appendDiagnosticLog(QStringLiteral("fablecut %1").arg(normalized));
  return true;
}

bool WorkbenchRuntime::testFablecutAiProvider(const QString& endpoint, const QString& apiKey,
                                              const QString& model, const QString& protocol) {
  const auto effectiveApiKey = apiKey.trimmed().isEmpty()
      ? desktopSettings().value(QStringLiteral("ai/apiKey")).toString() : apiKey.trimmed();
  return modelChatClient_.request({endpoint.trimmed(), effectiveApiKey, model.trimmed(), protocol.trimmed()},
                                  QStringLiteral("You are Edward's connection check."),
                                  QStringLiteral("Reply with OK."));
}

bool WorkbenchRuntime::fetchFablecutAiModels(const QString& endpoint, const QString& apiKey,
                                             const QString& model, const QString& protocol) {
  const auto effectiveApiKey = apiKey.trimmed().isEmpty()
      ? desktopSettings().value(QStringLiteral("ai/apiKey")).toString() : apiKey.trimmed();
  return modelChatClient_.requestModels({endpoint.trimmed(), effectiveApiKey, model.trimmed(), protocol.trimmed()});
}

bool WorkbenchRuntime::requestAiFablecutPlan(const QString& projectSnapshot, const QString& prompt) {
  QJsonParseError error;
  const auto project = QJsonDocument::fromJson(projectSnapshot.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError || !project.isObject() || prompt.trimmed().isEmpty() || aiRequestBusy_)
    return false;
  const auto projectObject = project.object();
  edward::ai::ProjectSnapshot snapshot;
  snapshot.revision = projectObject.value(QStringLiteral("revision")).toInteger(0);
  if (snapshot.revision == 0) snapshot.revision = projectObject.value(QStringLiteral("project")).toObject().value(QStringLiteral("revision")).toInteger(0);
  const auto clips = projectObject.value(QStringLiteral("clips")).isArray()
      ? projectObject.value(QStringLiteral("clips")).toArray()
      : projectObject.value(QStringLiteral("project")).toObject().value(QStringLiteral("clips")).toArray();
  for (const auto& value : clips) {
    const auto id = value.toObject().value(QStringLiteral("id")).toString();
    if (!id.isEmpty()) snapshot.knownTargetIds.append(id);
  }
  const auto resources = projectObject.value(QStringLiteral("resources")).toArray();
  for (const auto& value : resources) {
    const auto id = value.isString() ? value.toString() : value.toObject().value(QStringLiteral("id")).toString();
    if (!id.isEmpty()) snapshot.verifiedResourceIds.append(id);
  }
  pendingAiProject_ = snapshot;
  pendingAiActionPlan_.clear();
  aiStreamingText_.clear();
  const auto settings = desktopSettings();
  const edward::resources::ModelChatConfig config{
      settings.value(QStringLiteral("ai/endpoint")).toString(),
      settings.value(QStringLiteral("ai/apiKey")).toString(),
      settings.value(QStringLiteral("ai/model")).toString(),
      settings.value(QStringLiteral("ai/protocol")).toString()};
  aiRequestBusy_ = true;
  aiChatRequestActive_ = true;
  aiConversation_.append(QStringLiteral("用户：%1\n").arg(prompt.trimmed()));
  aiConversation_.append(QStringLiteral("AI："));
  QJsonObject context;
  context.insert(QStringLiteral("project"), projectObject.value(QStringLiteral("project")));
  context.insert(QStringLiteral("fps"), projectObject.value(QStringLiteral("fps")));
  context.insert(QStringLiteral("revision"), projectObject.value(QStringLiteral("revision")));
  context.insert(QStringLiteral("playhead"), projectObject.value(QStringLiteral("playhead")));
  context.insert(QStringLiteral("selectedClipIds"), projectObject.value(QStringLiteral("playhead")).toObject().value(QStringLiteral("selectedClipIds")));
  context.insert(QStringLiteral("markers"), projectObject.value(QStringLiteral("markers")));
  context.insert(QStringLiteral("clips"), clips);
  context.insert(QStringLiteral("tracks"), projectObject.value(QStringLiteral("tracks")));
  context.insert(QStringLiteral("capabilities"), projectObject.value(QStringLiteral("capabilities")));
  const auto contextualPrompt = prompt.trimmed() + QStringLiteral("\n\n[Edward 当前编辑上下文，请严格以此为准]\n")
      + QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact));
  emit timelineChanged();
  const auto accepted = modelChatClient_.requestStreaming(
      config, QStringLiteral("You are Edward, a video editing assistant. Answer in concise Chinese. Treat the supplied Edward editing context and its capabilities list as the program contract, not as an example of one conversation. Translate any visible user editing intent into one or more declared capability operations; do not invent a conversation-specific business rule or a special case for a marker number, phrase, clip name, or previous request. Never ask the user for internal IDs, baseProjectRevision, fps, source code, or project files. Resolve visible references such as the selected clip/component, playhead, timeline position, marker label, track, duration, color, or other declared property from the supplied context. If selectedClipIds is non-empty, selected/current clip references target those clips; otherwise use the playhead target only when unambiguous. For ordinary questions, reply with text only. For a requested project edit, return only one JSON object matching edward.action-plan.v1 with schemaVersion, requestId, baseProjectRevision and operations. Use only the operation types and fields in capabilities; copy the supplied revision value, but if omitted the desktop runtime will bind the current revision. Convert user-facing seconds to the required frame fields using the supplied project fps. Never ask the user to calculate frames or provide internal metadata. Never include shell commands, file paths, export actions, credentials, or undeclared properties. Do not claim a change was applied; the desktop runtime previews the plan and waits for confirmation."),
      contextualPrompt);
  if (!accepted) {
    aiChatRequestActive_ = false;
    aiRequestBusy_ = false;
    aiConversation_.append(QStringLiteral("AI：请求未发送，请检查模型设置。\n"));
    emit timelineChanged();
  }
  return accepted;
}

void WorkbenchRuntime::clearPendingAiActionPlan() {
  if (pendingAiActionPlan_.isEmpty()) return;
  pendingAiActionPlan_.clear();
  emit timelineChanged();
}

void WorkbenchRuntime::setPendingFablecutExportPath(const QString& path) {
  if (pendingExportPath_ == path) return;
  pendingExportPath_ = path;
  emit runtimeChanged();
}

bool WorkbenchRuntime::exportFileExists(const QString& path) const {
  return !path.trimmed().isEmpty() && QFileInfo::exists(path);
}

bool WorkbenchRuntime::removeExportFile(const QString& path) const {
  if (path.trimmed().isEmpty()) return false;
  const QFileInfo info(path);
  bool removed = !info.exists() || QFile::remove(info.absoluteFilePath());
  const QString partPath = info.absolutePath() + QStringLiteral("/.part") + info.fileName();
  if (QFileInfo::exists(partPath)) removed = QFile::remove(partPath) && removed;
  return removed;
}

}  // namespace edward::desktop
