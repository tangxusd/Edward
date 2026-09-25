#pragma once

#include "edward/core/native_runtime_component.hpp"
#include "edward/ai/ai_orchestrator.hpp"
#include "edward/ai/capability_registry.hpp"
#include "edward/desktop/preference_store.hpp"
#include "edward/desktop/execution_ledger.hpp"
#include "edward/resources/model_chat_client.hpp"
#include "edward/resources/supabase_auth_client.hpp"
#include "edward/runtime/runtime_manifest.hpp"
#include "edward/runtime/web_runtime_host.hpp"

#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace edward::desktop {

class WorkbenchRuntime final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool nativeRuntimeSelected READ nativeRuntimeSelected NOTIFY runtimeChanged)
  Q_PROPERTY(QJsonObject nativeRuntimeProps READ nativeRuntimeProps NOTIFY runtimeChanged)
  Q_PROPERTY(QString nativeRuntimePreviewEntry READ nativeRuntimePreviewEntry NOTIFY runtimeChanged)
  Q_PROPERTY(QJsonObject nativeRuntimeHostMessage READ nativeRuntimeHostMessage NOTIFY runtimeChanged)
  Q_PROPERTY(QString pendingFablecutExportPath READ pendingFablecutExportPath WRITE setPendingFablecutExportPath NOTIFY runtimeChanged)
  Q_PROPERTY(QVariantList clips READ clips NOTIFY runtimeChanged)
  Q_PROPERTY(QString aiConversation READ aiConversation NOTIFY timelineChanged)
  Q_PROPERTY(bool aiRequestBusy READ aiRequestBusy NOTIFY timelineChanged)
  Q_PROPERTY(QString aiRequestStage READ aiRequestStage NOTIFY timelineChanged)
  Q_PROPERTY(QString pendingAiActionPlan READ pendingAiActionPlan NOTIFY timelineChanged)

 public:
  explicit WorkbenchRuntime(QObject* parent = nullptr);
  [[nodiscard]] PreferenceStore* preferenceStore() { return &preferenceStore_; }
  [[nodiscard]] bool nativeRuntimeSelected() const { return component_.has_value(); }
  [[nodiscard]] QJsonObject nativeRuntimeProps() const;
  [[nodiscard]] QString nativeRuntimePreviewEntry() const;
  [[nodiscard]] QJsonObject nativeRuntimeHostMessage() const;
  [[nodiscard]] QString pendingFablecutExportPath() const { return pendingExportPath_; }
  [[nodiscard]] QVariantList clips() const;
  [[nodiscard]] QString aiConversation() const { return aiConversation_; }
  [[nodiscard]] bool aiRequestBusy() const { return aiRequestBusy_; }
  [[nodiscard]] QString aiRequestStage() const { return aiRequestStage_; }
  [[nodiscard]] QString pendingAiActionPlan() const { return pendingAiActionPlan_; }
  [[nodiscard]] QImage previewFrame() const { return {}; }
  [[nodiscard]] QImage clipThumbnail(qlonglong) const { return {}; }

  Q_INVOKABLE bool addNativeRuntimePackage(const QString& packageRoot, const QJsonObject& props = {});
  Q_INVOKABLE bool setNativeRuntimeProps(const QJsonObject& props);
  Q_INVOKABLE bool selectClip(qlonglong id);
  Q_INVOKABLE bool saveProject(const QString& path) const;
  Q_INVOKABLE bool loadProject(const QString& path);
  Q_INVOKABLE bool flushPreferencesForProjectClose();
  Q_INVOKABLE bool flushPreferencesForExport();
  Q_INVOKABLE bool compilePreferencesNow();
  Q_INVOKABLE QVariantMap preferenceStoreStatus() const;
  Q_INVOKABLE QVariantMap fablecutSettings() const;
  Q_INVOKABLE QVariantMap fablecutAuthSession() const;
  Q_INVOKABLE void setFablecutAuthState(bool authenticated);
  Q_INVOKABLE bool saveFablecutAuthSession(const QVariantMap& session);
  void setSupabaseAuthConfig(const edward::resources::SupabaseAuthConfig& config);
  Q_INVOKABLE bool submitFablecutRegistration(const QString& email, const QString& password, const QString& username);
  Q_INVOKABLE bool submitFablecutPasswordRecovery(const QString& email);
  Q_INVOKABLE bool submitFablecutDeviceEnrollment(const QString& accessToken);
  Q_INVOKABLE bool clearFablecutAuthSession();
  Q_INVOKABLE bool saveFablecutSettings(const QString& providerId, const QString& provider,
                                        const QString& endpoint, const QString& apiKey, const QString& model,
                                        const QString& protocol,
                                        const QString& exportDirectory);
  Q_INVOKABLE QString chooseFablecutExportDirectory(const QString& currentDirectory = {});
  Q_INVOKABLE bool saveFablecutPathSettings(const QVariantMap& paths);
  Q_INVOKABLE bool migrateFablecutPath(const QString& key, const QString& destination);
  Q_INVOKABLE bool clearDerivedFablecutPath(const QString& key);
  Q_INVOKABLE bool exportFablecutPreferences(const QString& path);
  Q_INVOKABLE bool importFablecutPreferences(const QString& path);
  Q_INVOKABLE QString chooseFablecutPreferencesExportPath();
  Q_INVOKABLE QString chooseFablecutPreferencesImportPath();
  Q_INVOKABLE QVariantList fablecutPreferenceFacts();
  Q_INVOKABLE bool importFablecutPreferenceFacts(const QVariantList& facts);
  Q_INVOKABLE QVariantMap fablecutDiagnostics();
  Q_INVOKABLE bool recordFablecutDiagnostic(const QString& event);
  Q_INVOKABLE bool testFablecutAiProvider(const QString& endpoint, const QString& apiKey,
                                          const QString& model, const QString& protocol);
  Q_INVOKABLE bool fetchFablecutAiModels(const QString& endpoint, const QString& apiKey,
                                         const QString& model, const QString& protocol);
  Q_INVOKABLE bool requestAiFablecutPlan(const QString& projectSnapshot, const QString& prompt);
  Q_INVOKABLE QJsonObject fablecutCapabilitySnapshot() const;
  Q_INVOKABLE QString currentPendingAiActionPlan() const { return pendingAiActionPlan_; }
  Q_INVOKABLE void clearPendingAiActionPlan();
  Q_INVOKABLE void setPendingFablecutExportPath(const QString& path);
  Q_INVOKABLE bool exportFileExists(const QString& path) const;
  Q_INVOKABLE bool removeExportFile(const QString& path) const;

 signals:
  void runtimeChanged();
  void operationFailed(const QString& message);
  void settingsOperationCompleted(const QString& operation, bool success,
                                  const QString& message, const QStringList& values = {});
  void timelineChanged();
  void fablecutAuthStateChanged(bool authenticated);

 private:
  bool mount(const QString& packageRoot, const QJsonObject& props);
  PreferenceStore preferenceStore_;
  ExecutionLedger executionLedger_;
  edward::resources::ModelChatClient modelChatClient_;
  edward::ai::AiOrchestrator aiOrchestrator_;
  edward::resources::SupabaseAuthClient authClient_;
  edward::resources::SupabaseAuthConfig authConfig_;
  QString pendingAuthenticationOperation_;
  std::optional<edward::core::NativeRuntimeComponent> component_;
  std::optional<edward::runtime::RuntimeManifest> manifest_;
  edward::runtime::WebRuntimeHost host_;
  QString pendingExportPath_;
  QString aiConversation_;
  bool aiRequestBusy_ = false;
  QString aiRequestStage_ = QStringLiteral("idle");
  bool aiChatRequestActive_ = false;
  QString pendingAiActionPlan_;
  QString aiStreamingText_;
  edward::ai::ProjectSnapshot pendingAiProject_;
  edward::ai::ReferenceSnapshot pendingAiReferences_;
  edward::ai::CapabilitySnapshot pendingAiCapabilities_;
  LedgerEntry pendingAiLedgerEntry_;
  qlonglong clipId_ = 0;
};

}  // namespace edward::desktop
