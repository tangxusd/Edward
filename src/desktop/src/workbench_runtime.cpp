#include "edward/desktop/workbench_runtime.hpp"

#include "edward/plugins/plugin_host.hpp"
#include "edward/resources/component_package.hpp"

#include <QVariantMap>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDateTime>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace edward::desktop {

namespace {
std::optional<edward::core::ComponentIr> demoOverlay(int x, int y, int width, int height, double opacity, const QString& label) {
  const QJsonObject box{{"id", "demo-box"}, {"type", "shape"},
                        {"transform", QJsonObject{{"x", x}, {"y", y}, {"width", width}, {"height", height}}},
                        {"properties", QJsonObject{{"fill", "#00b8c8"}, {"opacity", opacity}}},
                        {"keyframes", QJsonObject{{"x", QJsonArray{
                            QJsonObject{{"frame", 0}, {"value", x}},
                            QJsonObject{{"frame", 90}, {"value", x + 156}}
                        }}}}};
  const QJsonObject text{{"id", "demo-text"}, {"type", "text"},
                   {"transform", QJsonObject{{"x", x + 20}, {"y", y - 20}, {"width", 180}, {"height", 32}}},
                   {"properties", QJsonObject{{"text", label}, {"fontSize", 18}, {"color", "#ffffff"}}}};
  const QJsonObject root{{"id", "demo-root"}, {"type", "container"}, {"children", QJsonArray{box, text}}};
  return edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
}
}  // namespace

WorkbenchRuntime::WorkbenchRuntime(QObject* parent)
    : QObject(parent), timeline_(900), videoTrack_(timeline_.addVideoTrack()), controller_(timeline_, videoTrack_),
      renderGraph_(mltAdapter_) {
  silentUploadRetryTimer_.setInterval(3 * 60 * 1000);
  connect(&silentUploadRetryTimer_, &QTimer::timeout, this, &WorkbenchRuntime::dispatchSilentComponentUploads);
  connect(&sessions_, &edward::resources::AuthSessionStore::changed, this,
          &WorkbenchRuntime::timelineChanged);
  connect(&authClient_, &edward::resources::SupabaseAuthClient::completed, this,
          [this](bool success, const QString& message) {
            signInBusy_ = false;
            if (success)
              emit operationSucceeded(message);
            else
              emit operationFailed(message);
            if (success) dispatchSilentComponentUploads();
            emit timelineChanged();
          });
  connect(&componentUploadClient_, &edward::resources::ComponentUploadClient::completed, this,
          [this](bool success, const QString& message, const QJsonObject& response) {
            componentUploadBusy_ = false;
            if (!success) {
              emit operationFailed(message);
            } else {
              QString receiptError;
              const auto receipt = edward::resources::ComponentUploadClient::parseReceipt(response, &receiptError);
              if (receipt) {
                emit operationSucceeded(QStringLiteral("组件已上传，状态：%1，版本：%2")
                                            .arg(receipt->status)
                                            .arg(receipt->revision > 0 ? QString::number(receipt->revision)
                                                                        : QStringLiteral("未返回")));
              } else {
                emit operationSucceeded(message);
              }
            }
            emit timelineChanged();
          });
  connect(&pluginFrameWatcher_, &QFutureWatcher<PluginFrameResult>::finished, this, [this] {
    pluginRenderBusy_ = false;
    const auto result = pluginFrameWatcher_.result();
    if (!result.error.isEmpty()) {
      emit operationFailed(result.error);
    } else if (!result.frame.isNull()) {
      renderGraph_.setPluginFrame(result.frame);
    }
    emit timelineChanged();
  });
  connect(&pluginExportWatcher_, &QFutureWatcher<PluginExportResult>::finished, this, [this] {
    pluginExportBusy_ = false;
    const auto result = pluginExportWatcher_.result();
    if (!result.error.isEmpty()) emit operationFailed(result.error);
    emit timelineChanged();
  });
}

void WorkbenchRuntime::refreshDemoOverlay() {
  if (demoOverlayEnabled_) renderGraph_.setOverlay(demoOverlayIr_);
}

int WorkbenchRuntime::playheadFrame() const { return static_cast<int>(controller_.playheadFrame()); }
int WorkbenchRuntime::videoTrackCount() const {
  return static_cast<int>(timeline_.snapshot().videoTracks.size());
}

QString WorkbenchRuntime::installedPluginId() const {
  return installedPlugin_ ? installedPlugin_->manifest.pluginId : QString{};
}

QString WorkbenchRuntime::componentPluginDependencyStatus() const {
  if (!demoOverlayIr_) return QStringLiteral("无组件");
  switch (edward::plugins::dependencyStatus(*demoOverlayIr_, installedPlugin_)) {
    case edward::plugins::PluginDependencyStatus::NotRequired: return QStringLiteral("无插件依赖");
    case edward::plugins::PluginDependencyStatus::Available: return QStringLiteral("插件可用");
    case edward::plugins::PluginDependencyStatus::Missing: return QStringLiteral("缺少插件");
    case edward::plugins::PluginDependencyStatus::VersionMismatch: return QStringLiteral("插件版本不匹配");
  }
  return QStringLiteral("插件状态未知");
}

QJsonObject WorkbenchRuntime::componentJson() const {
  return demoOverlayIr_ ? demoOverlayIr_->toJson() : QJsonObject{};
}

QVariantList WorkbenchRuntime::clips() const {
  QVariantList result;
  const auto snapshot = timeline_.snapshot();
  for (std::size_t trackIndex = 0; trackIndex < snapshot.videoTracks.size(); ++trackIndex) {
    for (const auto& clip : timeline_.clips(snapshot.videoTracks[trackIndex])) {
      QVariantMap item;
      item.insert(QStringLiteral("id"), static_cast<qlonglong>(clip.id));
      item.insert(QStringLiteral("trackIndex"), static_cast<int>(trackIndex));
      item.insert(QStringLiteral("timelineStart"), static_cast<qlonglong>(clip.timelineStart));
      item.insert(QStringLiteral("sourceIn"), static_cast<qlonglong>(clip.sourceIn));
      item.insert(QStringLiteral("sourceOut"), static_cast<qlonglong>(clip.sourceOut));
      item.insert(QStringLiteral("name"), QString::fromStdString(clip.source.filename().string()));
      item.insert(QStringLiteral("selected"), clip.id == controller_.selectedClip());
      result.push_back(item);
    }
  }
  return result;
}

QImage WorkbenchRuntime::previewFrame() const {
  const auto scene = renderGraph_.build(timeline_.snapshot(), {controller_.playheadFrame()});
  return scene ? scene->frame : QImage{};
}

bool WorkbenchRuntime::importMedia(const QString& path) {
  if (!controller_.dropMediaAtPlayhead(path)) {
    emit operationFailed(QStringLiteral("素材无法添加：播放头位置存在冲突，或媒体不可读"));
    return false;
  }
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::selectClip(qlonglong id) {
  if (!controller_.selectClip(static_cast<edward::core::ClipId>(id))) return false;
  emit timelineChanged();
  return true;
}

void WorkbenchRuntime::toggleDemoOverlay() {
  demoOverlayEnabled_ = !demoOverlayEnabled_;
  if (demoOverlayEnabled_) demoOverlayIr_ = demoOverlay(demoOverlayX_, demoOverlayY_, demoOverlayWidth_, demoOverlayHeight_, demoOverlayOpacity_, demoOverlayText_);
  else demoOverlayIr_.reset();
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::generateComponentDraft() {
  demoOverlayIr_ = demoOverlay(demoOverlayX_, demoOverlayY_, demoOverlayWidth_, demoOverlayHeight_, demoOverlayOpacity_, demoOverlayText_);
  demoOverlayEnabled_ = true;
  refreshDemoOverlay();
  emit timelineChanged();
}

bool WorkbenchRuntime::loadComponentJson(const QString& json) {
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    emit operationFailed(QStringLiteral("组件 JSON 无效：%1").arg(parseError.errorString()));
    return false;
  }
  auto component = edward::core::ComponentIr::parse(document.object());
  if (!component) {
    emit operationFailed(QStringLiteral("组件 IR 校验失败"));
    return false;
  }
  demoOverlayIr_ = std::move(component);
  demoOverlayEnabled_ = true;
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::loadComponentFile(const QString& path) {
  if (path.isEmpty()) return false;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    emit operationFailed(QStringLiteral("组件文件无法读取"));
    return false;
  }
  return loadComponentJson(QString::fromUtf8(file.readAll()));
}

bool WorkbenchRuntime::saveComponentJson(const QString& path) const {
  if (path.isEmpty() || !demoOverlayIr_) return false;
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  const auto bytes = QJsonDocument(demoOverlayIr_->toJson()).toJson(QJsonDocument::Indented);
  return file.write(bytes) == bytes.size();
}

bool WorkbenchRuntime::saveComponentPackage(const QString& directory, const QString& resourceId,
                                            const QString& displayName) {
  if (directory.isEmpty() || !demoOverlayIr_) return false;
  edward::resources::ComponentPackage package{resourceId, displayName, *demoOverlayIr_, {}, {}, {}, {}};
  QString error;
  if (!package.saveLocal(directory.toStdString(), &error)) return false;
  if (silentUploadDispatcher_ && sessions_.authenticated() &&
      silentUploadDispatcher_->enqueue(directory, resourceId, QDateTime::currentDateTimeUtc(), &error)) {
    dispatchSilentComponentUploads();
  }
  return true;
}

bool WorkbenchRuntime::configureSilentComponentUploads(const QString& endpoint, const QString& statePath,
                                                       const QString& pendingRoot) {
  const QUrl url(endpoint);
  if (!url.isValid() || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0 ||
      url.host().isEmpty() || statePath.isEmpty() || pendingRoot.isEmpty()) {
    return false;
  }
  auto dispatcher = std::make_unique<edward::resources::ComponentUploadDispatcher>(statePath, pendingRoot, this);
  QString error;
  if (!dispatcher->restore(QDateTime::currentDateTimeUtc(), &error)) return false;
  connect(dispatcher.get(), &edward::resources::ComponentUploadDispatcher::finished, this,
          [this](bool, const QString&) { dispatchSilentComponentUploads(); });
  silentUploadEndpoint_ = endpoint;
  silentUploadDispatcher_ = std::move(dispatcher);
  silentUploadRetryTimer_.start();
  dispatchSilentComponentUploads();
  return true;
}

void WorkbenchRuntime::dispatchSilentComponentUploads() {
  if (!silentUploadDispatcher_ || !sessions_.authenticated() || silentUploadDispatcher_->busy()) return;
  QString error;
  silentUploadDispatcher_->dispatchNext(silentUploadEndpoint_, sessions_.session(),
                                        QDateTime::currentDateTimeUtc(), &error);
}

bool WorkbenchRuntime::signInWithSupabase(const QString& projectUrl, const QString& anonKey,
                                          const QString& email, const QString& password) {
  if (signInBusy_) return false;
  signInBusy_ = true;
  emit timelineChanged();
  if (!authClient_.signInWithPassword({projectUrl, anonKey}, email, password, &sessions_)) {
    signInBusy_ = false;
    emit timelineChanged();
    return false;
  }
  return true;
}

void WorkbenchRuntime::signOut() {
  sessions_.clear();
  emit operationSucceeded(QStringLiteral("已退出登录"));
}

bool WorkbenchRuntime::uploadCurrentComponent(const QString& endpoint, const QString& resourceId,
                                              const QString& displayName) {
  if (componentUploadBusy_ || !demoOverlayIr_ || !sessions_.authenticated()) {
    emit operationFailed(QStringLiteral("需要已登录的当前组件才能上传"));
    return false;
  }
  edward::resources::ComponentPackage package{resourceId, displayName, *demoOverlayIr_, {}, {}, {}, {}};
  if (const auto dependency = demoOverlayIr_->pluginDependency()) {
    package.pluginId = dependency->pluginId;
    package.pluginVersion = dependency->version;
  }
  componentUploadBusy_ = true;
  emit timelineChanged();
  if (!componentUploadClient_.submit(endpoint, package, sessions_.session())) {
    componentUploadBusy_ = false;
    emit timelineChanged();
    return false;
  }
  return true;
}

bool WorkbenchRuntime::loadPluginFrameJson(const QString& requestId, const QString& json) {
  if (requestId.isEmpty()) {
    emit operationFailed(QStringLiteral("插件帧请求 ID 不能为空"));
    return false;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    emit operationFailed(QStringLiteral("插件帧 JSON 无效：%1").arg(parseError.errorString()));
    return false;
  }
  QString rpcError;
  auto response = edward::plugins::RpcResponse::parse(document.object(), &rpcError);
  if (!response) {
    emit operationFailed(QStringLiteral("插件响应 JSON 无效：%1").arg(rpcError));
    return false;
  }
  const auto base = mltAdapter_.renderFrame(timeline_.snapshot(), controller_.playheadFrame());
  if (!base) {
    emit operationFailed(QStringLiteral("当前没有可用的预览画布"));
    return false;
  }
  QString error;
  const auto frame = edward::plugins::parseRenderFrameResponse(
      *response, requestId, controller_.playheadFrame(), base->size(), &error);
  if (!frame) {
    emit operationFailed(QStringLiteral("插件帧校验失败：%1").arg(error));
    return false;
  }
  renderGraph_.setPluginFrame(*frame);
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::selectInstalledPlugin(const QString& rootPath) {
  QString error;
  auto plugin = edward::plugins::loadInstalledPlugin(
      std::filesystem::path(rootPath.toStdString()), &error);
  if (!plugin) {
    emit operationFailed(QStringLiteral("插件不可用：%1").arg(error));
    return false;
  }
  installedPlugin_ = std::move(plugin);
  emit timelineChanged();
  return true;
}

void WorkbenchRuntime::clearInstalledPlugin() {
  if (!installedPlugin_) return;
  installedPlugin_.reset();
  renderGraph_.setPluginFrame(std::nullopt);
  emit timelineChanged();
}

bool WorkbenchRuntime::describeInstalledPlugin(const QString& compositionId) {
  if (!installedPlugin_ || compositionId.isEmpty()) {
    emit operationFailed(QStringLiteral("插件或组件 ID 不可用"));
    return false;
  }
  QString error;
  const auto component = edward::plugins::describePlugin(
      installedPlugin_->manifest, installedPlugin_->root,
      QStringLiteral("describe-%1").arg(QDateTime::currentMSecsSinceEpoch()), compositionId, 5000, &error);
  if (!component) {
    emit operationFailed(QStringLiteral("插件组件描述失败：%1").arg(error));
    return false;
  }
  auto componentJson = component->toJson();
  componentJson.insert("pluginDependency", QJsonObject{{"pluginId", installedPlugin_->manifest.pluginId},
                                                        {"version", installedPlugin_->manifest.version}});
  auto dependentComponent = edward::core::ComponentIr::parse(componentJson);
  if (!dependentComponent) {
    emit operationFailed(QStringLiteral("插件组件依赖信息无效"));
    return false;
  }
  demoOverlayIr_ = std::move(*dependentComponent);
  demoOverlayEnabled_ = true;
  refreshDemoOverlay();
  emit operationSucceeded(QStringLiteral("插件组件已载入，可继续调整属性和关键帧"));
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::renderInstalledPluginFrame(const QString& requestId, const QString& compositionId) {
  if (pluginRenderBusy_) {
    emit operationFailed(QStringLiteral("插件预览正在渲染"));
    return false;
  }
  if (!installedPlugin_ || requestId.isEmpty() || compositionId.isEmpty()) {
    emit operationFailed(QStringLiteral("插件或渲染请求参数不可用"));
    return false;
  }
  const auto base = mltAdapter_.renderFrame(timeline_.snapshot(), controller_.playheadFrame());
  if (!base) {
    emit operationFailed(QStringLiteral("当前没有可用的预览画布"));
    return false;
  }
  const auto plugin = *installedPlugin_;
  const int frame = controller_.playheadFrame();
  const auto size = base->size();
  pluginRenderBusy_ = true;
  emit timelineChanged();
  pluginFrameWatcher_.setFuture(QtConcurrent::run([plugin, requestId, compositionId, frame, size] {
    PluginFrameResult result;
    result.frame = edward::plugins::renderPluginFrame(plugin.manifest, plugin.root, requestId,
                                                      compositionId, frame, size, 5000, &result.error)
                       .value_or(QImage{});
    return result;
  }));
  return true;
}

bool WorkbenchRuntime::exportInstalledPlugin(const QString& requestId, const QString& compositionId,
                                             const QString& outputPath) {
  if (pluginExportBusy_ || pluginRenderBusy_) {
    emit operationFailed(QStringLiteral("插件任务正在执行"));
    return false;
  }
  if (!installedPlugin_ || requestId.isEmpty() || compositionId.isEmpty() || outputPath.isEmpty()) {
    emit operationFailed(QStringLiteral("插件或导出参数不可用"));
    return false;
  }
  const auto base = mltAdapter_.renderFrame(timeline_.snapshot(), controller_.playheadFrame());
  if (!base) {
    emit operationFailed(QStringLiteral("当前没有可用的预览画布"));
    return false;
  }
  const auto plugin = *installedPlugin_;
  const auto size = base->size();
  const auto selectedPath = std::filesystem::path(outputPath.toStdString());
  const auto outputRoot = selectedPath.parent_path();
  const auto outputName = QString::fromStdString(selectedPath.filename().string());
  if (outputRoot.empty() || outputName.isEmpty()) {
    emit operationFailed(QStringLiteral("插件导出路径不可用"));
    return false;
  }
  pluginExportBusy_ = true;
  emit timelineChanged();
  pluginExportWatcher_.setFuture(QtConcurrent::run([plugin, requestId, compositionId, outputRoot, outputName, size] {
    PluginExportResult result;
    QString error;
    if (!edward::plugins::exportPlugin(plugin.manifest, plugin.root, outputRoot, requestId, compositionId,
                                       outputName, size, 30000, &error)) {
      result.error = QStringLiteral("插件导出失败：%1").arg(error);
    }
    return result;
  }));
  return true;
}

void WorkbenchRuntime::clearComponentOverlay() {
  demoOverlayIr_.reset();
  demoOverlayEnabled_ = false;
  renderGraph_.setOverlay(std::nullopt);
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayX(int value) {
  const int clamped = std::max(-640, std::min(value, 640));
  if (demoOverlayX_ == clamped) return;
  demoOverlayX_ = clamped;
  if (demoOverlayIr_) {
    demoOverlayIr_->setNodeTransformNumber("demo-box", "x", demoOverlayX_);
    demoOverlayIr_->setNodeTransformNumber("demo-text", "x", demoOverlayX_ + 20);
    demoOverlayIr_->setNodeKeyframeNumber("demo-box", "x", playheadFrame(), demoOverlayX_);
    demoOverlayIr_->setNodeKeyframeNumber("demo-text", "x", playheadFrame(), demoOverlayX_ + 20);
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayY(int value) {
  const int clamped = std::max(-360, std::min(value, 360));
  if (demoOverlayY_ == clamped) return;
  demoOverlayY_ = clamped;
  if (demoOverlayIr_) {
    demoOverlayIr_->setNodeTransformNumber("demo-box", "y", demoOverlayY_);
    demoOverlayIr_->setNodeTransformNumber("demo-text", "y", demoOverlayY_ - 20);
    demoOverlayIr_->setNodeKeyframeNumber("demo-box", "y", playheadFrame(), demoOverlayY_);
    demoOverlayIr_->setNodeKeyframeNumber("demo-text", "y", playheadFrame(), demoOverlayY_ - 20);
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayWidth(int value) {
  demoOverlayWidth_ = std::max(40, std::min(value, 640));
  if (demoOverlayIr_) {
    demoOverlayIr_->setNodeTransformNumber("demo-box", "width", demoOverlayWidth_);
    demoOverlayIr_->setNodeKeyframeNumber("demo-box", "width", playheadFrame(), demoOverlayWidth_);
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayHeight(int value) {
  demoOverlayHeight_ = std::max(24, std::min(value, 360));
  if (demoOverlayIr_) {
    demoOverlayIr_->setNodeTransformNumber("demo-box", "height", demoOverlayHeight_);
    demoOverlayIr_->setNodeKeyframeNumber("demo-box", "height", playheadFrame(), demoOverlayHeight_);
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayScale(double value) {
  demoOverlayScale_ = std::max(0.1, std::min(value, 3.0));
  if (demoOverlayIr_) {
    demoOverlayIr_->setNodeTransformNumber("demo-box", "scaleX", demoOverlayScale_);
    demoOverlayIr_->setNodeTransformNumber("demo-box", "scaleY", demoOverlayScale_);
    demoOverlayIr_->setNodeKeyframeNumber("demo-box", "scaleX", playheadFrame(), demoOverlayScale_);
    demoOverlayIr_->setNodeKeyframeNumber("demo-box", "scaleY", playheadFrame(), demoOverlayScale_);
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayRotation(double value) {
  demoOverlayRotation_ = std::max(-180.0, std::min(value, 180.0));
  if (demoOverlayIr_) {
    demoOverlayIr_->setNodeTransformNumber("demo-box", "rotation", demoOverlayRotation_);
    demoOverlayIr_->setNodeKeyframeNumber("demo-box", "rotation", playheadFrame(), demoOverlayRotation_);
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayOpacity(double value) {
  demoOverlayOpacity_ = std::max(0.0, std::min(value, 1.0));
  if (demoOverlayIr_) {
    demoOverlayIr_->setNodeProperty("demo-box", "opacity", demoOverlayOpacity_);
    demoOverlayIr_->setNodeKeyframeNumber("demo-box", "opacity", playheadFrame(), demoOverlayOpacity_);
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayText(const QString& value) {
  demoOverlayText_ = value.left(120);
  if (demoOverlayIr_) demoOverlayIr_->setNodeProperty("demo-text", "text", demoOverlayText_);
  refreshDemoOverlay();
  emit timelineChanged();
}

bool WorkbenchRuntime::setPlayhead(int frame) {
  if (!controller_.setPlayhead(frame)) return false;
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::splitSelected() {
  if (!controller_.splitSelectedAtPlayhead()) {
    emit operationFailed(QStringLiteral("播放头不在选中片段内部"));
    return false;
  }
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::deleteSelected() {
  if (!controller_.deleteSelected()) {
    emit operationFailed(QStringLiteral("没有可删除的片段"));
    return false;
  }
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::rippleDeleteSelected() { return deleteSelected(); }

}  // namespace edward::desktop
