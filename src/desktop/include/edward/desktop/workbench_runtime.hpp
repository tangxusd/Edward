#pragma once

#include "edward/desktop/timeline_controller.hpp"
#include "edward/desktop/preference_store.hpp"
#include "edward/core/project_identity.hpp"

#include <QObject>
#include <QVariantList>
#include <QImage>
#include <QJsonArray>
#include <QFutureWatcher>
#include <QHash>
#include <QTimer>
#include <memory>
#include <atomic>

#include "edward/media/mlt_adapter.hpp"
#include "edward/media/audio_preview.hpp"
#include "edward/media/preview_session.hpp"
#include "edward/media/preview_frame_cache.hpp"
#include "edward/media/render_graph.hpp"
#include "edward/media/render_storage.hpp"
#include "edward/media/export_job.hpp"
#include "edward/plugins/installed_plugin.hpp"
#include "edward/resources/auth_session_store.hpp"
#include "edward/resources/component_upload.hpp"
#include "edward/resources/component_upload_dispatcher.hpp"
#include "edward/resources/component_library.hpp"
#include "edward/resources/fusion_artifact_cache.hpp"
#include "edward/resources/model_chat_client.hpp"
#include "edward/resources/resolve_api_manuals.hpp"
#include "edward/resources/supabase_auth_client.hpp"
#include "edward/resources/preference_sync_client.hpp"
#include "edward/resolve/resolve_adapter.hpp"
#include "edward/resolve/resolve_action_planner.hpp"
#include "edward/resolve/resolve_capability_telemetry.hpp"

namespace edward::desktop {

class WorkbenchRuntime final : public QObject {
  Q_OBJECT
  Q_PROPERTY(int playheadFrame READ playheadFrame NOTIFY timelineChanged)
  Q_PROPERTY(int timelineDurationFrames READ timelineDurationFrames NOTIFY timelineChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY timelineChanged)
  Q_PROPERTY(int videoTrackCount READ videoTrackCount NOTIFY timelineChanged)
  Q_PROPERTY(int selectedVideoTrackIndex READ selectedVideoTrackIndex NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList clips READ clips NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList transitions READ transitions NOTIFY timelineChanged)
  Q_PROPERTY(bool demoOverlayEnabled READ demoOverlayEnabled NOTIFY timelineChanged)
  Q_PROPERTY(bool componentBoundToClip READ componentBoundToClip NOTIFY timelineChanged)
  Q_PROPERTY(bool componentPlayheadEditable READ componentPlayheadIsEditable NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayX READ demoOverlayX WRITE setDemoOverlayX NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayY READ demoOverlayY WRITE setDemoOverlayY NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayWidth READ demoOverlayWidth WRITE setDemoOverlayWidth NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayHeight READ demoOverlayHeight WRITE setDemoOverlayHeight NOTIFY timelineChanged)
  Q_PROPERTY(double demoOverlayScale READ demoOverlayScale WRITE setDemoOverlayScale NOTIFY timelineChanged)
  Q_PROPERTY(double demoOverlayRotation READ demoOverlayRotation WRITE setDemoOverlayRotation NOTIFY timelineChanged)
  Q_PROPERTY(double demoOverlayOpacity READ demoOverlayOpacity WRITE setDemoOverlayOpacity NOTIFY timelineChanged)
  Q_PROPERTY(QString demoOverlayText READ demoOverlayText WRITE setDemoOverlayText NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayFontSize READ demoOverlayFontSize WRITE setDemoOverlayFontSize NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayBorderWidth READ demoOverlayBorderWidth WRITE setDemoOverlayBorderWidth NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList componentNodes READ componentNodes NOTIFY timelineChanged)
  Q_PROPERTY(QString selectedComponentNodeId READ selectedComponentNodeId NOTIFY timelineChanged)
  Q_PROPERTY(QString selectedComponentNodeType READ selectedComponentNodeType NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList selectedComponentNodeKeyframes READ selectedComponentNodeKeyframes NOTIFY timelineChanged)
  Q_PROPERTY(int selectedComponentNodeX READ selectedComponentNodeX WRITE setSelectedComponentNodeX NOTIFY timelineChanged)
  Q_PROPERTY(int selectedComponentNodeY READ selectedComponentNodeY WRITE setSelectedComponentNodeY NOTIFY timelineChanged)
  Q_PROPERTY(int selectedComponentNodeWidth READ selectedComponentNodeWidth WRITE setSelectedComponentNodeWidth NOTIFY timelineChanged)
  Q_PROPERTY(int selectedComponentNodeHeight READ selectedComponentNodeHeight WRITE setSelectedComponentNodeHeight NOTIFY timelineChanged)
  Q_PROPERTY(double selectedComponentNodeRotation READ selectedComponentNodeRotation WRITE setSelectedComponentNodeRotation NOTIFY timelineChanged)
  Q_PROPERTY(double selectedComponentNodeScale READ selectedComponentNodeScale WRITE setSelectedComponentNodeScale NOTIFY timelineChanged)
  Q_PROPERTY(double selectedComponentNodeOpacity READ selectedComponentNodeOpacity WRITE setSelectedComponentNodeOpacity NOTIFY timelineChanged)
  Q_PROPERTY(QString selectedComponentNodeColor READ selectedComponentNodeColor WRITE setSelectedComponentNodeColor NOTIFY timelineChanged)
  Q_PROPERTY(QString selectedComponentNodeBorderColor READ selectedComponentNodeBorderColor WRITE setSelectedComponentNodeBorderColor NOTIFY timelineChanged)
  Q_PROPERTY(QString selectedComponentNodeFontFamily READ selectedComponentNodeFontFamily WRITE setSelectedComponentNodeFontFamily NOTIFY timelineChanged)
  Q_PROPERTY(bool installedPluginAvailable READ installedPluginAvailable NOTIFY timelineChanged)
  Q_PROPERTY(QString installedPluginId READ installedPluginId NOTIFY timelineChanged)
  Q_PROPERTY(QString componentPluginDependencyStatus READ componentPluginDependencyStatus NOTIFY timelineChanged)
  Q_PROPERTY(bool pluginRenderBusy READ pluginRenderBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool pluginExportBusy READ pluginExportBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool timelineExportBusy READ timelineExportBusy NOTIFY timelineChanged)
  Q_PROPERTY(int timelineExportProgress READ timelineExportProgress NOTIFY timelineChanged)
  Q_PROPERTY(QString pendingFablecutExportPath READ pendingFablecutExportPath WRITE setPendingFablecutExportPath NOTIFY timelineChanged)
  Q_PROPERTY(QString savedExportOutputDirectory READ savedExportOutputDirectory NOTIFY timelineChanged)
  Q_PROPERTY(bool exportDialogRequested READ exportDialogRequested NOTIFY timelineChanged)
  Q_PROPERTY(bool authenticated READ authenticated NOTIFY timelineChanged)
  Q_PROPERTY(QString authenticatedUsername READ authenticatedUsername NOTIFY timelineChanged)
  Q_PROPERTY(QString supabaseAccessToken READ supabaseAccessToken NOTIFY timelineChanged)
  Q_PROPERTY(QString subscriptionStatus READ subscriptionStatus NOTIFY timelineChanged)
  Q_PROPERTY(QString subscriptionExpiresAt READ subscriptionExpiresAt NOTIFY timelineChanged)
  Q_PROPERTY(qint64 subscriptionCredits READ subscriptionCredits NOTIFY timelineChanged)
  Q_PROPERTY(QString paymentQrCode READ paymentQrCode NOTIFY timelineChanged)
  Q_PROPERTY(bool paymentBusy READ paymentBusy NOTIFY timelineChanged)
  Q_PROPERTY(QString paymentOrderId READ paymentOrderId NOTIFY timelineChanged)
  Q_PROPERTY(bool signInBusy READ signInBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool componentUploadBusy READ componentUploadBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool aiComponentDraftAvailable READ aiComponentDraftAvailable NOTIFY timelineChanged)
  Q_PROPERTY(bool aiAnalysisDraftAvailable READ aiAnalysisDraftAvailable NOTIFY timelineChanged)
  Q_PROPERTY(QString aiComponentDraft READ aiComponentDraft NOTIFY timelineChanged)
  Q_PROPERTY(bool aiRequestBusy READ aiRequestBusy NOTIFY timelineChanged)
  Q_PROPERTY(QString aiConversation READ aiConversation NOTIFY timelineChanged)
  Q_PROPERTY(QString aiModelEndpoint READ aiModelEndpoint NOTIFY timelineChanged)
  Q_PROPERTY(QString aiProviderName READ aiProviderName NOTIFY timelineChanged)
  Q_PROPERTY(QString aiTestModel READ aiTestModel NOTIFY timelineChanged)
  Q_PROPERTY(QString aiModelList READ aiModelList NOTIFY timelineChanged)
  Q_PROPERTY(QString aiContextWindow READ aiContextWindow NOTIFY timelineChanged)
  Q_PROPERTY(QStringList aiAvailableModels READ aiAvailableModels NOTIFY timelineChanged)
  Q_PROPERTY(bool aiModelListBusy READ aiModelListBusy NOTIFY timelineChanged)
  Q_PROPERTY(QString aiModelId READ aiModelId NOTIFY timelineChanged)
  Q_PROPERTY(bool aiModelConfigured READ aiModelConfigured NOTIFY timelineChanged)
  Q_PROPERTY(bool aiModelCredentialsConfigured READ aiModelCredentialsConfigured NOTIFY timelineChanged)
  Q_PROPERTY(QString resolveApiAssessment READ resolveApiAssessment NOTIFY timelineChanged)
  Q_PROPERTY(QString resolveActionPlan READ resolveActionPlan NOTIFY timelineChanged)
  Q_PROPERTY(QString resolveActionLastResult READ resolveActionLastResult NOTIFY timelineChanged)
  Q_PROPERTY(QString resolveRebuildTransactionId READ resolveRebuildTransactionId NOTIFY timelineChanged)
  Q_PROPERTY(QString resolveRebuildTransactionState READ resolveRebuildTransactionState NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList localComponents READ localComponents NOTIFY timelineChanged)
  Q_PROPERTY(QString projectWindowTitle READ projectWindowTitle NOTIFY timelineChanged)
  Q_PROPERTY(int previewQuality READ previewQuality NOTIFY timelineChanged)
  Q_PROPERTY(bool previewProxyBusy READ previewProxyBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool previewProxyReady READ previewProxyReady NOTIFY timelineChanged)
  Q_PROPERTY(QString proxyStorageRoot READ proxyStorageRoot NOTIFY timelineChanged)
  Q_PROPERTY(QString cacheStorageRoot READ cacheStorageRoot NOTIFY timelineChanged)
  Q_PROPERTY(QString renderStorageRoot READ renderStorageRoot NOTIFY timelineChanged)
  Q_PROPERTY(bool qualityImprovementEnabled READ qualityImprovementEnabled NOTIFY timelineChanged)
  Q_PROPERTY(bool qualityImprovementNoticeRequired READ qualityImprovementNoticeRequired NOTIFY timelineChanged)
  Q_PROPERTY(bool resolveConnected READ resolveConnected NOTIFY resolveStateChanged)
  Q_PROPERTY(bool resolveComponentImportBusy READ resolveComponentImportBusy NOTIFY timelineChanged)
  Q_PROPERTY(QString resolveStatus READ resolveStatus NOTIFY resolveStateChanged)
  Q_PROPERTY(QString resolveContextSummary READ resolveContextSummary NOTIFY resolveStateChanged)
  Q_PROPERTY(QString resolveSelectionId READ resolveSelectionId NOTIFY resolveStateChanged)
  Q_PROPERTY(QString resolveSelectionKind READ resolveSelectionKind NOTIFY resolveStateChanged)
  Q_PROPERTY(QString resolveSelectionName READ resolveSelectionName NOTIFY resolveStateChanged)
  Q_PROPERTY(QVariantList resolveRenderFormats READ resolveRenderFormats NOTIFY resolveRenderCapabilitiesChanged)
  Q_PROPERTY(QVariantList resolveRenderCodecs READ resolveRenderCodecs NOTIFY resolveRenderCapabilitiesChanged)
  Q_PROPERTY(QVariantList resolveRenderResolutions READ resolveRenderResolutions NOTIFY resolveRenderCapabilitiesChanged)

 public:
  explicit WorkbenchRuntime(QObject* parent = nullptr);
  [[nodiscard]] PreferenceStore* preferenceStore() { return &preferenceStore_; }
  [[nodiscard]] int playheadFrame() const;
  [[nodiscard]] int timelineDurationFrames() const;
  [[nodiscard]] bool playing() const { return playing_; }
  [[nodiscard]] int videoTrackCount() const;
  [[nodiscard]] int selectedVideoTrackIndex() const;
  [[nodiscard]] QVariantList clips() const;
  [[nodiscard]] QVariantList transitions() const;
  [[nodiscard]] QImage clipThumbnail(qlonglong id) const;
  [[nodiscard]] bool demoOverlayEnabled() const { return demoOverlayEnabled_; }
  [[nodiscard]] bool componentBoundToClip() const { return componentClipId_ != 0; }
  [[nodiscard]] bool componentPlayheadIsEditable() const;
  [[nodiscard]] int demoOverlayX() const { return demoOverlayX_; }
  [[nodiscard]] int demoOverlayY() const { return demoOverlayY_; }
  [[nodiscard]] int demoOverlayWidth() const { return demoOverlayWidth_; }
  [[nodiscard]] int demoOverlayHeight() const { return demoOverlayHeight_; }
  [[nodiscard]] double demoOverlayScale() const { return demoOverlayScale_; }
  [[nodiscard]] double demoOverlayRotation() const { return demoOverlayRotation_; }
  [[nodiscard]] double demoOverlayOpacity() const { return demoOverlayOpacity_; }
  [[nodiscard]] QString demoOverlayText() const { return demoOverlayText_; }
  [[nodiscard]] int demoOverlayFontSize() const { return demoOverlayFontSize_; }
  [[nodiscard]] int demoOverlayBorderWidth() const { return demoOverlayBorderWidth_; }
  [[nodiscard]] QVariantList componentNodes() const;
  [[nodiscard]] QString selectedComponentNodeId() const { return selectedComponentNodeId_; }
  [[nodiscard]] QString selectedComponentNodeType() const;
  [[nodiscard]] QVariantList selectedComponentNodeKeyframes() const;
  [[nodiscard]] int selectedComponentNodeX() const;
  [[nodiscard]] int selectedComponentNodeY() const;
  [[nodiscard]] int selectedComponentNodeWidth() const;
  [[nodiscard]] int selectedComponentNodeHeight() const;
  [[nodiscard]] double selectedComponentNodeRotation() const;
  [[nodiscard]] double selectedComponentNodeScale() const;
  [[nodiscard]] double selectedComponentNodeOpacity() const;
  [[nodiscard]] QString selectedComponentNodeColor() const;
  [[nodiscard]] QString selectedComponentNodeBorderColor() const;
  [[nodiscard]] QString selectedComponentNodeFontFamily() const;
  [[nodiscard]] bool installedPluginAvailable() const { return installedPlugin_.has_value(); }
  [[nodiscard]] QString installedPluginId() const;
  [[nodiscard]] QString componentPluginDependencyStatus() const;
  [[nodiscard]] bool pluginRenderBusy() const { return pluginRenderBusy_; }
  [[nodiscard]] bool pluginExportBusy() const { return pluginExportBusy_; }
  [[nodiscard]] bool timelineExportBusy() const { return timelineExportBusy_; }
  [[nodiscard]] int timelineExportProgress() const { return timelineExportProgress_; }
  [[nodiscard]] QString pendingFablecutExportPath() const { return pendingFablecutExportPath_; }
  [[nodiscard]] QString savedExportOutputDirectory() const;
  [[nodiscard]] bool authenticated() const { return sessions_.authenticated(); }
  [[nodiscard]] QString authenticatedUsername() const { return sessions_.username(); }
  [[nodiscard]] QString supabaseAccessToken() const { return sessions_.session().accessToken; }
  [[nodiscard]] QString subscriptionStatus() const { return subscriptionStatus_; }
  [[nodiscard]] QString subscriptionExpiresAt() const { return subscriptionExpiresAt_; }
  [[nodiscard]] qint64 subscriptionCredits() const { return subscriptionCredits_; }
  [[nodiscard]] QString paymentQrCode() const { return paymentQrCode_; }
  [[nodiscard]] bool paymentBusy() const { return paymentBusy_; }
  [[nodiscard]] QString paymentOrderId() const { return paymentOrderId_; }
  [[nodiscard]] bool signInBusy() const { return signInBusy_; }
  [[nodiscard]] bool componentUploadBusy() const { return componentUploadBusy_; }
  [[nodiscard]] bool aiComponentDraftAvailable() const { return aiComponentDraft_.has_value(); }
  [[nodiscard]] bool aiAnalysisDraftAvailable() const {
    return aiComponentDraft_.has_value() && aiComponentDraftIsAnalysis_;
  }
  [[nodiscard]] QString aiComponentDraft() const { return aiComponentDraftJson_; }
  [[nodiscard]] bool aiRequestBusy() const { return aiRequestBusy_; }
  [[nodiscard]] QString aiConversation() const { return aiConversation_; }
  Q_INVOKABLE void appendAiConversationError(const QString& message);
  [[nodiscard]] QString aiModelEndpoint() const { return aiModelEndpoint_; }
  [[nodiscard]] QString aiProviderName() const { return aiProviderName_; }
  [[nodiscard]] QString aiTestModel() const { return aiTestModel_; }
  [[nodiscard]] QString aiModelList() const { return aiModelList_; }
  [[nodiscard]] QString aiContextWindow() const { return aiContextWindow_; }
  [[nodiscard]] QStringList aiAvailableModels() const { return aiAvailableModels_; }
  [[nodiscard]] bool aiModelListBusy() const { return aiModelListBusy_; }
  [[nodiscard]] QString aiModelId() const { return aiModelId_; }
  [[nodiscard]] bool aiModelConfigured() const { return !aiModelEndpoint_.isEmpty() && !aiModelId_.isEmpty() && !aiModelApiKey_.isEmpty(); }
  [[nodiscard]] bool aiModelCredentialsConfigured() const { return !aiModelEndpoint_.isEmpty() && !aiModelApiKey_.isEmpty(); }
  [[nodiscard]] QString resolveApiAssessment() const { return resolveApiAssessment_; }
  [[nodiscard]] QString resolveActionPlan() const { return resolveActionPlan_; }
  [[nodiscard]] QString resolveActionLastResult() const { return resolveActionLastResult_; }
  [[nodiscard]] QString resolveRebuildTransactionId() const { return resolveRebuildTransactionId_; }
  [[nodiscard]] QString resolveRebuildTransactionState() const { return resolveRebuildTransactionState_; }
  [[nodiscard]] QVariantList localComponents() const;
  [[nodiscard]] QString projectWindowTitle() const;
  [[nodiscard]] int previewQuality() const;
  [[nodiscard]] bool previewProxyBusy() const { return previewProxyBusy_; }
  [[nodiscard]] bool previewProxyReady() const;
  [[nodiscard]] QString proxyStorageRoot() const;
  [[nodiscard]] QString cacheStorageRoot() const;
  [[nodiscard]] QString renderStorageRoot() const;
  [[nodiscard]] bool qualityImprovementEnabled() const;
  [[nodiscard]] bool qualityImprovementNoticeRequired() const;
  [[nodiscard]] QJsonObject componentJson() const;
  void setDemoOverlayX(int value);
  void setDemoOverlayY(int value);
  void setDemoOverlayWidth(int value);
  void setDemoOverlayHeight(int value);
  void setDemoOverlayScale(double value);
  void setDemoOverlayRotation(double value);
  void setDemoOverlayOpacity(double value);
  void setDemoOverlayText(const QString& value);
  void setDemoOverlayFontSize(int value);
  void setDemoOverlayBorderWidth(int value);
  void setSelectedComponentNodeX(int value);
  void setSelectedComponentNodeY(int value);
  void setSelectedComponentNodeWidth(int value);
  void setSelectedComponentNodeHeight(int value);
  void setSelectedComponentNodeRotation(double value);
  void setSelectedComponentNodeScale(double value);
  void setSelectedComponentNodeOpacity(double value);
  void setSelectedComponentNodeColor(const QString& value);
  void setSelectedComponentNodeBorderColor(const QString& value);
  void setSelectedComponentNodeFontFamily(const QString& value);
  [[nodiscard]] QImage previewFrame() const;
  Q_INVOKABLE bool importMedia(const QString& path);
  Q_INVOKABLE bool setPreviewQuality(int quality);
  Q_INVOKABLE bool configurePreviewStorageRoots(const QString& proxyRoot, const QString& cacheRoot,
                                                const QString& renderRoot);
  Q_INVOKABLE bool setQualityImprovementEnabled(bool enabled);
  Q_INVOKABLE bool acknowledgeQualityImprovementNotice();
  Q_INVOKABLE bool clearDerivedStorage(int kind);
  Q_INVOKABLE bool selectClip(qlonglong id);
  Q_INVOKABLE bool selectComponentNode(const QString& nodeId);
  Q_INVOKABLE bool removeSelectedComponentNodeKeyframe(const QString& field, int frame);
  Q_INVOKABLE bool toggleSelectedComponentNodeKeyframeEasing(const QString& field, int frame);
  Q_INVOKABLE void toggleDemoOverlay();
  Q_INVOKABLE bool bindComponentToSelectedClip();
  Q_INVOKABLE bool addCurrentComponentToTimeline(int durationFrames = 150);
  Q_INVOKABLE void generateComponentDraft();
  Q_INVOKABLE bool applyAiComponentCommand(const QString& json);
  Q_INVOKABLE bool requestAiComponentDraft(const QString& endpoint, const QString& apiKey,
                                           const QString& model, const QString& prompt);
  Q_INVOKABLE bool deleteLastAiInsertions(int count = -1, int index = -1);
  Q_INVOKABLE QString pasteAiAttachment();
  Q_INVOKABLE QStringList pasteAiAttachments();
  Q_INVOKABLE QStringList chooseAiAttachments();
  Q_INVOKABLE bool analyzeCurrentClipWithAi(const QString& prompt = {});
  Q_INVOKABLE bool configureAiModel(const QString& providerName, const QString& endpoint,
                                    const QString& apiKey, const QString& model,
                                    const QString& testModel = {}, const QString& modelList = {},
                                    const QString& contextWindow = {});
  Q_INVOKABLE bool refreshAiModelList();
  Q_INVOKABLE bool selectAiModel(const QString& model);
  Q_INVOKABLE void assessResolveRequest(const QString& request);
  Q_INVOKABLE void planResolveAction(const QString& request);
  Q_INVOKABLE bool executeResolveActionPlan();
  // 首页快捷操作的统一入口：先按当前 Resolve 上下文规划，再执行并回读。
  Q_INVOKABLE bool executeResolveQuickAction(const QString& action);
  Q_INVOKABLE bool beginResolveRebuild();
  Q_INVOKABLE bool importResolveRebuildFile(const QString& path);
  Q_INVOKABLE bool validateResolveRebuild(const QString& timelineName = {}, const QString& renderJobId = {}, const QString& outputPath = {});
  Q_INVOKABLE bool commitResolveRebuild();
  Q_INVOKABLE bool rollbackResolveRebuild();
  Q_INVOKABLE bool proposeAiComponentCommand(const QString& json);
  Q_INVOKABLE bool applyPendingAiComponentCommand();
  Q_INVOKABLE bool applyAiAnalysisDraftToResolve(int durationFrames = 150);
  Q_INVOKABLE void discardPendingAiComponentCommand();
  Q_INVOKABLE bool loadComponentJson(const QString& json);
  Q_INVOKABLE bool loadComponentFile(const QString& path);
  Q_INVOKABLE bool saveComponentJson(const QString& path) const;
  Q_INVOKABLE bool saveComponentPackage(const QString& directory, const QString& resourceId,
                                        const QString& displayName);
  Q_INVOKABLE bool configureComponentLibrary(const QString& rootPath);
  Q_INVOKABLE bool saveCurrentComponentToLibrary(const QString& resourceId, const QString& displayName,
                                                 const QString& category = QStringLiteral("my"));
  Q_INVOKABLE bool loadLibraryComponent(const QString& resourceId);
  Q_INVOKABLE bool insertLibraryComponentAtPlayhead(const QString& resourceId,
                                                     int durationFrames = 150);
  Q_INVOKABLE bool downloadFusionArtifact(const QString& url, const QString& destination,
                                          const QString& sha256);
  Q_INVOKABLE bool configureSilentComponentUploads(const QString& endpoint, const QString& statePath,
                                                    const QString& pendingRoot);
  Q_INVOKABLE bool signInWithSupabase(const QString& projectUrl, const QString& anonKey,
                                      const QString& email, const QString& password);
  Q_INVOKABLE bool signUpWithSupabase(const QString& projectUrl, const QString& anonKey,
                                      const QString& email, const QString& password);
  Q_INVOKABLE bool sendSupabasePasswordReset(const QString& projectUrl, const QString& anonKey,
                                             const QString& email);
  Q_INVOKABLE bool refreshSupabaseEntitlement(const QString& projectUrl, const QString& anonKey);
  Q_INVOKABLE bool refreshSupabaseSession(const QString& projectUrl, const QString& anonKey);
  Q_INVOKABLE bool createNativePayment(const QString& projectUrl, const QString& anonKey, double amount, const QString& goodsDesc);
  Q_INVOKABLE bool createAlipayNativePayment(const QString& projectUrl, const QString& anonKey, const QString& planKey);
  Q_INVOKABLE bool queryAlipayOrder(const QString& projectUrl, const QString& anonKey, const QString& orderId);
  Q_INVOKABLE void signOut();
  Q_INVOKABLE bool uploadCurrentComponent(const QString& endpoint, const QString& resourceId,
                                          const QString& displayName);
  Q_INVOKABLE bool loadPluginFrameJson(const QString& requestId, const QString& json);
  Q_INVOKABLE bool selectInstalledPlugin(const QString& rootPath);
  Q_INVOKABLE void clearInstalledPlugin();
  Q_INVOKABLE bool describeInstalledPlugin(const QString& compositionId);
  Q_INVOKABLE bool renderInstalledPluginFrame(const QString& requestId, const QString& compositionId);
  Q_INVOKABLE bool exportInstalledPlugin(const QString& requestId, const QString& compositionId,
                                         const QString& outputPath);
  Q_INVOKABLE bool applyInstalledPluginToTimeline(const QString& requestId, const QString& compositionId,
                                                  const QString& outputPath);
  Q_INVOKABLE bool exportTimeline(const QString& outputPath);
  Q_INVOKABLE bool exportTimelineWithOptions(const QString& outputPath, int width, int height,
                                             int fps, int quality);
  Q_INVOKABLE bool flushPreferencesForProjectClose();
  Q_INVOKABLE bool flushPreferencesForExport();
  Q_INVOKABLE bool compilePreferencesNow();
  Q_INVOKABLE QVariantMap preferenceStoreStatus() const;
  Q_INVOKABLE bool uploadPreferences(const QString& projectUrl, const QString& anonKey);
  Q_INVOKABLE bool downloadPreferences(const QString& projectUrl, const QString& anonKey);
  Q_INVOKABLE void setPendingFablecutExportPath(const QString& path);
  Q_INVOKABLE void setSavedExportOutputDirectory(const QString& path);
  Q_INVOKABLE void clearExportDialogRequest();
  Q_INVOKABLE void cancelTimelineExport();
  Q_INVOKABLE void clearComponentOverlay();
  Q_INVOKABLE bool setPlayhead(int frame);
  Q_INVOKABLE bool connectResolve(bool notifyFailure = true);
  Q_INVOKABLE void disconnectResolve();
  Q_INVOKABLE bool refreshResolveTimeline();
  Q_INVOKABLE bool refreshResolveRenderCapabilities();
  Q_INVOKABLE bool setSelectedComponentPropertyAtPlayhead(const QString& nodeId,
                                                          const QString& field,
                                                          const QJsonValue& value);
  Q_INVOKABLE QVariantList inspectSelectedResolveTool(const QString& toolName);
  Q_INVOKABLE bool setSelectedResolveAttribute(const QString& toolName,
                                               const QString& inputName,
                                               const QJsonValue& value);
  Q_INVOKABLE bool exportWithEdwardOptions(const QString& outputPath, int width, int height,
                                           int fps, int quality);
  Q_INVOKABLE bool openResolveDeliverPage();
  Q_INVOKABLE bool importResolveLayoutPreset(const QString& path, const QString& name = {});
  Q_INVOKABLE bool loadResolveLayoutPreset(const QString& name);
  Q_INVOKABLE bool saveResolveLayoutPreset(const QString& name);
  Q_INVOKABLE bool exportResolveLayoutPreset(const QString& name, const QString& path);
  Q_INVOKABLE bool ensureResolveSubtitleTrack();
  Q_INVOKABLE bool saveProject(const QString& path);
  Q_INVOKABLE bool loadProject(const QString& path);
  Q_INVOKABLE bool hasProjectRecovery(const QString& path) const;
  Q_INVOKABLE bool recoverProject(const QString& path);
  Q_INVOKABLE bool discardProjectRecovery(const QString& path);
  Q_INVOKABLE void togglePlayback();
  Q_INVOKABLE bool splitSelected();
  Q_INVOKABLE bool deleteSelected();
  Q_INVOKABLE bool rippleDeleteSelected();
  Q_INVOKABLE bool moveSelected(qlonglong destination);
  Q_INVOKABLE bool trimSelectedLeft();
  Q_INVOKABLE bool trimSelectedRight();
  Q_INVOKABLE bool addDissolveToSelected();
  Q_INVOKABLE bool addTransitionToSelected(const QString& type);
  Q_INVOKABLE bool setTransitionDuration(qlonglong leftClipId, qlonglong rightClipId,
                                         int durationFrames);
  Q_INVOKABLE bool removeTransition(qlonglong leftClipId, qlonglong rightClipId);
  Q_INVOKABLE bool addVideoTrack();
  Q_INVOKABLE bool removeEmptyVideoTrack();
  Q_INVOKABLE bool selectVideoTrack(int index);
  Q_INVOKABLE bool undoTimeline();
  Q_INVOKABLE bool redoTimeline();

 signals:
  void timelineChanged();
  void operationFailed(QString message);
  void operationSucceeded(QString message);
  void resolveStateChanged();
  void resolveRenderCapabilitiesChanged();

 private:
  bool exportInstalledPlugin(const QString& requestId, const QString& compositionId,
                             const QString& outputPath, bool applyToTimeline);
  void refreshDemoOverlay();
  void requestClipWaveform(const edward::core::TimelineClip& clip);
  void refreshClipWaveforms();
  void requestClipThumbnail(const edward::core::TimelineClip& clip);
  void refreshClipThumbnails();
  void syncDemoOverlayProperties(const QJsonObject& component);
  [[nodiscard]] QString recoveryPathForProject(const QString& path) const;
  [[nodiscard]] bool writeProject(const QString& path) const;
  void scheduleProjectAutosave();
  void saveProjectRecovery();
  [[nodiscard]] int componentKeyframeFrame() const;
  [[nodiscard]] edward::core::TimelineSnapshot previewSnapshot() const;
  edward::core::Timeline timeline_;
  edward::core::TrackId videoTrack_;
  TimelineController controller_;
  edward::core::ProjectIdentity projectIdentity_ = edward::core::ProjectIdentity::create();
  PreferenceStore preferenceStore_;
  edward::resources::PreferenceSyncClient preferenceSyncClient_;
  edward::media::RenderStorageRoots previewStorageRoots_;
  edward::media::PreviewSession previewSession_;
  mutable edward::media::PreviewFrameCache previewFrameCache_;
  QFutureWatcher<bool> previewProxyWatcher_;
  bool previewProxyBusy_ = false;
  edward::media::MltAdapter mltAdapter_;
  edward::media::RenderGraph renderGraph_;
  bool demoOverlayEnabled_ = false;
  std::optional<edward::core::ComponentIr> demoOverlayIr_;
  edward::core::ClipId componentClipId_ = 0;
  edward::core::ClipId editingComponentClipId_ = 0;
  QHash<QString, QString> resolveComponentNodeTimelineIds_;
  int demoOverlayX_ = 24;
  int demoOverlayY_ = 24;
  int demoOverlayWidth_ = 220;
  int demoOverlayHeight_ = 72;
  int demoOverlayFontSize_ = 18;
  int demoOverlayBorderWidth_ = 0;
  QString selectedComponentNodeId_ = QStringLiteral("demo-box");
  double demoOverlayScale_ = 1.0;
  double demoOverlayRotation_ = 0.0;
  double demoOverlayOpacity_ = 0.82;
  QString demoOverlayText_ = QStringLiteral("Edward Component");
  std::optional<edward::plugins::InstalledPlugin> installedPlugin_;
  struct PluginFrameResult { QImage frame; QString error; };
  QFutureWatcher<PluginFrameResult> pluginFrameWatcher_;
  bool pluginRenderBusy_ = false;
  struct PluginExportResult {
    QString error;
    QString outputPath;
    bool applyToTimeline = false;
  };
  QFutureWatcher<PluginExportResult> pluginExportWatcher_;
  bool pluginExportBusy_ = false;
  struct TimelineExportResult { QString error; QString outputPath; };
  QFutureWatcher<TimelineExportResult> timelineExportWatcher_;
  QTimer resolveRenderPollTimer_;
  QTimer resolveContextPollTimer_;
  QTimer resolveReconnectTimer_;
  QString resolveRenderJobId_;
  QString resolveRenderOutputPath_;
  bool timelineExportBusy_ = false;
  int timelineExportProgress_ = 0;
  QString pendingFablecutExportPath_;
  std::shared_ptr<std::atomic_bool> timelineExportCancel_;
  edward::resources::AuthSessionStore sessions_;
  edward::resources::SupabaseAuthClient authClient_;
  bool signInBusy_ = false;
  edward::resources::ComponentUploadClient componentUploadClient_;
  edward::resources::ComponentLibrary componentLibrary_;
  edward::resources::FusionArtifactCache fusionArtifactCache_;
  bool componentUploadBusy_ = false;
  std::optional<edward::core::ComponentIr> aiComponentDraft_;
  bool aiComponentDraftIsAnalysis_ = false;
  QString aiComponentDraftJson_;
  edward::resources::ModelChatClient modelChatClient_;
  bool aiRequestBusy_ = false;
  QString subscriptionStatus_;
  QString subscriptionExpiresAt_;
  qint64 subscriptionCredits_ = 0;
  QString paymentQrCode_;
  QString paymentOrderId_;
  bool paymentBusy_ = false;
  QString aiConversation_;
  QString aiModelEndpoint_;
  QString aiProviderName_;
  QString aiModelApiKey_;
  QString aiModelId_;
  QString aiTestModel_;
  QString aiModelList_;
  QString aiContextWindow_;
  QStringList aiAvailableModels_;
  bool aiModelListBusy_ = false;
  QString resolveApiAssessment_;
  QString resolveActionPlan_;
  QString resolveActionLastResult_;
  QString resolveRebuildTransactionId_;
  QString resolveRebuildTransactionState_;
  edward::resources::ResolveApiManuals resolveApiManuals_;
  QString pendingAiPrompt_;
  QJsonArray aiLastInsertionRecords_;
  bool pendingAiAnalysis_ = false;
  bool pendingAiConversationOnly_ = false;
  std::unique_ptr<edward::resources::ComponentUploadDispatcher> silentUploadDispatcher_;
  QString silentUploadEndpoint_;
  QTimer silentUploadRetryTimer_;
  QTimer playbackTimer_;
  QTimer projectAutosaveTimer_;
  QTimer preferenceIdleTimer_;
  QString activeProjectPath_;
  enum class ProjectSaveState { Unsaved, Saved, AutoSaved };
  ProjectSaveState projectSaveState_ = ProjectSaveState::Unsaved;
  bool writingProjectStatus_ = false;
  bool loadingProject_ = false;
  edward::media::AudioPreview audioPreview_;
  QHash<qint64, QVariantList> clipWaveforms_;
  quint64 waveformGeneration_ = 0;
  QHash<qint64, QImage> clipThumbnails_;
  quint64 thumbnailGeneration_ = 0;
  bool playing_ = false;
  edward::resolve::ResolveConnection resolveConnection_;
  edward::resolve::ResolveAdapter resolveAdapter_;
  bool resolveConnected_ = false;
  bool resolveComponentImportBusy_ = false;
  bool exportDialogRequested_ = false;
  QString resolveStatus_ = QStringLiteral("未连接 Resolve Studio");
  QString resolveContextSummary_ = QStringLiteral("尚未读取 Resolve 当前上下文");
  QString resolveSelectionId_;
  QString resolveSelectionKind_ = QStringLiteral("none");
  QString resolveSelectionName_;
  QVariantList resolveRenderFormats_;
  QVariantList resolveRenderCodecs_;
  QVariantList resolveRenderResolutions_;
  std::optional<edward::resolve::ResolveTimelineSnapshot> resolveTimelineSnapshot_;
  void recordResolveCapabilityEvent(const QString& capabilityId, bool success,
                                    const QString& errorCode = {});
  void recordResolveError(const QString& method, const QString& errorCode);
  void dispatchSilentComponentUploads();

 public:
  [[nodiscard]] bool resolveConnected() const { return resolveConnected_; }
  [[nodiscard]] bool exportDialogRequested() const { return exportDialogRequested_; }
  [[nodiscard]] bool resolveComponentImportBusy() const { return resolveComponentImportBusy_; }
  [[nodiscard]] QString resolveStatus() const { return resolveStatus_; }
  [[nodiscard]] QString resolveContextSummary() const { return resolveContextSummary_; }
  [[nodiscard]] QString resolveSelectionId() const { return resolveSelectionId_; }
  [[nodiscard]] QString resolveSelectionKind() const { return resolveSelectionKind_; }
  [[nodiscard]] QString resolveSelectionName() const { return resolveSelectionName_; }
  [[nodiscard]] QVariantList resolveRenderFormats() const { return resolveRenderFormats_; }
  [[nodiscard]] QVariantList resolveRenderCodecs() const { return resolveRenderCodecs_; }
  [[nodiscard]] QVariantList resolveRenderResolutions() const { return resolveRenderResolutions_; }
};

}  // namespace edward::desktop
