#include "edward/desktop/workbench_runtime.hpp"

#include "edward/media/export_job.hpp"
#include "edward/media/audio_waveform.hpp"
#include "edward/plugins/plugin_host.hpp"
#include "edward/resources/component_package.hpp"
#include "edward/core/component_edit_command.hpp"
#include "edward/core/component_edit_command_parser.hpp"

#include <QVariantMap>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QUrl>
#include <QDateTime>
#include <QRegularExpression>
#include <QPainter>
#include <QPointer>
#include <QStandardPaths>
#include <QSettings>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace edward::desktop {

namespace {
QString previewSettingsPath() {
  const auto overridePath = qEnvironmentVariable("EDWARD_SETTINGS_PATH");
  if (!overridePath.isEmpty()) return overridePath;
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
         QStringLiteral("/preview-storage.ini");
}

QByteArray previewCacheKey(const edward::core::TimelineSnapshot& snapshot,
                           edward::core::Frame frame,
                           int quality,
                           const QByteArray& renderSignature) {
  QJsonArray tracks;
  for (const auto track : snapshot.videoTracks) tracks.append(track);
  QJsonArray clips;
  for (const auto& clip : snapshot.clips) {
    QJsonObject entry{{QStringLiteral("id"), clip.id},
                      {QStringLiteral("track"), clip.trackId},
                      {QStringLiteral("source"), QString::fromStdString(clip.source.generic_string())},
                      {QStringLiteral("sourceIn"), clip.sourceIn},
                      {QStringLiteral("sourceOut"), clip.sourceOut},
                      {QStringLiteral("timelineStart"), clip.timelineStart},
                      {QStringLiteral("kind"), static_cast<int>(clip.kind)}};
    if (clip.component) entry.insert(QStringLiteral("component"), clip.component->toJson());
    clips.append(entry);
  }
  QJsonArray transitions;
  for (const auto& transition : snapshot.transitions) {
    transitions.append(QJsonObject{{QStringLiteral("type"), static_cast<int>(transition.type)},
                                   {QStringLiteral("left"), transition.leftClipId},
                                   {QStringLiteral("right"), transition.rightClipId},
                                   {QStringLiteral("start"), transition.startFrame},
                                   {QStringLiteral("duration"), transition.durationFrames}});
  }
  const QJsonObject key{{QStringLiteral("duration"), snapshot.durationFrames},
                        {QStringLiteral("frame"), frame},
                        {QStringLiteral("quality"), quality},
                        {QStringLiteral("tracks"), tracks},
                        {QStringLiteral("clips"), clips},
                        {QStringLiteral("transitions"), transitions},
                        {QStringLiteral("render"), QString::fromLatin1(renderSignature.toHex())}};
  return QCryptographicHash::hash(QJsonDocument(key).toJson(QJsonDocument::Compact),
                                  QCryptographicHash::Sha256);
}

edward::media::RenderStorageRoots defaultPreviewStorageRoots() {
  const auto defaultRoot = std::filesystem::path(
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()) /
                           "preview";
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  const auto pathFor = [&settings](const QString& key, const std::filesystem::path& fallback) {
    return std::filesystem::path(settings.value(key, QString::fromStdString(fallback.string())).toString().toStdString());
  };
  return {pathFor(QStringLiteral("preview/proxyRoot"), defaultRoot / "proxies"),
          pathFor(QStringLiteral("preview/cacheRoot"), defaultRoot / "cache"),
          pathFor(QStringLiteral("preview/renderRoot"), defaultRoot / "renders")};
}

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

std::optional<QJsonObject> findNode(const QJsonObject& node, const QString& id) {
  if (node.value("id").toString() == id) return node;
  for (const auto& child : node.value("children").toArray()) {
    if (!child.isObject()) continue;
    if (const auto result = findNode(child.toObject(), id)) return result;
  }
  return std::nullopt;
}

void appendComponentNodes(const QJsonObject& node, QVariantList& result) {
  const auto id = node.value("id").toString();
  const auto type = node.value("type").toString();
  if (!id.isEmpty()) {
    QVariantMap item;
    item.insert(QStringLiteral("id"), id);
    item.insert(QStringLiteral("type"), type);
    item.insert(QStringLiteral("displayName"), type.isEmpty() ? id : QStringLiteral("%1 (%2)").arg(id, type));
    const auto transform = node.value("transform").toObject();
    item.insert(QStringLiteral("x"), transform.value("x").toDouble());
    item.insert(QStringLiteral("y"), transform.value("y").toDouble());
    item.insert(QStringLiteral("width"), transform.value("width").toDouble());
    item.insert(QStringLiteral("height"), transform.value("height").toDouble());
    result.push_back(item);
  }
  for (const auto& child : node.value("children").toArray()) {
    if (child.isObject()) appendComponentNodes(child.toObject(), result);
  }
}

QString editableTextNodeId(const QJsonObject& component, const QString& selectedNodeId) {
  const auto selected = findNode(component.value("root").toObject(), selectedNodeId);
  if (selected && selected->value("type").toString() == QStringLiteral("text")) return selectedNodeId;
  return QStringLiteral("demo-text");
}

QString editableShapeNodeId(const QJsonObject& component, const QString& selectedNodeId) {
  const auto selected = findNode(component.value("root").toObject(), selectedNodeId);
  if (selected && selected->value("type").toString() == QStringLiteral("shape")) return selectedNodeId;
  return QStringLiteral("demo-box");
}

QImage thumbnailSprite(const std::filesystem::path& source, edward::core::Frame sourceIn,
                        edward::core::Frame sourceOut) {
  constexpr int thumbnailCount = 4;
  constexpr QSize thumbnailSize{128, 72};
  if (source.empty() || sourceOut <= sourceIn) return {};
  edward::media::MltAdapter adapter;
  QImage result(thumbnailSize.width() * thumbnailCount, thumbnailSize.height(), QImage::Format_RGBA8888);
  result.fill(Qt::black);
  QPainter painter(&result);
  bool rendered = false;
  for (int index = 0; index < thumbnailCount; ++index) {
    const auto sourceFrame = sourceIn + (sourceOut - sourceIn - 1) * (2 * index + 1) /
                                         (2 * thumbnailCount);
    const auto frame = adapter.renderSourceFrame(source, sourceFrame);
    if (!frame || frame->isNull()) continue;
    const auto scaled = frame->scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawImage(index * thumbnailSize.width() + (thumbnailSize.width() - scaled.width()) / 2,
                      (thumbnailSize.height() - scaled.height()) / 2, scaled);
    rendered = true;
  }
  return rendered ? result : QImage{};
}

void setTransformAndKeyframe(edward::core::ComponentIr& component, const QString& nodeId,
                             const QString& field, double value, int frame) {
  edward::core::ComponentEditCommand::apply(
      component, {edward::core::ComponentEditKind::SetTransformNumber, nodeId, field, 0, value});
  edward::core::ComponentEditCommand::apply(
      component, {edward::core::ComponentEditKind::SetKeyframeValue, nodeId, field, frame, value});
}

void setPropertyAndKeyframe(edward::core::ComponentIr& component, const QString& nodeId,
                            const QString& field, const QJsonValue& value, int frame) {
  edward::core::ComponentEditCommand::apply(
      component, {edward::core::ComponentEditKind::SetProperty, nodeId, field, 0, value});
  edward::core::ComponentEditCommand::apply(
      component, {edward::core::ComponentEditKind::SetKeyframeValue, nodeId, field, frame, value});
}

bool pluginAllowsComponentEdit(const edward::core::ComponentIr& component,
                               const std::optional<edward::plugins::InstalledPlugin>& plugin,
                               const edward::core::ComponentEditCommand& command) {
  const auto dependency = component.pluginDependency();
  if (!dependency) return true;
  return plugin && plugin->manifest.pluginId == dependency->pluginId &&
         plugin->manifest.version == dependency->version &&
         plugin->manifest.editableProps.contains(command.field);
}
}  // namespace

WorkbenchRuntime::WorkbenchRuntime(QObject* parent)
    : QObject(parent), timeline_(900), videoTrack_(timeline_.addVideoTrack()), controller_(timeline_, videoTrack_),
      previewStorageRoots_(defaultPreviewStorageRoots()),
      previewSession_(previewStorageRoots_, projectIdentity_),
      previewFrameCache_(previewStorageRoots_, projectIdentity_),
      renderGraph_(mltAdapter_),
      resolveConnection_(),
      resolveAdapter_(resolveConnection_) {
  silentUploadRetryTimer_.setInterval(3 * 60 * 1000);
  connect(&silentUploadRetryTimer_, &QTimer::timeout, this, &WorkbenchRuntime::dispatchSilentComponentUploads);
  playbackTimer_.setInterval(40);
  connect(&playbackTimer_, &QTimer::timeout, this, [this] {
    audioPreview_.pump();
    if (!controller_.advancePlayhead()) {
      playing_ = false;
      playbackTimer_.stop();
      audioPreview_.stop();
      emit timelineChanged();
      return;
    }
    emit timelineChanged();
  });
  projectAutosaveTimer_.setSingleShot(true);
  projectAutosaveTimer_.setInterval(1000);
  connect(&projectAutosaveTimer_, &QTimer::timeout, this, &WorkbenchRuntime::saveProjectRecovery);
  connect(&previewProxyWatcher_, &QFutureWatcher<bool>::finished, this, [this] {
    previewProxyBusy_ = false;
    emit timelineChanged();
  });
  connect(this, &WorkbenchRuntime::timelineChanged, this, &WorkbenchRuntime::scheduleProjectAutosave);
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
  connect(&modelChatClient_, &edward::resources::ModelChatClient::completed, this,
          [this](bool success, const QString& result) {
            aiRequestBusy_ = false;
            if (!success) {
              pendingAiPrompt_.clear();
              emit operationFailed(QStringLiteral("AI 请求失败：%1").arg(result));
            } else if (!proposeAiComponentCommand(result)) {
              pendingAiPrompt_.clear();
              return;
            } else {
              if (!pendingAiPrompt_.isEmpty()) {
                if (!aiConversation_.isEmpty()) aiConversation_ += QLatin1Char('\n');
                aiConversation_ += QStringLiteral("用户：") + pendingAiPrompt_ +
                                   QStringLiteral("\nAI：") + result;
              }
              pendingAiPrompt_.clear();
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
    if (!result.error.isEmpty()) {
      emit operationFailed(result.error);
    } else if (result.applyToTimeline) {
      const auto info = edward::media::MediaProbe::probe(result.outputPath.toStdString());
      if (!info || !info->hasAlpha) {
        emit operationFailed(QStringLiteral("插件导出结果不包含透明通道，未应用到时间线"));
      } else if (!controller_.dropMediaAtPlayhead(result.outputPath)) {
        emit operationFailed(QStringLiteral("插件动画无法加入当前播放头位置，可能超出时长或轨道冲突"));
      } else {
        demoOverlayIr_.reset();
        demoOverlayEnabled_ = false;
        renderGraph_.setOverlay(std::nullopt);
        renderGraph_.setPluginFrame(std::nullopt);
        emit operationSucceeded(QStringLiteral("插件动画已应用到时间线"));
      }
    } else {
      emit operationSucceeded(QStringLiteral("插件视频已导出：%1").arg(result.outputPath));
    }
    emit timelineChanged();
  });
  connect(&timelineExportWatcher_, &QFutureWatcher<TimelineExportResult>::finished, this, [this] {
    timelineExportBusy_ = false;
    timelineExportCancel_.reset();
    const auto result = timelineExportWatcher_.result();
    if (result.error.isEmpty()) {
      timelineExportProgress_ = 100;
      emit operationSucceeded(QStringLiteral("视频已导出：%1").arg(result.outputPath));
    } else {
      timelineExportProgress_ = 0;
      emit operationFailed(result.error);
    }
    emit timelineChanged();
  });
}

void WorkbenchRuntime::refreshDemoOverlay() {
  const auto selected = timeline_.clip(controller_.selectedClip());
  if (selected && selected->kind == edward::core::TimelineClipKind::Component && demoOverlayIr_) {
    auto replacement = *selected;
    replacement.component = *demoOverlayIr_;
    timeline_.replaceClip(replacement.id, std::move(replacement));
    renderGraph_.setOverlay(std::nullopt);
    renderGraph_.setComponentLayers({});
    return;
  }
  if (!demoOverlayEnabled_ || !demoOverlayIr_) {
    renderGraph_.setOverlay(std::nullopt);
    renderGraph_.setComponentLayers({});
    return;
  }
  if (componentClipId_ != 0) {
    const auto clip = timeline_.clip(componentClipId_);
    if (clip) {
      renderGraph_.setOverlay(std::nullopt);
      renderGraph_.setComponentLayers({{clip->timelineStart,
                                         clip->timelineStart + clip->sourceOut - clip->sourceIn,
                                         clip->sourceIn,
                                         *demoOverlayIr_}});
      return;
    }
    componentClipId_ = 0;
  }
  renderGraph_.setComponentLayers({});
  renderGraph_.setOverlay(demoOverlayIr_);
}

void WorkbenchRuntime::syncDemoOverlayProperties(const QJsonObject& component) {
  const auto box = findNode(component.value("root").toObject(), QStringLiteral("demo-box"));
  if (!box) return;
  const auto transform = box->value("transform").toObject();
  const auto properties = box->value("properties").toObject();
  if (transform.value("x").isDouble()) demoOverlayX_ = transform.value("x").toInt();
  if (transform.value("y").isDouble()) demoOverlayY_ = transform.value("y").toInt();
  if (transform.value("width").isDouble()) demoOverlayWidth_ = transform.value("width").toInt();
  if (transform.value("height").isDouble()) demoOverlayHeight_ = transform.value("height").toInt();
  if (transform.value("scaleX").isDouble()) demoOverlayScale_ = transform.value("scaleX").toDouble();
  if (transform.value("rotation").isDouble()) demoOverlayRotation_ = transform.value("rotation").toDouble();
  if (properties.value("opacity").isDouble()) demoOverlayOpacity_ = properties.value("opacity").toDouble();
  if (properties.value("borderWidth").isDouble()) demoOverlayBorderWidth_ = properties.value("borderWidth").toInt();
  const auto text = findNode(component.value("root").toObject(), QStringLiteral("demo-text"));
  if (text) {
    const auto textProperties = text->value("properties").toObject();
    if (textProperties.value("text").isString()) demoOverlayText_ = textProperties.value("text").toString();
    if (textProperties.value("fontSize").isDouble()) demoOverlayFontSize_ = textProperties.value("fontSize").toInt();
  }
}

int WorkbenchRuntime::playheadFrame() const { return static_cast<int>(controller_.playheadFrame()); }

int WorkbenchRuntime::timelineDurationFrames() const {
  return std::max<edward::core::Frame>(1, timeline_.snapshot().durationFrames);
}

bool WorkbenchRuntime::componentPlayheadIsEditable() const {
  const auto clipId = editingComponentClipId_ != 0 ? editingComponentClipId_ : componentClipId_;
  if (clipId != 0) {
    if (const auto clip = timeline_.clip(clipId)) {
      const auto start = static_cast<int>(clip->timelineStart);
      const auto end = start + static_cast<int>(clip->sourceOut - clip->sourceIn);
      return playheadFrame() >= start && playheadFrame() < end;
    }
  }
  return true;
}

int WorkbenchRuntime::componentKeyframeFrame() const {
  const auto clipId = editingComponentClipId_ != 0 ? editingComponentClipId_ : componentClipId_;
  if (clipId != 0) {
    if (const auto clip = timeline_.clip(clipId))
      return playheadFrame() - static_cast<int>(clip->timelineStart);
  }
  return playheadFrame();
}
int WorkbenchRuntime::videoTrackCount() const {
  return static_cast<int>(timeline_.snapshot().videoTracks.size());
}

int WorkbenchRuntime::selectedVideoTrackIndex() const {
  const auto tracks = timeline_.snapshot().videoTracks;
  const auto it = std::ranges::find(tracks, controller_.targetTrack());
  return it == tracks.end() ? 0 : static_cast<int>(std::distance(tracks.begin(), it));
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

int WorkbenchRuntime::selectedComponentNodeX() const {
  if (!demoOverlayIr_) return 0;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  return node ? node->value("transform").toObject().value("x").toInt() : 0;
}

QString WorkbenchRuntime::selectedComponentNodeType() const {
  if (!demoOverlayIr_) return {};
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  return node ? node->value("type").toString() : QString{};
}

QVariantList WorkbenchRuntime::selectedComponentNodeKeyframes() const {
  QVariantList result;
  if (!demoOverlayIr_) return result;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node) return result;
  const auto keyframes = node->value("keyframes").toObject();
  for (auto it = keyframes.begin(); it != keyframes.end(); ++it) {
    for (const auto& frameValue : it.value().toArray()) {
      const auto frame = frameValue.toObject();
      if (!frame.value("frame").isDouble()) continue;
      QVariantMap item;
      item.insert(QStringLiteral("field"), it.key());
      item.insert(QStringLiteral("frame"), frame.value("frame").toInt());
      item.insert(QStringLiteral("value"), frame.value("value").toVariant());
      item.insert(QStringLiteral("easing"), frame.value("easing").toString("linear"));
      result.push_back(item);
    }
  }
  std::sort(result.begin(), result.end(), [](const QVariant& left, const QVariant& right) {
    return left.toMap().value(QStringLiteral("frame")).toInt() < right.toMap().value(QStringLiteral("frame")).toInt();
  });
  return result;
}

int WorkbenchRuntime::selectedComponentNodeY() const {
  if (!demoOverlayIr_) return 0;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  return node ? node->value("transform").toObject().value("y").toInt() : 0;
}

int WorkbenchRuntime::selectedComponentNodeWidth() const {
  if (!demoOverlayIr_) return 0;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  return node ? node->value("transform").toObject().value("width").toInt() : 0;
}

int WorkbenchRuntime::selectedComponentNodeHeight() const {
  if (!demoOverlayIr_) return 0;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  return node ? node->value("transform").toObject().value("height").toInt() : 0;
}

double WorkbenchRuntime::selectedComponentNodeRotation() const {
  if (!demoOverlayIr_) return 0.0;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  return node ? node->value("transform").toObject().value("rotation").toDouble() : 0.0;
}

double WorkbenchRuntime::selectedComponentNodeOpacity() const {
  if (!demoOverlayIr_) return 0.0;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  return node ? node->value("properties").toObject().value("opacity").toDouble(1.0) : 0.0;
}

QString WorkbenchRuntime::selectedComponentNodeColor() const {
  if (!demoOverlayIr_) return {};
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node) return {};
  const auto properties = node->value("properties").toObject();
  const auto type = node->value("type").toString();
  return properties.value(type == QStringLiteral("shape") ? "fill" : "color").toString();
}

QString WorkbenchRuntime::selectedComponentNodeBorderColor() const {
  if (!demoOverlayIr_) return {};
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node || node->value("type").toString() != QStringLiteral("shape")) return {};
  return node->value("properties").toObject().value("borderColor").toString();
}

QString WorkbenchRuntime::selectedComponentNodeFontFamily() const {
  if (!demoOverlayIr_) return {};
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node || node->value("type").toString() != QStringLiteral("text")) return {};
  return node->value("properties").toObject().value("fontFamily").toString();
}

QJsonObject WorkbenchRuntime::componentJson() const {
  return demoOverlayIr_ ? demoOverlayIr_->toJson() : QJsonObject{};
}

QVariantList WorkbenchRuntime::componentNodes() const {
  QVariantList result;
  if (!demoOverlayIr_) return result;
  appendComponentNodes(demoOverlayIr_->toJson().value("root").toObject(), result);
  return result;
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
      item.insert(QStringLiteral("kind"), clip.kind == edward::core::TimelineClipKind::Component
                                         ? QStringLiteral("component") : QStringLiteral("media"));
      if (clip.kind == edward::core::TimelineClipKind::Component)
        item.insert(QStringLiteral("name"), QStringLiteral("组件"));
      item.insert(QStringLiteral("selected"), clip.id == controller_.selectedClip());
      item.insert(QStringLiteral("waveform"), clipWaveforms_.value(static_cast<qint64>(clip.id)));
      item.insert(QStringLiteral("hasAudio"), clipWaveforms_.contains(static_cast<qint64>(clip.id)));
      if (clipThumbnails_.contains(static_cast<qint64>(clip.id)))
        item.insert(QStringLiteral("thumbnail"), QStringLiteral("image://edward/clip-%1").arg(clip.id));
      result.push_back(item);
    }
  }
  return result;
}

QVariantList WorkbenchRuntime::transitions() const {
  QVariantList result;
  const auto snapshot = timeline_.snapshot();
  for (const auto& transition : snapshot.transitions) {
    QVariantMap item;
    item.insert(QStringLiteral("type"), transition.type == edward::core::TransitionType::FlashBlack
                                            ? QStringLiteral("flash_black")
                                            : transition.type == edward::core::TransitionType::FlashWhite
                                                  ? QStringLiteral("flash_white")
                                                  : QStringLiteral("dissolve"));
    item.insert(QStringLiteral("startFrame"), static_cast<qlonglong>(transition.startFrame));
    item.insert(QStringLiteral("durationFrames"), static_cast<qlonglong>(transition.durationFrames));
    item.insert(QStringLiteral("leftClipId"), static_cast<qlonglong>(transition.leftClipId));
    item.insert(QStringLiteral("rightClipId"), static_cast<qlonglong>(transition.rightClipId));
    const auto leftClip = std::ranges::find_if(snapshot.clips, [&](const auto& clip) {
      return clip.id == transition.leftClipId;
    });
    if (leftClip == snapshot.clips.end()) continue;
    const auto track = std::ranges::find(snapshot.videoTracks, leftClip->trackId);
    if (track == snapshot.videoTracks.end()) continue;
    item.insert(QStringLiteral("trackIndex"), static_cast<int>(std::distance(snapshot.videoTracks.begin(), track)));
    result.push_back(item);
  }
  return result;
}

QImage WorkbenchRuntime::clipThumbnail(qlonglong id) const {
  return clipThumbnails_.value(id);
}

QImage WorkbenchRuntime::previewFrame() const {
  const auto snapshot = previewSnapshot();
  const auto frame = controller_.playheadFrame();
  const auto key = previewCacheKey(snapshot, frame, previewQuality(), renderGraph_.cacheSignature());
  if (const auto cached = previewFrameCache_.load(key)) return *cached;
  const auto scene = renderGraph_.build(snapshot, {frame});
  if (scene) {
    const auto stored = previewFrameCache_.store(key, scene->frame);
    Q_UNUSED(stored);
  }
  return scene ? scene->frame : QImage{};
}

int WorkbenchRuntime::previewQuality() const {
  return static_cast<int>(previewSession_.quality());
}

bool WorkbenchRuntime::previewProxyReady() const {
  if (previewSession_.quality() == edward::media::PreviewQuality::Original) return false;
  const auto snapshot = timeline_.snapshot();
  bool containsMedia = false;
  for (const auto& clip : snapshot.clips) {
    if (clip.kind != edward::core::TimelineClipKind::Media) continue;
    containsMedia = true;
    if (previewSession_.sourceFor(clip.source) == clip.source) return false;
  }
  return containsMedia;
}

QString WorkbenchRuntime::proxyStorageRoot() const {
  return QString::fromStdString(previewStorageRoots_.proxyRoot.string());
}

QString WorkbenchRuntime::cacheStorageRoot() const {
  return QString::fromStdString(previewStorageRoots_.cacheRoot.string());
}

QString WorkbenchRuntime::renderStorageRoot() const {
  return QString::fromStdString(previewStorageRoots_.renderRoot.string());
}

bool WorkbenchRuntime::setPreviewQuality(int quality) {
  if (quality < static_cast<int>(edward::media::PreviewQuality::Original) ||
      quality > static_cast<int>(edward::media::PreviewQuality::Fluent)) return false;
  const auto selected = static_cast<edward::media::PreviewQuality>(quality);
  if (selected == previewSession_.quality()) return true;
  previewSession_.setQuality(selected);
  if (selected == edward::media::PreviewQuality::Original) {
    emit timelineChanged();
    return true;
  }
  const auto snapshot = timeline_.snapshot();
  const auto roots = previewStorageRoots_;
  const auto project = projectIdentity_;
  previewProxyBusy_ = true;
  previewProxyWatcher_.setFuture(QtConcurrent::run([snapshot, roots, project, selected] {
    edward::media::PreviewSession session(roots, project);
    session.setQuality(selected);
    bool prepared = true;
    for (const auto& clip : snapshot.clips) {
      if (clip.kind == edward::core::TimelineClipKind::Media) prepared = session.prepare(clip.source) && prepared;
    }
    return prepared;
  }));
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::configurePreviewStorageRoots(const QString& proxyRoot, const QString& cacheRoot,
                                                     const QString& renderRoot) {
  if (previewProxyBusy_) return false;
  const edward::media::RenderStorageRoots roots{
      std::filesystem::path(proxyRoot.toStdString()), std::filesystem::path(cacheRoot.toStdString()),
      std::filesystem::path(renderRoot.toStdString())};
  if (roots.proxyRoot.empty() || roots.cacheRoot.empty() || roots.renderRoot.empty() ||
      !roots.proxyRoot.is_absolute() || !roots.cacheRoot.is_absolute() || !roots.renderRoot.is_absolute() ||
      roots.proxyRoot == roots.cacheRoot || roots.proxyRoot == roots.renderRoot ||
      roots.cacheRoot == roots.renderRoot) return false;
  std::error_code error;
  std::filesystem::create_directories(roots.proxyRoot, error);
  if (error) return false;
  std::filesystem::create_directories(roots.cacheRoot, error);
  if (error) return false;
  std::filesystem::create_directories(roots.renderRoot, error);
  if (error) return false;
  previewStorageRoots_ = roots;
  previewSession_ = edward::media::PreviewSession(previewStorageRoots_, projectIdentity_);
  previewFrameCache_ = edward::media::PreviewFrameCache(previewStorageRoots_, projectIdentity_);
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  settings.setValue(QStringLiteral("preview/proxyRoot"), proxyStorageRoot());
  settings.setValue(QStringLiteral("preview/cacheRoot"), cacheStorageRoot());
  settings.setValue(QStringLiteral("preview/renderRoot"), renderStorageRoot());
  settings.sync();
  if (settings.status() != QSettings::NoError) return false;
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::clearDerivedStorage(int kind) {
  if (kind < static_cast<int>(edward::media::DerivedStorageKind::Proxy) ||
      kind > static_cast<int>(edward::media::DerivedStorageKind::Render)) return false;
  const auto cleared = edward::media::RenderStorage::clearDerived(
      previewStorageRoots_, static_cast<edward::media::DerivedStorageKind>(kind));
  if (cleared) emit timelineChanged();
  return cleared;
}

edward::core::TimelineSnapshot WorkbenchRuntime::previewSnapshot() const {
  auto snapshot = timeline_.snapshot();
  for (auto& clip : snapshot.clips) {
    if (clip.kind == edward::core::TimelineClipKind::Media)
      clip.source = previewSession_.sourceFor(clip.source);
  }
  return snapshot;
}

bool WorkbenchRuntime::importMedia(const QString& path) {
  if (!controller_.dropMediaAtPlayhead(path)) {
    emit operationFailed(QStringLiteral("素材无法添加：播放头位置存在冲突，或媒体不可读"));
    return false;
  }
  if (const auto clip = timeline_.clip(controller_.selectedClip())) requestClipWaveform(*clip);
  if (const auto clip = timeline_.clip(controller_.selectedClip())) requestClipThumbnail(*clip);
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("素材已加入时间线"));
  return true;
}

void WorkbenchRuntime::requestClipWaveform(const edward::core::TimelineClip& clip) {
  if (clip.kind != edward::core::TimelineClipKind::Media || clip.source.empty() ||
      clipWaveforms_.contains(static_cast<qint64>(clip.id))) return;
  const auto info = edward::media::MediaProbe::probe(clip.source);
  if (!info || !info->hasAudio) return;
  const auto start = static_cast<double>(clip.sourceIn) * info->fpsDenominator / info->fpsNumerator;
  const auto end = static_cast<double>(clip.sourceOut) * info->fpsDenominator / info->fpsNumerator;
  const auto clipId = clip.id;
  const auto generation = waveformGeneration_;
  auto* watcher = new QFutureWatcher<QVariantList>(this);
  connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher, clipId, generation] {
    const auto peaks = watcher->result();
    watcher->deleteLater();
    if (generation != waveformGeneration_ || peaks.isEmpty() || !timeline_.clip(clipId)) return;
    clipWaveforms_.insert(static_cast<qint64>(clipId), peaks);
    emit timelineChanged();
  });
  watcher->setFuture(QtConcurrent::run([source = clip.source, start, end] {
    QVariantList peaks;
    const auto waveform = edward::media::AudioWaveformExtractor::extract(source, 96, start, end);
    if (!waveform) return peaks;
    for (const auto peak : waveform->peaks) peaks.push_back(peak);
    return peaks;
  }));
}

void WorkbenchRuntime::refreshClipWaveforms() {
  ++waveformGeneration_;
  clipWaveforms_.clear();
  for (const auto& clip : timeline_.snapshot().clips) requestClipWaveform(clip);
}

void WorkbenchRuntime::requestClipThumbnail(const edward::core::TimelineClip& clip) {
  if (clip.kind != edward::core::TimelineClipKind::Media || clip.source.empty() ||
      clipThumbnails_.contains(static_cast<qint64>(clip.id))) return;
  const auto clipId = clip.id;
  const auto generation = thumbnailGeneration_;
  auto* watcher = new QFutureWatcher<QImage>(this);
  connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, clipId, generation] {
    const auto sprite = watcher->result();
    watcher->deleteLater();
    if (generation != thumbnailGeneration_ || sprite.isNull() || !timeline_.clip(clipId)) return;
    clipThumbnails_.insert(static_cast<qint64>(clipId), sprite);
    emit timelineChanged();
  });
  watcher->setFuture(QtConcurrent::run([source = clip.source, sourceIn = clip.sourceIn, sourceOut = clip.sourceOut] {
    return thumbnailSprite(source, sourceIn, sourceOut);
  }));
}

void WorkbenchRuntime::refreshClipThumbnails() {
  ++thumbnailGeneration_;
  clipThumbnails_.clear();
  for (const auto& clip : timeline_.snapshot().clips) requestClipThumbnail(clip);
}

bool WorkbenchRuntime::selectClip(qlonglong id) {
  if (!controller_.selectClip(static_cast<edward::core::ClipId>(id))) return false;
  if (const auto clip = timeline_.clip(static_cast<edward::core::ClipId>(id));
      clip && clip->kind == edward::core::TimelineClipKind::Component && clip->component) {
    demoOverlayIr_ = *clip->component;
    componentClipId_ = 0;
    editingComponentClipId_ = clip->id;
    demoOverlayEnabled_ = true;
    syncDemoOverlayProperties(demoOverlayIr_->toJson());
    refreshDemoOverlay();
  } else if (editingComponentClipId_ != 0) {
    editingComponentClipId_ = 0;
    demoOverlayIr_.reset();
    demoOverlayEnabled_ = false;
    refreshDemoOverlay();
  }
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::selectComponentNode(const QString& nodeId) {
  if (!demoOverlayIr_ || !findNode(demoOverlayIr_->toJson().value("root").toObject(), nodeId)) return false;
  selectedComponentNodeId_ = nodeId;
  const auto component = demoOverlayIr_->toJson();
  if (const auto node = findNode(component.value("root").toObject(), nodeId)) {
    const auto properties = node->value("properties").toObject();
    if (node->value("type").toString() == QStringLiteral("text")) {
      if (properties.value("text").isString()) demoOverlayText_ = properties.value("text").toString();
      if (properties.value("fontSize").isDouble()) demoOverlayFontSize_ = properties.value("fontSize").toInt();
    }
    if (node->value("type").toString() == QStringLiteral("shape") &&
        properties.value("borderWidth").isDouble()) {
      demoOverlayBorderWidth_ = properties.value("borderWidth").toInt();
    }
  }
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::removeSelectedComponentNodeKeyframe(const QString& field, int frame) {
  if (!demoOverlayIr_ || !demoOverlayIr_->removeNodeKeyframe(selectedComponentNodeId_, field, frame)) return false;
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::toggleSelectedComponentNodeKeyframeEasing(const QString& field, int frame) {
  if (!demoOverlayIr_ || selectedComponentNodeId_.isEmpty()) return false;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node) return false;
  const auto point = node->value("keyframes").toObject().value(field).toArray();
  for (const auto& value : point) {
    const auto object = value.toObject();
    if (object.value("frame").toInt(-1) == frame) {
      const auto current = object.value("easing").toString("linear");
      const auto next = current == "bezier" ? QStringLiteral("linear") : QStringLiteral("bezier");
      if (!demoOverlayIr_->setNodeKeyframeEasing(selectedComponentNodeId_, field, frame, next)) return false;
      refreshDemoOverlay();
      emit timelineChanged();
      return true;
    }
  }
  return false;
}

bool WorkbenchRuntime::bindComponentToSelectedClip() {
  if (!demoOverlayIr_ || controller_.selectedClip() == 0 || !timeline_.clip(controller_.selectedClip())) {
    emit operationFailed(QStringLiteral("请先选择素材片段和组件"));
    return false;
  }
  componentClipId_ = controller_.selectedClip();
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("组件已绑定到选中片段"));
  return true;
}

bool WorkbenchRuntime::addCurrentComponentToTimeline(int durationFrames) {
  if (!demoOverlayIr_ || !controller_.dropComponentAtPlayhead(*demoOverlayIr_, durationFrames)) {
    emit operationFailed(QStringLiteral("组件无法添加到时间线"));
    return false;
  }
  demoOverlayIr_.reset();
  componentClipId_ = 0;
  demoOverlayEnabled_ = false;
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("组件已作为独立片段加入时间线"));
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

bool WorkbenchRuntime::applyAiComponentCommand(const QString& json) {
  if (!demoOverlayIr_) {
    emit operationFailed(QStringLiteral("AI 编辑失败：当前没有可编辑组件"));
    return false;
  }
  QString error;
  const auto command = edward::core::parseComponentEditCommandText(json, &error);
  if (command && !pluginAllowsComponentEdit(*demoOverlayIr_, installedPlugin_, *command)) {
    emit operationFailed(QStringLiteral("AI 编辑失败：外部插件未声明该可编辑字段"));
    return false;
  }
  if (!command || !edward::core::ComponentEditCommand::apply(*demoOverlayIr_, *command, &error)) {
    emit operationFailed(QStringLiteral("AI 编辑失败：%1").arg(error));
    return false;
  }
  syncDemoOverlayProperties(demoOverlayIr_->toJson());
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("AI 组件编辑已应用"));
  return true;
}

bool WorkbenchRuntime::requestAiComponentDraft(const QString& endpoint, const QString& apiKey,
                                               const QString& model, const QString& prompt) {
  if (aiRequestBusy_) return false;
  if (!demoOverlayIr_) {
    emit operationFailed(QStringLiteral("AI 请求失败：当前没有可编辑组件"));
    return false;
  }
  const auto systemPrompt = QStringLiteral(
      "Return exactly one JSON object for Edward component editing. Allowed operations are "
      "setTransformNumber, setProperty, setKeyframeValue. Do not use Markdown or code fences.");
  const auto componentContext = QString::fromUtf8(
      QJsonDocument(demoOverlayIr_->toJson()).toJson(QJsonDocument::Compact));
  const auto dependency = demoOverlayIr_->pluginDependency();
  const auto editable = dependency && installedPlugin_ &&
                                dependency->pluginId == installedPlugin_->manifest.pluginId &&
                                dependency->version == installedPlugin_->manifest.version
                            ? installedPlugin_->manifest.editableProps.join(QStringLiteral(","))
                            : QStringLiteral("standard Edward fields");
  const auto contextualPrompt = QStringLiteral(
      "Current Component IR (read-only context): %1\nAllowed plugin editable fields: %2\nPrevious conversation: %3\nUser request: %4")
                                    .arg(componentContext, editable, aiConversation_, prompt);
  if (contextualPrompt.toUtf8().size() > 64 * 1024) {
    emit operationFailed(QStringLiteral("AI 请求失败：组件上下文或提示词过大"));
    return false;
  }
  aiRequestBusy_ = true;
  emit timelineChanged();
  if (!modelChatClient_.request({endpoint, apiKey, model}, systemPrompt, contextualPrompt)) {
    aiRequestBusy_ = false;
    emit timelineChanged();
    return false;
  }
  pendingAiPrompt_ = prompt;
  return true;
}

bool WorkbenchRuntime::proposeAiComponentCommand(const QString& json) {
  if (!demoOverlayIr_) {
    emit operationFailed(QStringLiteral("AI 草案失败：当前没有可编辑组件"));
    return false;
  }
  QString error;
  const auto command = edward::core::parseComponentEditCommandText(json, &error);
  if (!command) {
    emit operationFailed(QStringLiteral("AI 草案失败：%1").arg(error));
    return false;
  }
  if (!pluginAllowsComponentEdit(*demoOverlayIr_, installedPlugin_, *command)) {
    emit operationFailed(QStringLiteral("AI 草案失败：外部插件未声明该可编辑字段"));
    return false;
  }
  auto candidate = *demoOverlayIr_;
  if (!edward::core::ComponentEditCommand::apply(candidate, *command, &error)) {
    emit operationFailed(QStringLiteral("AI 草案失败：%1").arg(error));
    return false;
  }
  aiComponentDraft_ = std::move(candidate);
  aiComponentDraftJson_ = json.trimmed();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("AI 草案已生成，确认后应用"));
  return true;
}

bool WorkbenchRuntime::applyPendingAiComponentCommand() {
  if (!aiComponentDraft_) {
    emit operationFailed(QStringLiteral("没有待应用的 AI 草案"));
    return false;
  }
  demoOverlayIr_ = std::move(aiComponentDraft_);
  aiComponentDraftJson_.clear();
  syncDemoOverlayProperties(demoOverlayIr_->toJson());
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("AI 草案已应用"));
  return true;
}

void WorkbenchRuntime::discardPendingAiComponentCommand() {
  if (!aiComponentDraft_) return;
  aiComponentDraft_.reset();
  aiComponentDraftJson_.clear();
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
  componentClipId_ = 0;
  editingComponentClipId_ = 0;
  aiConversation_.clear();
  pendingAiPrompt_.clear();
  syncDemoOverlayProperties(document.object());
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

QVariantList WorkbenchRuntime::localComponents() const {
  QVariantList result;
  for (const auto& item : componentLibrary_.list()) {
    result.push_back(QVariantMap{{QStringLiteral("resourceId"), item.resourceId},
                                 {QStringLiteral("displayName"), item.displayName},
                                 {QStringLiteral("category"), item.category}});
  }
  return result;
}

bool WorkbenchRuntime::configureComponentLibrary(const QString& rootPath) {
  if (rootPath.isEmpty()) return false;
  componentLibrary_.setRoot(rootPath.toStdString());
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::saveCurrentComponentToLibrary(const QString& resourceId, const QString& displayName,
                                                     const QString& category) {
  if (!demoOverlayIr_ || resourceId.isEmpty() || displayName.isEmpty()) {
    emit operationFailed(QStringLiteral("请先生成组件并填写资源信息"));
    return false;
  }
  edward::resources::ComponentPackage package{resourceId, displayName, *demoOverlayIr_, {}, {}, {}, {}, category};
  QString error;
  if (!componentLibrary_.save(package, &error)) {
    emit operationFailed(QStringLiteral("组件保存到资源库失败：%1").arg(error));
    return false;
  }
  const auto localPackagePath = componentLibrary_.root() / resourceId.toStdString();
  if (silentUploadDispatcher_ && sessions_.authenticated() &&
      silentUploadDispatcher_->enqueue(QString::fromStdString(localPackagePath.string()), resourceId,
                                       QDateTime::currentDateTimeUtc(), &error)) {
    dispatchSilentComponentUploads();
  }
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("组件已保存到我的资源库"));
  return true;
}

bool WorkbenchRuntime::loadLibraryComponent(const QString& resourceId) {
  QString error;
  const auto package = componentLibrary_.load(resourceId, &error);
  if (!package) {
    emit operationFailed(QStringLiteral("资源库组件无法读取：%1").arg(error));
    return false;
  }
  demoOverlayIr_ = package->component;
  componentClipId_ = 0;
  editingComponentClipId_ = 0;
  demoOverlayEnabled_ = true;
  aiConversation_.clear();
  pendingAiPrompt_.clear();
  syncDemoOverlayProperties(demoOverlayIr_->toJson());
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("资源库组件已载入为独立实例"));
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
  return exportInstalledPlugin(requestId, compositionId, outputPath, false);
}

bool WorkbenchRuntime::applyInstalledPluginToTimeline(const QString& requestId, const QString& compositionId,
                                                     const QString& outputPath) {
  return exportInstalledPlugin(requestId, compositionId, outputPath, true);
}

bool WorkbenchRuntime::exportInstalledPlugin(const QString& requestId, const QString& compositionId,
                                             const QString& outputPath, bool applyToTimeline) {
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
  const auto snapshot = timeline_.snapshot();
  edward::core::Frame frameCount = 0;
  for (const auto& clip : snapshot.clips)
    frameCount = std::max(frameCount, clip.timelineStart + clip.sourceOut - clip.sourceIn);
  if (frameCount <= 0) frameCount = 1;
  pluginExportWatcher_.setFuture(QtConcurrent::run([plugin, requestId, compositionId, outputRoot, outputName, size, frameCount, applyToTimeline, outputPath] {
    PluginExportResult result;
    result.outputPath = outputPath;
    result.applyToTimeline = applyToTimeline;
    QString error;
    const edward::plugins::RenderExportRequest request{requestId, compositionId, outputName, size,
                                                        static_cast<int>(frameCount), 25, 1, 30000};
    if (!edward::plugins::exportPlugin(plugin.manifest, plugin.root, outputRoot, request, &error)) {
      result.error = QStringLiteral("插件导出失败：%1").arg(error);
    }
    return result;
  }));
  return true;
}

bool WorkbenchRuntime::exportTimeline(const QString& outputPath) {
  return exportTimelineWithOptions(outputPath, 1920, 1080, 25, 0);
}

bool WorkbenchRuntime::exportTimelineWithOptions(const QString& outputPath, int width, int height,
                                                 int fps, int quality) {
  if (timelineExportBusy_) {
    emit operationFailed(QStringLiteral("视频导出正在执行"));
    return false;
  }
  if (outputPath.isEmpty()) {
    emit operationFailed(QStringLiteral("请选择导出路径"));
    return false;
  }
  if (width <= 0 || height <= 0 || fps <= 0 || quality < 0 || quality > 2) {
    emit operationFailed(QStringLiteral("导出参数无效"));
    return false;
  }
  const auto path = std::filesystem::path(outputPath.toStdString());
  std::error_code pathError;
  if (!path.is_absolute() || path.filename().empty() || path.extension() != ".mp4"
      || !std::filesystem::is_directory(path.parent_path(), pathError)) {
    emit operationFailed(QStringLiteral("导出路径必须是现有目录中的 MP4 文件"));
    return false;
  }
  auto snapshot = timeline_.snapshot();
  if (snapshot.clips.empty()) {
    emit operationFailed(QStringLiteral("时间线没有可导出的视频片段"));
    return false;
  }
  edward::core::Frame lastFrame = 0;
  for (const auto& clip : snapshot.clips)
    lastFrame = std::max(lastFrame, clip.timelineStart + clip.sourceOut - clip.sourceIn);
  snapshot.durationFrames = lastFrame;
  const auto component = demoOverlayIr_ ? std::optional<QJsonObject>(demoOverlayIr_->toJson()) : std::nullopt;
  const auto componentClipId = componentClipId_;
  timelineExportBusy_ = true;
  timelineExportProgress_ = 0;
  timelineExportCancel_ = std::make_shared<std::atomic_bool>(false);
  emit timelineChanged();
  const auto exportQuality = quality == 0 ? edward::media::ExportQuality::High
                                          : quality == 1 ? edward::media::ExportQuality::Medium
                                                         : edward::media::ExportQuality::Low;
  QPointer<WorkbenchRuntime> runtime(this);
  const auto cancel = timelineExportCancel_;
  timelineExportWatcher_.setFuture(QtConcurrent::run([snapshot, component, componentClipId, path, width, height, fps, exportQuality, runtime, cancel] {
    TimelineExportResult result;
    edward::media::MltAdapter adapter;
    std::optional<edward::core::ComponentIr> overlay;
    if (component) {
      overlay = edward::core::ComponentIr::parse(*component);
      if (!overlay) {
        result.error = QStringLiteral("组件数据无效，无法导出");
        return result;
      }
    }
    edward::media::RenderGraph graph(adapter);
    std::vector<edward::media::ComponentLayer> componentLayers;
    for (const auto& clip : snapshot.clips) {
      if (clip.kind != edward::core::TimelineClipKind::Component || !clip.component) continue;
      componentLayers.push_back({clip.timelineStart,
                                 clip.timelineStart + clip.sourceOut - clip.sourceIn,
                                 clip.sourceIn,
                                 *clip.component});
    }
    if (overlay && componentClipId != 0) {
      const auto clip = std::ranges::find_if(snapshot.clips, [componentClipId](const auto& candidate) {
        return candidate.id == componentClipId;
      });
      if (clip == snapshot.clips.end()) {
        result.error = QStringLiteral("组件绑定的时间线片段不存在");
        return result;
      }
    }
    if (!componentLayers.empty()) {
      graph.setComponentLayers(std::move(componentLayers));
    }
    if (overlay && componentClipId == 0) {
      graph.setOverlay(std::move(overlay));
    }
    const edward::media::ExportJob job(graph);
    const auto exported = job.run(snapshot, {path, {width, height}, fps, 1, exportQuality,
                                              [cancel] { return cancel && cancel->load(); }},
                                  [runtime](edward::core::Frame completed, edward::core::Frame total) {
      if (!runtime || total <= 0) return;
      const auto progress = static_cast<int>(completed * 100 / total);
      QMetaObject::invokeMethod(runtime, [runtime, progress] {
        if (!runtime) return;
        runtime->timelineExportProgress_ = progress;
        emit runtime->timelineChanged();
      }, Qt::QueuedConnection);
    });
    if (!exported)
      result.error = QStringLiteral("视频导出失败");
    else
      result.outputPath = QString::fromStdString(exported->outputPath.string());
    return result;
  }));
  return true;
}

void WorkbenchRuntime::cancelTimelineExport() {
  if (!timelineExportBusy_ || !timelineExportCancel_) return;
  timelineExportCancel_->store(true);
}

void WorkbenchRuntime::clearComponentOverlay() {
  demoOverlayIr_.reset();
  componentClipId_ = 0;
  editingComponentClipId_ = 0;
  aiConversation_.clear();
  pendingAiPrompt_.clear();
  demoOverlayEnabled_ = false;
  renderGraph_.setOverlay(std::nullopt);
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayX(int value) {
  if (!componentPlayheadIsEditable()) return;
  const int clamped = std::max(-640, std::min(value, 640));
  if (demoOverlayX_ == clamped) return;
  demoOverlayX_ = clamped;
  if (demoOverlayIr_) {
    setTransformAndKeyframe(*demoOverlayIr_, "demo-box", "x", demoOverlayX_, componentKeyframeFrame());
    setTransformAndKeyframe(*demoOverlayIr_, "demo-text", "x", demoOverlayX_ + 20, componentKeyframeFrame());
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayY(int value) {
  if (!componentPlayheadIsEditable()) return;
  const int clamped = std::max(-360, std::min(value, 360));
  if (demoOverlayY_ == clamped) return;
  demoOverlayY_ = clamped;
  if (demoOverlayIr_) {
    setTransformAndKeyframe(*demoOverlayIr_, "demo-box", "y", demoOverlayY_, componentKeyframeFrame());
    setTransformAndKeyframe(*demoOverlayIr_, "demo-text", "y", demoOverlayY_ - 20, componentKeyframeFrame());
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayWidth(int value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayWidth_ = std::max(40, std::min(value, 640));
  if (demoOverlayIr_) {
    setTransformAndKeyframe(*demoOverlayIr_, "demo-box", "width", demoOverlayWidth_, componentKeyframeFrame());
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayHeight(int value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayHeight_ = std::max(24, std::min(value, 360));
  if (demoOverlayIr_) {
    setTransformAndKeyframe(*demoOverlayIr_, "demo-box", "height", demoOverlayHeight_, componentKeyframeFrame());
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayScale(double value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayScale_ = std::max(0.1, std::min(value, 3.0));
  if (demoOverlayIr_) {
    setTransformAndKeyframe(*demoOverlayIr_, "demo-box", "scaleX", demoOverlayScale_, componentKeyframeFrame());
    setTransformAndKeyframe(*demoOverlayIr_, "demo-box", "scaleY", demoOverlayScale_, componentKeyframeFrame());
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayRotation(double value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayRotation_ = std::max(-180.0, std::min(value, 180.0));
  if (demoOverlayIr_) {
    setTransformAndKeyframe(*demoOverlayIr_, "demo-box", "rotation", demoOverlayRotation_, componentKeyframeFrame());
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayOpacity(double value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayOpacity_ = std::max(0.0, std::min(value, 1.0));
  if (demoOverlayIr_) {
    setPropertyAndKeyframe(*demoOverlayIr_, "demo-box", "opacity", demoOverlayOpacity_, componentKeyframeFrame());
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayText(const QString& value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayText_ = value.left(120);
  if (demoOverlayIr_)
    setPropertyAndKeyframe(*demoOverlayIr_, editableTextNodeId(demoOverlayIr_->toJson(), selectedComponentNodeId_),
                           "text", demoOverlayText_, componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayFontSize(int value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayFontSize_ = std::max(8, std::min(value, 96));
  if (demoOverlayIr_)
    setPropertyAndKeyframe(*demoOverlayIr_, editableTextNodeId(demoOverlayIr_->toJson(), selectedComponentNodeId_),
                           "fontSize", demoOverlayFontSize_, componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayBorderWidth(int value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayBorderWidth_ = std::max(0, std::min(value, 32));
  if (demoOverlayIr_)
    setPropertyAndKeyframe(*demoOverlayIr_, editableShapeNodeId(demoOverlayIr_->toJson(), selectedComponentNodeId_),
                           "borderWidth", demoOverlayBorderWidth_, componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeX(int value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  setTransformAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("x"),
                          std::max(-640, std::min(value, 640)), componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeY(int value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  setTransformAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("y"),
                          std::max(-360, std::min(value, 360)), componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeWidth(int value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  setTransformAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("width"),
                          std::max(1, std::min(value, 640)), componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeHeight(int value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  setTransformAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("height"),
                          std::max(1, std::min(value, 360)), componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeRotation(double value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  const auto clamped = std::max(-180.0, std::min(value, 180.0));
  setTransformAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("rotation"),
                          clamped, componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeOpacity(double value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  const auto clamped = std::max(0.0, std::min(value, 1.0));
  setPropertyAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("opacity"),
                         clamped, componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeColor(const QString& value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  const auto type = node->value("type").toString();
  const auto field = type == QStringLiteral("shape") ? QStringLiteral("fill") : QStringLiteral("color");
  const auto color = value.trimmed().left(32);
  if (!color.startsWith(QLatin1Char('#')) || (color.size() != 4 && color.size() != 7 && color.size() != 9)) return;
  setPropertyAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, field, color, componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeBorderColor(const QString& value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable()) return;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node || node->value("type").toString() != QStringLiteral("shape")) return;
  const auto color = value.trimmed().left(32);
  if (!color.startsWith(QLatin1Char('#')) || (color.size() != 4 && color.size() != 7 && color.size() != 9)) return;
  setPropertyAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("borderColor"), color, componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeFontFamily(const QString& value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable()) return;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node || node->value("type").toString() != QStringLiteral("text")) return;
  const auto family = value.trimmed().left(120);
  if (family.isEmpty() || family.contains(QRegularExpression(QStringLiteral("[\\r\\n]")))) return;
  setPropertyAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("fontFamily"), family, componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

bool WorkbenchRuntime::setPlayhead(int frame) {
  if (!controller_.setPlayhead(frame)) return false;
  if (playing_ && !audioPreview_.start(timeline_.snapshot(), controller_.playheadFrame(), 25, 1))
    emit operationFailed(QStringLiteral("音频预览无法启动"));
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::connectResolve() {
  const auto bridgeUrl = qEnvironmentVariable("EDWARD_RESOLVE_BRIDGE_URL");
  if (bridgeUrl.isEmpty()) {
    resolveConnected_ = false;
    resolveStatus_ = QStringLiteral("请先启动并连接 Resolve Studio");
    emit resolveStateChanged();
    emit operationFailed(resolveStatus_);
    return false;
  }

  QString error;
  if (!resolveConnection_.connectToBridge(QUrl(bridgeUrl), &error) || !resolveAdapter_.attach(&error)) {
    resolveConnected_ = false;
    resolveStatus_ = error.isEmpty() ? QStringLiteral("Resolve Studio 连接失败")
                                    : QStringLiteral("Resolve Studio 连接失败：%1").arg(error);
    emit resolveStateChanged();
    emit operationFailed(resolveStatus_);
    return false;
  }
  resolveConnected_ = true;
  resolveStatus_ = QStringLiteral("已连接 Resolve Studio");
  emit resolveStateChanged();
  return refreshResolveTimeline();
}

void WorkbenchRuntime::disconnectResolve() {
  resolveConnection_.disconnect();
  resolveConnected_ = false;
  resolveTimelineSnapshot_.reset();
  resolveStatus_ = QStringLiteral("未连接 Resolve Studio");
  emit resolveStateChanged();
}

bool WorkbenchRuntime::refreshResolveTimeline() {
  if (!resolveConnected_) {
    resolveStatus_ = QStringLiteral("未连接 Resolve Studio");
    emit resolveStateChanged();
    return false;
  }
  QString error;
  const auto snapshot = resolveAdapter_.timelineSnapshot(&error);
  if (!snapshot) {
    resolveStatus_ = QStringLiteral("Resolve 时间线读取失败：%1").arg(error);
    emit resolveStateChanged();
    emit operationFailed(resolveStatus_);
    return false;
  }
  resolveTimelineSnapshot_ = snapshot;
  resolveStatus_ = QStringLiteral("已连接 Resolve Studio：%1").arg(snapshot->timelineName);
  emit resolveStateChanged();
  return true;
}

bool WorkbenchRuntime::setSelectedComponentPropertyAtPlayhead(const QString& nodeId,
                                                               const QString& field,
                                                               const QJsonValue& value) {
  if (!resolveConnected_ || !resolveTimelineSnapshot_) {
    emit operationFailed(QStringLiteral("请先连接 Resolve Studio"));
    return false;
  }
  if (!demoOverlayIr_ || resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    emit operationFailed(QStringLiteral("当前组件尚未添加到 Resolve 时间线"));
    return false;
  }
  if (!value.isDouble()) {
    emit operationFailed(QStringLiteral("当前 Resolve 关键帧接口只支持数值属性"));
    return false;
  }
  const auto before = demoOverlayIr_->toJson();
  const auto frame = resolveTimelineSnapshot_->playheadFrame;
  bool changed = false;
  if (value.isDouble()) {
    changed = demoOverlayIr_->setNodeTransformNumber(nodeId, field, value.toDouble()) &&
              demoOverlayIr_->setNodeKeyframeNumber(nodeId, field, frame, value.toDouble());
  }
  if (!changed) {
    emit operationFailed(QStringLiteral("组件属性无效"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.setComponentKeyframe(resolveAdapter_.lastInsertedComponentId(), nodeId, field,
                                             frame, value.toDouble(), &error)) {
    demoOverlayIr_ = edward::core::ComponentIr::parse(before);
    emit operationFailed(QStringLiteral("Resolve 关键帧写入失败：%1").arg(error));
    return false;
  }
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::addDissolveToSelected() {
  return addTransitionToSelected(QStringLiteral("dissolve"));
}

bool WorkbenchRuntime::addTransitionToSelected(const QString& type) {
  const auto selected = timeline_.clip(controller_.selectedClip());
  if (!selected) return false;
  const auto trackClips = timeline_.clips(selected->trackId);
  const auto adjacent = std::ranges::find_if(trackClips, [&](const auto& candidate) {
    return candidate.timelineStart == selected->timelineStart + (selected->sourceOut - selected->sourceIn);
  });
  if (adjacent == trackClips.end()) return false;
  const auto duration = std::min<edward::core::Frame>(15,
      std::min(selected->sourceOut - selected->sourceIn, adjacent->sourceOut - adjacent->sourceIn));
  edward::core::TransitionType transitionType;
  if (type == QStringLiteral("dissolve")) transitionType = edward::core::TransitionType::Dissolve;
  else if (type == QStringLiteral("flash_black")) transitionType = edward::core::TransitionType::FlashBlack;
  else if (type == QStringLiteral("flash_white")) transitionType = edward::core::TransitionType::FlashWhite;
  else return false;
  const auto transition = timeline_.addTransition(transitionType,
                                                  selected->id, adjacent->id, duration);
  if (!transition) return false;
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::setTransitionDuration(qlonglong leftClipId, qlonglong rightClipId,
                                             int durationFrames) {
  if (durationFrames <= 0) return false;
  if (!controller_.setTransitionDuration(static_cast<edward::core::ClipId>(leftClipId),
                                         static_cast<edward::core::ClipId>(rightClipId), durationFrames)) return false;
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::removeTransition(qlonglong leftClipId, qlonglong rightClipId) {
  if (!controller_.removeTransition(static_cast<edward::core::ClipId>(leftClipId),
                                    static_cast<edward::core::ClipId>(rightClipId))) return false;
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::writeProject(const QString& path) const {
  if (path.isEmpty()) return false;
  const auto snapshot = timeline_.snapshot();
  QJsonArray tracks;
  for (const auto track : snapshot.videoTracks) tracks.append(static_cast<qint64>(track));
  QJsonArray clips;
  for (const auto& clip : snapshot.clips) {
    clips.append(QJsonObject{{"id", static_cast<qint64>(clip.id)},
                             {"trackId", static_cast<qint64>(clip.trackId)},
                             {"source", QString::fromStdString(clip.source.string())},
                             {"sourceIn", static_cast<qint64>(clip.sourceIn)},
                             {"sourceOut", static_cast<qint64>(clip.sourceOut)},
                             {"timelineStart", static_cast<qint64>(clip.timelineStart)},
                             {"kind", clip.kind == edward::core::TimelineClipKind::Component ? "component" : "media"},
                             {"component", clip.component ? QJsonValue(clip.component->toJson()) : QJsonValue()}});
  }
  QJsonArray transitions;
  for (const auto& transition : snapshot.transitions) {
    const auto type = transition.type == edward::core::TransitionType::FlashBlack
                          ? QStringLiteral("flash_black")
                          : transition.type == edward::core::TransitionType::FlashWhite
                                ? QStringLiteral("flash_white") : QStringLiteral("dissolve");
    transitions.append(QJsonObject{{"type", type}, {"leftClipId", static_cast<qint64>(transition.leftClipId)},
                                   {"rightClipId", static_cast<qint64>(transition.rightClipId)},
                                   {"startFrame", static_cast<qint64>(transition.startFrame)},
                                   {"durationFrames", static_cast<qint64>(transition.durationFrames)}});
  }
  QJsonObject project{{"version", 1}, {"durationFrames", static_cast<qint64>(snapshot.durationFrames)},
                      {"playheadFrame", static_cast<qint64>(snapshot.playheadFrame)},
                      {"videoTracks", tracks}, {"clips", clips}, {"transitions", transitions},
                      {"projectId", projectIdentity_.value()}};
  if (demoOverlayIr_) project.insert("component", demoOverlayIr_->toJson());
  if (componentClipId_ != 0) project.insert("componentClipId", static_cast<qint64>(componentClipId_));
  if (!aiConversation_.isEmpty()) project.insert("aiConversation", aiConversation_);
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(project).toJson(QJsonDocument::Compact)) < 0) return false;
  return file.commit();
}

QString WorkbenchRuntime::recoveryPathForProject(const QString& path) const {
  return path + QStringLiteral(".autosave");
}

bool WorkbenchRuntime::saveProject(const QString& path) {
  if (!writeProject(path)) return false;
  activeProjectPath_ = path;
  projectSaveState_ = ProjectSaveState::Saved;
  projectAutosaveTimer_.stop();
  QFile::remove(recoveryPathForProject(path));
  writingProjectStatus_ = true;
  emit timelineChanged();
  writingProjectStatus_ = false;
  return true;
}

QString WorkbenchRuntime::projectWindowTitle() const {
  QString name = QStringLiteral("未命名项目");
  if (!activeProjectPath_.isEmpty()) {
    name = QFileInfo(activeProjectPath_).fileName();
    const auto edwardExtension = QStringLiteral(".edward.json");
    if (name.endsWith(edwardExtension))
      name.chop(edwardExtension.size());
    else
      name = QFileInfo(activeProjectPath_).completeBaseName();
  }
  const auto status = projectSaveState_ == ProjectSaveState::Saved
                          ? QStringLiteral("已保存")
                          : projectSaveState_ == ProjectSaveState::AutoSaved
                                ? QStringLiteral("已保存 · 刚刚自动保存")
                                : QStringLiteral("未保存更改");
  return name + QStringLiteral(" — ") + status;
}

bool WorkbenchRuntime::hasProjectRecovery(const QString& path) const {
  if (path.isEmpty()) return false;
  const QFileInfo project(path);
  const QFileInfo recovery(recoveryPathForProject(path));
  return project.exists() && recovery.exists() && recovery.lastModified() > project.lastModified();
}

bool WorkbenchRuntime::recoverProject(const QString& path) {
  if (!hasProjectRecovery(path)) {
    emit operationFailed(QStringLiteral("没有可恢复的自动保存副本"));
    return false;
  }
  if (!loadProject(recoveryPathForProject(path))) {
    emit operationFailed(QStringLiteral("自动保存副本已损坏，正式工程保持不变"));
    return false;
  }
  activeProjectPath_ = path;
  return true;
}

bool WorkbenchRuntime::discardProjectRecovery(const QString& path) {
  if (path.isEmpty()) return false;
  const auto recovery = recoveryPathForProject(path);
  return !QFile::exists(recovery) || QFile::remove(recovery);
}

void WorkbenchRuntime::scheduleProjectAutosave() {
  if (loadingProject_ || writingProjectStatus_ || activeProjectPath_.isEmpty()) return;
  if (projectSaveState_ != ProjectSaveState::Unsaved) {
    projectSaveState_ = ProjectSaveState::Unsaved;
    writingProjectStatus_ = true;
    emit timelineChanged();
    writingProjectStatus_ = false;
  }
  projectAutosaveTimer_.start();
}

void WorkbenchRuntime::saveProjectRecovery() {
  if (activeProjectPath_.isEmpty()) return;
  const auto saved = writeProject(recoveryPathForProject(activeProjectPath_));
  if (!saved) {
    emit operationFailed(QStringLiteral("工程自动保存失败"));
    return;
  }
  projectSaveState_ = ProjectSaveState::AutoSaved;
  writingProjectStatus_ = true;
  emit timelineChanged();
  writingProjectStatus_ = false;
}

bool WorkbenchRuntime::loadProject(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return false;
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
  const auto project = document.object();
  if (project.value("version").toInt() != 1 || !project.value("durationFrames").isDouble() ||
      !project.value("playheadFrame").isDouble() || !project.value("videoTracks").isArray() ||
      !project.value("clips").isArray()) return false;
  auto projectIdentity = edward::core::ProjectIdentity::create();
  if (!project.value("projectId").isUndefined()) {
    if (!project.value("projectId").isString()) return false;
    const auto parsed = edward::core::ProjectIdentity::parse(project.value("projectId").toString());
    if (!parsed) return false;
    projectIdentity = *parsed;
  }
  edward::core::TimelineSnapshot snapshot;
  snapshot.durationFrames = project.value("durationFrames").toInteger();
  snapshot.playheadFrame = project.value("playheadFrame").toInteger();
  for (const auto value : project.value("videoTracks").toArray()) {
    if (!value.isDouble()) return false;
    snapshot.videoTracks.push_back(value.toInteger());
  }
  for (const auto value : project.value("clips").toArray()) {
    if (!value.isObject()) return false;
    const auto clip = value.toObject();
    if (!clip.value("id").isDouble() || !clip.value("trackId").isDouble() || !clip.value("source").isString() ||
        !clip.value("sourceIn").isDouble() || !clip.value("sourceOut").isDouble() || !clip.value("timelineStart").isDouble()) return false;
    const auto kindValue = clip.value("kind").toString(QStringLiteral("media"));
    const auto kind = kindValue == QStringLiteral("component") ? edward::core::TimelineClipKind::Component
                                                                  : edward::core::TimelineClipKind::Media;
    if (kindValue != QStringLiteral("media") && kindValue != QStringLiteral("component")) return false;
    std::optional<edward::core::ComponentIr> clipComponent;
    if (kind == edward::core::TimelineClipKind::Component) {
      if (!clip.value("component").isObject()) return false;
      clipComponent = edward::core::ComponentIr::parse(clip.value("component").toObject());
      if (!clipComponent) return false;
    }
    snapshot.clips.push_back({static_cast<edward::core::ClipId>(clip.value("id").toInteger()),
                              static_cast<edward::core::TrackId>(clip.value("trackId").toInteger()),
                              clip.value("source").toString().toStdString(), clip.value("sourceIn").toInteger(),
                              clip.value("sourceOut").toInteger(), clip.value("timelineStart").toInteger(),
                              kind, std::move(clipComponent)});
  }
  if (!project.value("transitions").isUndefined()) {
    if (!project.value("transitions").isArray()) return false;
    for (const auto value : project.value("transitions").toArray()) {
      if (!value.isObject()) return false;
      const auto transition = value.toObject();
      const auto type = transition.value("type").toString();
      edward::core::TransitionType transitionType;
      if (type == QStringLiteral("flash_black")) transitionType = edward::core::TransitionType::FlashBlack;
      else if (type == QStringLiteral("flash_white")) transitionType = edward::core::TransitionType::FlashWhite;
      else if (type == QStringLiteral("dissolve")) transitionType = edward::core::TransitionType::Dissolve;
      else return false;
      if (!transition.value("leftClipId").isDouble() || !transition.value("rightClipId").isDouble() ||
          !transition.value("startFrame").isDouble() || !transition.value("durationFrames").isDouble()) return false;
      snapshot.transitions.push_back({transitionType, static_cast<edward::core::ClipId>(transition.value("leftClipId").toInteger()),
                                      static_cast<edward::core::ClipId>(transition.value("rightClipId").toInteger()),
                                      transition.value("startFrame").toInteger(), transition.value("durationFrames").toInteger()});
    }
  }
  std::optional<edward::core::ComponentIr> component;
  if (!project.value("component").isUndefined()) {
    if (!project.value("component").isObject()) return false;
    component = edward::core::ComponentIr::parse(project.value("component").toObject());
    if (!component) return false;
  }
  QString conversation;
  if (!project.value("aiConversation").isUndefined()) {
    if (!project.value("aiConversation").isString()) return false;
    conversation = project.value("aiConversation").toString();
    if (conversation.toUtf8().size() > 64 * 1024) return false;
  }
  edward::core::ClipId componentClipId = 0;
  if (!project.value("componentClipId").isUndefined()) {
    if (!project.value("componentClipId").isDouble()) return false;
    componentClipId = static_cast<edward::core::ClipId>(project.value("componentClipId").toInteger());
    if (componentClipId == 0 || !std::ranges::any_of(snapshot.clips, [componentClipId](const auto& clip) {
          return clip.id == componentClipId;
        })) return false;
  }
  if (!timeline_.restore(snapshot)) return false;
  projectIdentity_ = std::move(projectIdentity);
  previewSession_ = edward::media::PreviewSession(previewStorageRoots_, projectIdentity_);
  previewFrameCache_ = edward::media::PreviewFrameCache(previewStorageRoots_, projectIdentity_);
  refreshClipWaveforms();
  refreshClipThumbnails();
  demoOverlayIr_ = std::move(component);
  componentClipId_ = demoOverlayIr_ ? componentClipId : 0;
  aiConversation_ = std::move(conversation);
  pendingAiPrompt_.clear();
  if (demoOverlayIr_) syncDemoOverlayProperties(project.value("component").toObject());
  demoOverlayEnabled_ = demoOverlayIr_.has_value();
  refreshDemoOverlay();
  activeProjectPath_ = path;
  projectSaveState_ = ProjectSaveState::Saved;
  loadingProject_ = true;
  emit timelineChanged();
  loadingProject_ = false;
  return true;
}

void WorkbenchRuntime::togglePlayback() {
  if (playing_) {
    playing_ = false;
    playbackTimer_.stop();
    audioPreview_.stop();
  } else {
    if (controller_.playheadFrame() >= timeline_.snapshot().durationFrames) controller_.setPlayhead(0);
    playing_ = true;
    if (!audioPreview_.start(timeline_.snapshot(), controller_.playheadFrame(), 25, 1) &&
        std::ranges::any_of(timeline_.snapshot().clips, [](const auto& clip) {
          return clip.kind == edward::core::TimelineClipKind::Media;
        }))
      emit operationFailed(QStringLiteral("音频预览无法启动，视频仍会无声播放"));
    playbackTimer_.start();
  }
  emit timelineChanged();
}

bool WorkbenchRuntime::splitSelected() {
  if (!controller_.splitSelectedAtPlayhead()) {
    emit operationFailed(QStringLiteral("播放头不在选中片段内部"));
    return false;
  }
  refreshClipWaveforms();
  refreshClipThumbnails();
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::deleteSelected() {
  if (!controller_.deleteSelected()) {
    emit operationFailed(QStringLiteral("没有可删除的片段"));
    return false;
  }
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::rippleDeleteSelected() {
  if (!controller_.rippleDeleteSelected()) {
    emit operationFailed(QStringLiteral("没有可波纹删除的片段"));
    return false;
  }
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::moveSelected(qlonglong destination) {
  if (!controller_.moveSelectedTo(static_cast<edward::core::Frame>(destination))) {
    emit operationFailed(QStringLiteral("片段移动后会超出时间线或覆盖同轨片段"));
    return false;
  }
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::trimSelectedLeft() {
  if (!controller_.trimSelectedLeftToPlayhead()) {
    emit operationFailed(QStringLiteral("播放头必须位于选中片段内部"));
    return false;
  }
  refreshClipWaveforms();
  refreshClipThumbnails();
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::trimSelectedRight() {
  if (!controller_.trimSelectedRightToPlayhead()) {
    emit operationFailed(QStringLiteral("播放头必须位于选中片段内部"));
    return false;
  }
  refreshClipWaveforms();
  refreshClipThumbnails();
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::addVideoTrack() {
  const auto track = timeline_.addVideoTrack();
  if (track == 0) {
    emit operationFailed(QStringLiteral("无法新增视频轨"));
    return false;
  }
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("已新增视频轨"));
  return true;
}

bool WorkbenchRuntime::removeEmptyVideoTrack() {
  const auto tracks = timeline_.snapshot().videoTracks;
  for (auto it = tracks.rbegin(); it != tracks.rend(); ++it) {
    if (timeline_.removeEmptyVideoTrack(*it)) {
      const auto remaining = timeline_.snapshot().videoTracks;
      if (controller_.targetTrack() == *it) controller_.setTargetTrack(remaining.front());
      emit timelineChanged();
      emit operationSucceeded(QStringLiteral("已删除空视频轨"));
      return true;
    }
  }
  emit operationFailed(QStringLiteral("只能删除额外的空视频轨"));
  return false;
}

bool WorkbenchRuntime::selectVideoTrack(int index) {
  const auto tracks = timeline_.snapshot().videoTracks;
  if (index < 0 || index >= static_cast<int>(tracks.size()) || !controller_.setTargetTrack(tracks[index])) return false;
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::undoTimeline() {
  if (!controller_.undo()) return false;
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::redoTimeline() {
  if (!controller_.redo()) return false;
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

}  // namespace edward::desktop
