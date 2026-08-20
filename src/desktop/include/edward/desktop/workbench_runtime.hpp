#pragma once

#include "edward/desktop/timeline_controller.hpp"

#include <QObject>
#include <QVariantList>
#include <QImage>
#include <QFutureWatcher>
#include <QTimer>
#include <memory>

#include "edward/media/mlt_adapter.hpp"
#include "edward/media/render_graph.hpp"
#include "edward/plugins/installed_plugin.hpp"
#include "edward/resources/auth_session_store.hpp"
#include "edward/resources/component_upload.hpp"
#include "edward/resources/component_upload_dispatcher.hpp"
#include "edward/resources/model_chat_client.hpp"
#include "edward/resources/supabase_auth_client.hpp"

namespace edward::desktop {

class WorkbenchRuntime final : public QObject {
  Q_OBJECT
  Q_PROPERTY(int playheadFrame READ playheadFrame NOTIFY timelineChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY timelineChanged)
  Q_PROPERTY(int videoTrackCount READ videoTrackCount NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList clips READ clips NOTIFY timelineChanged)
  Q_PROPERTY(bool demoOverlayEnabled READ demoOverlayEnabled NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayX READ demoOverlayX WRITE setDemoOverlayX NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayY READ demoOverlayY WRITE setDemoOverlayY NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayWidth READ demoOverlayWidth WRITE setDemoOverlayWidth NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayHeight READ demoOverlayHeight WRITE setDemoOverlayHeight NOTIFY timelineChanged)
  Q_PROPERTY(double demoOverlayScale READ demoOverlayScale WRITE setDemoOverlayScale NOTIFY timelineChanged)
  Q_PROPERTY(double demoOverlayRotation READ demoOverlayRotation WRITE setDemoOverlayRotation NOTIFY timelineChanged)
  Q_PROPERTY(double demoOverlayOpacity READ demoOverlayOpacity WRITE setDemoOverlayOpacity NOTIFY timelineChanged)
  Q_PROPERTY(QString demoOverlayText READ demoOverlayText WRITE setDemoOverlayText NOTIFY timelineChanged)
  Q_PROPERTY(bool installedPluginAvailable READ installedPluginAvailable NOTIFY timelineChanged)
  Q_PROPERTY(QString installedPluginId READ installedPluginId NOTIFY timelineChanged)
  Q_PROPERTY(QString componentPluginDependencyStatus READ componentPluginDependencyStatus NOTIFY timelineChanged)
  Q_PROPERTY(bool pluginRenderBusy READ pluginRenderBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool pluginExportBusy READ pluginExportBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool timelineExportBusy READ timelineExportBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool authenticated READ authenticated NOTIFY timelineChanged)
  Q_PROPERTY(QString authenticatedUsername READ authenticatedUsername NOTIFY timelineChanged)
  Q_PROPERTY(bool signInBusy READ signInBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool componentUploadBusy READ componentUploadBusy NOTIFY timelineChanged)
  Q_PROPERTY(bool aiComponentDraftAvailable READ aiComponentDraftAvailable NOTIFY timelineChanged)
  Q_PROPERTY(QString aiComponentDraft READ aiComponentDraft NOTIFY timelineChanged)
  Q_PROPERTY(bool aiRequestBusy READ aiRequestBusy NOTIFY timelineChanged)
  Q_PROPERTY(QString aiConversation READ aiConversation NOTIFY timelineChanged)

 public:
  explicit WorkbenchRuntime(QObject* parent = nullptr);
  [[nodiscard]] int playheadFrame() const;
  [[nodiscard]] bool playing() const { return playing_; }
  [[nodiscard]] int videoTrackCount() const;
  [[nodiscard]] QVariantList clips() const;
  [[nodiscard]] bool demoOverlayEnabled() const { return demoOverlayEnabled_; }
  [[nodiscard]] int demoOverlayX() const { return demoOverlayX_; }
  [[nodiscard]] int demoOverlayY() const { return demoOverlayY_; }
  [[nodiscard]] int demoOverlayWidth() const { return demoOverlayWidth_; }
  [[nodiscard]] int demoOverlayHeight() const { return demoOverlayHeight_; }
  [[nodiscard]] double demoOverlayScale() const { return demoOverlayScale_; }
  [[nodiscard]] double demoOverlayRotation() const { return demoOverlayRotation_; }
  [[nodiscard]] double demoOverlayOpacity() const { return demoOverlayOpacity_; }
  [[nodiscard]] QString demoOverlayText() const { return demoOverlayText_; }
  [[nodiscard]] bool installedPluginAvailable() const { return installedPlugin_.has_value(); }
  [[nodiscard]] QString installedPluginId() const;
  [[nodiscard]] QString componentPluginDependencyStatus() const;
  [[nodiscard]] bool pluginRenderBusy() const { return pluginRenderBusy_; }
  [[nodiscard]] bool pluginExportBusy() const { return pluginExportBusy_; }
  [[nodiscard]] bool timelineExportBusy() const { return timelineExportBusy_; }
  [[nodiscard]] bool authenticated() const { return sessions_.authenticated(); }
  [[nodiscard]] QString authenticatedUsername() const { return sessions_.username(); }
  [[nodiscard]] bool signInBusy() const { return signInBusy_; }
  [[nodiscard]] bool componentUploadBusy() const { return componentUploadBusy_; }
  [[nodiscard]] bool aiComponentDraftAvailable() const { return aiComponentDraft_.has_value(); }
  [[nodiscard]] QString aiComponentDraft() const { return aiComponentDraftJson_; }
  [[nodiscard]] bool aiRequestBusy() const { return aiRequestBusy_; }
  [[nodiscard]] QString aiConversation() const { return aiConversation_; }
  [[nodiscard]] QJsonObject componentJson() const;
  void setDemoOverlayX(int value);
  void setDemoOverlayY(int value);
  void setDemoOverlayWidth(int value);
  void setDemoOverlayHeight(int value);
  void setDemoOverlayScale(double value);
  void setDemoOverlayRotation(double value);
  void setDemoOverlayOpacity(double value);
  void setDemoOverlayText(const QString& value);
  [[nodiscard]] QImage previewFrame() const;
  Q_INVOKABLE bool importMedia(const QString& path);
  Q_INVOKABLE bool selectClip(qlonglong id);
  Q_INVOKABLE void toggleDemoOverlay();
  Q_INVOKABLE void generateComponentDraft();
  Q_INVOKABLE bool applyAiComponentCommand(const QString& json);
  Q_INVOKABLE bool requestAiComponentDraft(const QString& endpoint, const QString& apiKey,
                                           const QString& model, const QString& prompt);
  Q_INVOKABLE bool proposeAiComponentCommand(const QString& json);
  Q_INVOKABLE bool applyPendingAiComponentCommand();
  Q_INVOKABLE void discardPendingAiComponentCommand();
  Q_INVOKABLE bool loadComponentJson(const QString& json);
  Q_INVOKABLE bool loadComponentFile(const QString& path);
  Q_INVOKABLE bool saveComponentJson(const QString& path) const;
  Q_INVOKABLE bool saveComponentPackage(const QString& directory, const QString& resourceId,
                                        const QString& displayName);
  Q_INVOKABLE bool configureSilentComponentUploads(const QString& endpoint, const QString& statePath,
                                                    const QString& pendingRoot);
  Q_INVOKABLE bool signInWithSupabase(const QString& projectUrl, const QString& anonKey,
                                      const QString& email, const QString& password);
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
  Q_INVOKABLE void clearComponentOverlay();
  Q_INVOKABLE bool setPlayhead(int frame);
  Q_INVOKABLE bool saveProject(const QString& path) const;
  Q_INVOKABLE bool loadProject(const QString& path);
  Q_INVOKABLE void togglePlayback();
  Q_INVOKABLE bool splitSelected();
  Q_INVOKABLE bool deleteSelected();
  Q_INVOKABLE bool rippleDeleteSelected();
  Q_INVOKABLE bool moveSelected(qlonglong destination);
  Q_INVOKABLE bool trimSelectedLeft();
  Q_INVOKABLE bool trimSelectedRight();
  Q_INVOKABLE bool undoTimeline();
  Q_INVOKABLE bool redoTimeline();

 signals:
  void timelineChanged();
  void operationFailed(QString message);
  void operationSucceeded(QString message);

 private:
  bool exportInstalledPlugin(const QString& requestId, const QString& compositionId,
                             const QString& outputPath, bool applyToTimeline);
  void refreshDemoOverlay();
  void syncDemoOverlayProperties(const QJsonObject& component);
  edward::core::Timeline timeline_;
  edward::core::TrackId videoTrack_;
  TimelineController controller_;
  edward::media::MltAdapter mltAdapter_;
  edward::media::RenderGraph renderGraph_;
  bool demoOverlayEnabled_ = false;
  std::optional<edward::core::ComponentIr> demoOverlayIr_;
  int demoOverlayX_ = 24;
  int demoOverlayY_ = 24;
  int demoOverlayWidth_ = 220;
  int demoOverlayHeight_ = 72;
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
  bool timelineExportBusy_ = false;
  edward::resources::AuthSessionStore sessions_;
  edward::resources::SupabaseAuthClient authClient_;
  bool signInBusy_ = false;
  edward::resources::ComponentUploadClient componentUploadClient_;
  bool componentUploadBusy_ = false;
  std::optional<edward::core::ComponentIr> aiComponentDraft_;
  QString aiComponentDraftJson_;
  edward::resources::ModelChatClient modelChatClient_;
  bool aiRequestBusy_ = false;
  QString aiConversation_;
  QString pendingAiPrompt_;
  std::unique_ptr<edward::resources::ComponentUploadDispatcher> silentUploadDispatcher_;
  QString silentUploadEndpoint_;
  QTimer silentUploadRetryTimer_;
  QTimer playbackTimer_;
  bool playing_ = false;
  void dispatchSilentComponentUploads();
};

}  // namespace edward::desktop
