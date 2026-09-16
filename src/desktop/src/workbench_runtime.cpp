#include "edward/desktop/workbench_runtime.hpp"

#include "edward/media/export_job.hpp"
#include "edward/media/audio_waveform.hpp"
#include "edward/plugins/plugin_host.hpp"
#include "edward/resources/component_package.hpp"
#include "edward/resolve/fusion_converter.hpp"
#include "edward/resolve/resolve_component_capability_matrix.hpp"
#include "edward/core/component_edit_command.hpp"
#include "edward/core/component_edit_command_parser.hpp"
#include "edward/desktop/diagnostics_reporter.hpp"
#include "edward/runtime/runtime_manifest.hpp"
#include "edward/runtime/web_runtime_host.hpp"
#include "edward/ai/ai_orchestrator.hpp"
#include "edward/ai/rule_file_loader.hpp"

#include <QVariantMap>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
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
#include <QDataStream>
#include <QFileDialog>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QStringList>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <limits>

namespace edward::desktop {

namespace {
QString previewSettingsPath() {
  const auto overridePath = qEnvironmentVariable("EDWARD_SETTINGS_PATH");
  if (!overridePath.isEmpty()) return overridePath;
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
         QStringLiteral("/preview-storage.ini");
}

QString nativeRuntimeResourceId(const edward::core::NativeRuntimeComponent& component) {
  const auto source = component.packageRoot + QLatin1Char('|') + component.manifestPath;
  return QStringLiteral("runtime.") + QString::fromLatin1(
      QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

qint64 nativeAiProjectRevision(const edward::core::TimelineSnapshot& snapshot) {
  QByteArray state;
  state.append(QByteArray::number(snapshot.playheadFrame));
  for (const auto& clip : snapshot.clips) {
    state.append('|').append(QByteArray::number(clip.id));
    state.append(':').append(QByteArray::number(clip.trackId));
    state.append(':').append(QByteArray::number(clip.timelineStart));
    state.append(':').append(QByteArray::number(clip.sourceIn));
    state.append(':').append(QByteArray::number(clip.sourceOut));
    if (clip.nativeRuntime) {
      state.append(':').append(clip.nativeRuntime->runtime.toUtf8());
      state.append(':').append(QJsonDocument(clip.nativeRuntime->props).toJson(QJsonDocument::Compact));
    }
  }
  const auto digest = QCryptographicHash::hash(state, QCryptographicHash::Sha256);
  qint64 revision = 0;
  for (int index = 0; index < 8; ++index)
    revision = (revision << 8) | static_cast<unsigned char>(digest.at(index));
  return revision & std::numeric_limits<qint64>::max();
}

QString telemetryInstallationId() {
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  auto identifier = settings.value(QStringLiteral("diagnostics/installationId")).toString().trimmed();
  if (!identifier.isEmpty()) return identifier;
  identifier = QUuid::createUuid().toString(QUuid::WithoutBraces);
  settings.setValue(QStringLiteral("diagnostics/installationId"), identifier);
  settings.sync();
  return settings.status() == QSettings::NoError ? identifier : QString{};
}

QString normalizeAiAssetPath(const QJsonObject& object, const QString& prompt) {
  const QStringList keys{QStringLiteral("asset_path"), QStringLiteral("file_path"), QStringLiteral("media_path"),
                         QStringLiteral("path"), QStringLiteral("file"), QStringLiteral("url")};
  for (const auto& key : keys) {
    const auto value = object.value(key).toString().trimmed();
    if (!value.isEmpty()) return QUrl(value).isLocalFile() ? QUrl(value).toLocalFile() : value;
  }
  static const QRegularExpression pathPattern(QStringLiteral("(?:file://)?/[^\\s\\\"']+\\.(?:png|jpe?g|webp|gif|mp4|mov|mkv|mp3|wav|m4a)"), QRegularExpression::CaseInsensitiveOption);
  const auto match = pathPattern.match(prompt);
  return match.hasMatch() ? match.captured(0) : QString{};
}

QStringList attachmentPathsFromPrompt(const QString& prompt) {
  QStringList paths;
  static const QRegularExpression pattern(QStringLiteral("(?:file://)?/[^\\s]+\\.(?:png|jpe?g|webp|gif|mp4|mov|mkv|mp3|wav|m4a|txt|md|json|csv)"), QRegularExpression::CaseInsensitiveOption);
  auto match = pattern.globalMatch(prompt);
  while (match.hasNext()) {
    const auto value = match.next().captured(0);
    if (!paths.contains(value)) paths.append(value);
  }
  return paths;
}

QJsonObject normalizeComponentResponse(QJsonObject object) {
  if (!object.value(QStringLiteral("root")).isUndefined()) return object;
  const auto legacyNodes = object.value(QStringLiteral("nodes")).toArray();
  if (legacyNodes.isEmpty()) return object;
  const auto normalizeNode = [](const auto& self, QJsonObject node) -> QJsonObject {
    QJsonObject transform = node.value(QStringLiteral("transform")).toObject();
    QJsonObject properties = node.value(QStringLiteral("properties")).toObject();
    for (const auto& key : {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height")}) {
      if (node.contains(key)) { transform.insert(key, node.value(key)); node.remove(key); }
    }
    for (auto it = node.constBegin(); it != node.constEnd(); ++it) {
      if (it.key() != QStringLiteral("id") && it.key() != QStringLiteral("type") &&
          it.key() != QStringLiteral("children") && it.key() != QStringLiteral("keyframes") &&
          it.key() != QStringLiteral("transform") && it.key() != QStringLiteral("properties"))
        properties.insert(it.key(), it.value());
    }
    if (!transform.isEmpty()) node.insert(QStringLiteral("transform"), transform);
    if (!properties.isEmpty()) node.insert(QStringLiteral("properties"), properties);
    const auto children = node.value(QStringLiteral("children")).toArray();
    if (!children.isEmpty()) {
      QJsonArray normalizedChildren;
      for (const auto& child : children) normalizedChildren.append(self(self, child.toObject()));
      node.insert(QStringLiteral("children"), normalizedChildren);
    }
    const auto legacyKeyframes = node.value(QStringLiteral("keyframes"));
    if (legacyKeyframes.isArray()) {
      QJsonObject grouped;
      for (const auto& value : legacyKeyframes.toArray()) {
        const auto point = value.toObject();
        const auto property = point.value(QStringLiteral("property")).toString();
        if (property.isEmpty() || !point.value(QStringLiteral("value")).isDouble()) continue;
        auto points = grouped.value(property).toArray();
        points.append(QJsonObject{{QStringLiteral("frame"), point.value(QStringLiteral("time"))},
                                  {QStringLiteral("value"), point.value(QStringLiteral("value"))}});
        grouped.insert(property, points);
      }
      node.insert(QStringLiteral("keyframes"), grouped);
    }
    return node;
  };
  QJsonArray children;
  for (const auto& value : legacyNodes) children.append(normalizeNode(normalizeNode, value.toObject()));
  object.remove(QStringLiteral("nodes"));
  object.insert(QStringLiteral("root"), QJsonObject{{QStringLiteral("id"), object.value(QStringLiteral("name")).toString(QStringLiteral("root"))},
                                                     {QStringLiteral("type"), QStringLiteral("container")},
                                                     {QStringLiteral("children"), children}});
  return object;
}

QString promptWithTextAttachmentContents(const QString& prompt) {
  QString expanded = prompt;
  const auto paths = attachmentPathsFromPrompt(prompt);
  constexpr qsizetype maxAttachmentBytes = 192 * 1024;
  for (const auto& rawPath : paths) {
    const auto path = QUrl(rawPath).isLocalFile() ? QUrl(rawPath).toLocalFile() : rawPath;
    const auto suffix = QFileInfo(path).suffix().toLower();
    if (suffix != QStringLiteral("txt") && suffix != QStringLiteral("md") && suffix != QStringLiteral("json") &&
        suffix != QStringLiteral("csv"))
      continue;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    const auto content = file.read(maxAttachmentBytes + 1);
    if (content.size() > maxAttachmentBytes) continue;
    expanded += QStringLiteral("\n\n[文本附件内容: %1]\n`````\n%2\n`````\n")
                    .arg(QFileInfo(path).fileName(), QString::fromUtf8(content));
  }
  return expanded;
}

QString fusionConversionFailureMessage(const edward::core::ComponentConversionReport& report) {
  QStringList entries;
  for (const auto& unsupported : report.unsupported) {
    entries.append(QStringLiteral("节点 %1 的 %2 不支持：%3")
                       .arg(unsupported.nodeId, unsupported.field, unsupported.reason));
  }
  return QStringLiteral("组件无法转换为 Fusion：%1").arg(entries.join(QStringLiteral("；")));
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
    if (clip.nativeRuntime) {
      entry.insert(QStringLiteral("nativeRuntime"), QJsonObject{
          {QStringLiteral("packageRoot"), clip.nativeRuntime->packageRoot},
          {QStringLiteral("manifestPath"), clip.nativeRuntime->manifestPath},
          {QStringLiteral("runtime"), clip.nativeRuntime->runtime},
          {QStringLiteral("props"), clip.nativeRuntime->props}});
    }
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

struct ComponentImageSource final {
  QString nodeId;
  QString path;
};

QVector<ComponentImageSource> imageSources(const edward::core::ComponentIr& component) {
  QVector<ComponentImageSource> sources;
  const auto visit = [&sources](const auto& self, const QJsonObject& node) -> void {
    if (node.value(QStringLiteral("type")).toString() == QStringLiteral("image")) {
      const auto properties = node.value(QStringLiteral("properties")).toObject();
      // SVG assets are Fusion Loader layers, not Resolve timeline media. They
      // must stay in the single Fusion component route; treating them as
      // external image sources incorrectly activates the multi-media overlay
      // path and rejects valid sibling layers.
      if (properties.contains(QStringLiteral("svgAttributes"))) {
        for (const auto& child : node.value(QStringLiteral("children")).toArray()) {
          if (child.isObject()) self(self, child.toObject());
        }
        return;
      }
      const auto path = properties
                            .value(QStringLiteral("src")).toString().trimmed();
      const auto nodeId = node.value(QStringLiteral("id")).toString().trimmed();
      if (!nodeId.isEmpty() && !path.isEmpty() && QFileInfo::exists(path))
        sources.push_back({nodeId, path});
    }
    for (const auto& child : node.value(QStringLiteral("children")).toArray()) {
      if (child.isObject()) self(self, child.toObject());
    }
  };
  visit(visit, component.toJson().value(QStringLiteral("root")).toObject());
  return sources;
}

QSet<QString> imageLayerNodeIds(const QJsonObject& imageNode) {
  QSet<QString> ids;
  const auto visit = [&ids](const auto& self, const QJsonObject& node) -> void {
    const auto id = node.value(QStringLiteral("id")).toString().trimmed();
    if (!id.isEmpty()) ids.insert(id);
    for (const auto& child : node.value(QStringLiteral("children")).toArray()) {
      if (child.isObject()) self(self, child.toObject());
    }
  };
  visit(visit, imageNode);
  return ids;
}

bool extractSingleTextNode(const edward::core::ComponentNode& node, QString* text, QString* font) {
  if (node.type == edward::core::ComponentNodeType::Text) {
    if (!text || !text->isEmpty()) return false;
    *text = node.properties.value(QStringLiteral("text")).toString().trimmed();
    if (font) *font = node.properties.value(QStringLiteral("fontFamily")).toString().trimmed();
    return !text->isEmpty();
  }
  if (node.type != edward::core::ComponentNodeType::Container) return false;
  for (const auto& child : node.children) {
    if (!extractSingleTextNode(child, text, font)) return false;
  }
  return true;
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

void setTransformStatic(edward::core::ComponentIr& component, const QString& nodeId,
                        const QString& field, double value) {
  edward::core::ComponentEditCommand::apply(
      component, {edward::core::ComponentEditKind::SetTransformNumber, nodeId, field, 0, value});
}

void setPropertyStatic(edward::core::ComponentIr& component, const QString& nodeId,
                       const QString& field, const QJsonValue& value) {
  edward::core::ComponentEditCommand::apply(
      component, {edward::core::ComponentEditKind::SetProperty, nodeId, field, 0, value});
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
      preferenceStore_(this),
      previewStorageRoots_(defaultPreviewStorageRoots()),
      previewSession_(previewStorageRoots_, projectIdentity_),
      previewFrameCache_(previewStorageRoots_, projectIdentity_),
      renderGraph_(mltAdapter_),
      resolveConnection_(),
      resolveAdapter_(resolveConnection_),
      resolveApiManuals_(edward::resources::ResolveApiManuals::load()) {
  {
    QSettings settings(previewSettingsPath(), QSettings::IniFormat);
    aiModelEndpoint_ = settings.value(QStringLiteral("ai/endpoint")).toString();
    aiProviderName_ = settings.value(QStringLiteral("ai/provider")).toString();
    aiModelApiKey_ = settings.value(QStringLiteral("ai/apiKey")).toString();
    aiModelId_ = settings.value(QStringLiteral("ai/model")).toString();
    aiTestModel_ = settings.value(QStringLiteral("ai/testModel")).toString();
    aiModelList_ = settings.value(QStringLiteral("ai/modelList")).toString();
    aiContextWindow_ = settings.value(QStringLiteral("ai/contextWindow")).toString();
    aiConversation_ = settings.value(QStringLiteral("ai/conversation")).toString();
    QJsonParseError insertionParseError;
    const auto insertionDocument = QJsonDocument::fromJson(settings.value(QStringLiteral("ai/lastInsertionRecords")).toByteArray(), &insertionParseError);
    if (insertionDocument.isArray()) aiLastInsertionRecords_ = insertionDocument.array();
    aiAvailableModels_ = aiModelList_.split(QRegularExpression(QStringLiteral("[\\r\\n,]+")), Qt::SkipEmptyParts);
    aiAvailableModels_.replaceInStrings(QRegularExpression(QStringLiteral("^\\s+|\\s+$")), QString());
    aiAvailableModels_.removeAll(QString());
    aiAvailableModels_.removeDuplicates();
    if (!aiModelId_.trimmed().isEmpty() && !aiAvailableModels_.contains(aiModelId_.trimmed()))
      aiAvailableModels_.prepend(aiModelId_.trimmed());
  }
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
  resolveContextPollTimer_.setInterval(1000);
  connect(&resolveContextPollTimer_, &QTimer::timeout, this, [this] {
    if (resolveConnected_) refreshResolveTimeline();
    else resolveContextPollTimer_.stop();
  });
  resolveReconnectTimer_.setInterval(2000);
  connect(&resolveReconnectTimer_, &QTimer::timeout, this, [this] {
    if (resolveConnected_) {
      resolveReconnectTimer_.stop();
      return;
    }
    connectResolve(false);
  });
  // 心跳持续维护 Resolve 连接：即使上下文读取失败，也立即进入重连，而非等待用户手动点击。
  resolveReconnectTimer_.start();
  projectAutosaveTimer_.setSingleShot(true);
  projectAutosaveTimer_.setInterval(1000);
  connect(&projectAutosaveTimer_, &QTimer::timeout, this, &WorkbenchRuntime::saveProjectRecovery);
  preferenceIdleTimer_.setSingleShot(true);
  preferenceIdleTimer_.setInterval(300000);
  connect(&preferenceIdleTimer_, &QTimer::timeout, this, [this] {
    preferenceStore_.flushPendingPreferences();
    preferenceStore_.compilePreferences();
  });
  preferenceIdleTimer_.start();
  connect(&previewProxyWatcher_, &QFutureWatcher<bool>::finished, this, [this] {
    previewProxyBusy_ = false;
    emit timelineChanged();
  });
  connect(this, &WorkbenchRuntime::timelineChanged, this, &WorkbenchRuntime::scheduleProjectAutosave);
  connect(this, &WorkbenchRuntime::timelineChanged, this, [this] {
    preferenceIdleTimer_.start();
  });
  connect(this, &WorkbenchRuntime::timelineChanged, this, [this] {
    QSettings settings(previewSettingsPath(), QSettings::IniFormat);
    const auto conversation = aiConversation_.size() > 100000 ? aiConversation_.right(100000) : aiConversation_;
    settings.setValue(QStringLiteral("ai/conversation"), conversation);
    settings.sync();
  });
  connect(&sessions_, &edward::resources::AuthSessionStore::changed, this,
          &WorkbenchRuntime::timelineChanged);
  connect(&preferenceSyncClient_, &edward::resources::PreferenceSyncClient::uploadCompleted, this,
          [this](bool success, const QString& message) { success ? emit operationSucceeded(message) : emit operationFailed(message); });
  connect(&preferenceSyncClient_, &edward::resources::PreferenceSyncClient::downloadCompleted, this,
          [this](bool success, const QVariantList& facts, const QString& message) {
            if (success) {
              if (!preferenceStore_.importFacts(facts)) { emit operationFailed(QStringLiteral("偏好下载后校验失败")); return; }
            }
            success ? emit operationSucceeded(message) : emit operationFailed(message);
          });
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
  connect(&authClient_, &edward::resources::SupabaseAuthClient::entitlementCompleted, this,
          [this](bool success, const QString& status, const QString& expiresAt, qint64 credits) {
            if (success) { subscriptionStatus_ = status; subscriptionExpiresAt_ = expiresAt; subscriptionCredits_ = credits; }
            emit timelineChanged();
          });
  connect(&authClient_, &edward::resources::SupabaseAuthClient::paymentCompleted, this,
          [this](bool success, const QString& orderId, const QString& qrCode, const QString& message) {
            paymentBusy_ = false;
            if (success) { paymentQrCode_ = qrCode; paymentOrderId_ = orderId; }
            success ? emit operationSucceeded(message) : emit operationFailed(message);
            emit timelineChanged();
          });
  connect(&authClient_, &edward::resources::SupabaseAuthClient::paymentStatus, this,
          [this](const QString&, const QString& status) { if (status == QStringLiteral("paid")) { paymentBusy_ = false; paymentQrCode_.clear(); emit operationSucceeded(QStringLiteral("支付宝支付成功，订阅权限已开通")); emit timelineChanged(); } });
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
  connect(&fusionArtifactCache_, &edward::resources::FusionArtifactCache::completed, this,
          [this](bool success, const QString&, const QString& message) {
            if (success)
              emit operationSucceeded(message);
            else
              emit operationFailed(message);
            emit timelineChanged();
          });
  connect(&modelChatClient_, &edward::resources::ModelChatClient::completed, this,
          [this](bool success, const QString& result) {
            aiRequestBusy_ = false;
            const auto clearProcessingMessage = [this] {
              aiConversation_.replace(QStringLiteral("\nAI：正在处理..."), QString());
              if (aiConversation_ == QStringLiteral("AI：正在处理...")) aiConversation_.clear();
            };
            const auto appendAiError = [this](const QString& message) {
              if (!pendingAiPrompt_.isEmpty()) {
                if (!aiConversation_.isEmpty()) aiConversation_ += QLatin1Char('\n');
                aiConversation_ += QStringLiteral("AI：") + message;
              }
              pendingAiAnalysis_ = false;
              pendingAiConversationOnly_ = false;
              pendingAiNativeProtocol_ = false;
              pendingAiActionPlan_.clear();
              pendingAiPrompt_.clear();
              emit timelineChanged();
            };
            if (!success) {
              clearProcessingMessage();
              appendAiError(QStringLiteral("AI 请求失败：%1").arg(result));
            } else if (pendingAiConversationOnly_) {
              clearProcessingMessage();
              pendingAiConversationOnly_ = false;
              if (!pendingAiPrompt_.isEmpty()) {
                if (!aiConversation_.isEmpty()) aiConversation_ += QLatin1Char('\n');
                aiConversation_ += QStringLiteral("AI：") + result.trimmed();
              }
              pendingAiPrompt_.clear();
              emit timelineChanged();
            } else if (pendingAiNativeProtocol_) {
              clearProcessingMessage();
              pendingAiNativeProtocol_ = false;
              const auto snapshot = timeline_.snapshot();
              QStringList targets;
              for (const auto& clip : snapshot.clips) targets.append(QString::number(clip.id));
              edward::ai::ProjectSnapshot project{nativeAiProjectRevision(snapshot), targets,
                                                  nativeRuntimePackages_.keys()};
              const auto routed = edward::ai::AiOrchestrator{}.handle(result, project);
              if (!aiConversation_.isEmpty()) aiConversation_ += QLatin1Char('\n');
              if (routed.kind == edward::ai::AiResult::Kind::ActionPlan && routed.plan) {
                pendingAiActionPlan_ = QString::fromUtf8(
                    QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), routed.plan->schemaVersion},
                                              {QStringLiteral("requestId"), routed.plan->requestId},
                                              {QStringLiteral("baseProjectRevision"), routed.plan->baseProjectRevision},
                                              {QStringLiteral("operations"), routed.plan->operations}})
                        .toJson(QJsonDocument::Compact));
                aiConversation_ += QStringLiteral("AI：已生成可执行项目修改方案。确认后将作为一个可撤销事务应用。");
              } else {
                aiConversation_ += QStringLiteral("AI：") + routed.text;
              }
              pendingAiPrompt_.clear();
              pendingAiAnalysis_ = false;
              emit timelineChanged();
            } else {
              clearProcessingMessage();
              appendAiError(QStringLiteral("AI 请求未通过 Edward 0.6.0 原生协议发送，未执行任何项目修改。"));
              pendingAiConversationOnly_ = false;
              pendingAiAnalysis_ = false;
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
  connect(&modelChatClient_, &edward::resources::ModelChatClient::modelsCompleted, this,
          [this](bool success, const QStringList& models, const QString& message) {
            aiModelListBusy_ = false;
            if (success) {
              aiAvailableModels_ = models;
              aiModelList_ = models.join(QStringLiteral("\n"));
              QSettings settings(previewSettingsPath(), QSettings::IniFormat);
              settings.setValue(QStringLiteral("ai/modelList"), aiModelList_);
              settings.sync();
              emit operationSucceeded(message);
            } else {
              emit operationFailed(QStringLiteral("模型列表获取失败：%1").arg(message));
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
  resolveRenderPollTimer_.setInterval(500);
  connect(&resolveRenderPollTimer_, &QTimer::timeout, this, [this] {
    if (!timelineExportBusy_ || resolveRenderJobId_.isEmpty()) {
      resolveRenderPollTimer_.stop();
      return;
    }
    QString error;
    const auto status = resolveAdapter_.renderStatus(resolveRenderJobId_, &error, resolveRenderOutputPath_);
    timelineExportProgress_ = status.progress;
    if (status.state == edward::resolve::ResolveRenderState::Completed) {
      resolveRenderPollTimer_.stop();
      timelineExportBusy_ = false;
      resolveRenderJobId_.clear();
      resolveRenderOutputPath_.clear();
      timelineExportProgress_ = 100;
      recordResolveCapabilityEvent(QStringLiteral("resolve.render.status"), true);
      emit operationSucceeded(QStringLiteral("Resolve 已完成导出：%1").arg(status.outputPath));
    } else if (status.state == edward::resolve::ResolveRenderState::Failed ||
               status.state == edward::resolve::ResolveRenderState::Canceled) {
      resolveRenderPollTimer_.stop();
      timelineExportBusy_ = false;
      resolveRenderJobId_.clear();
      resolveRenderOutputPath_.clear();
      timelineExportProgress_ = 0;
      recordResolveCapabilityEvent(
          QStringLiteral("resolve.render.status"), false,
          status.state == edward::resolve::ResolveRenderState::Canceled
              ? QStringLiteral("render_canceled")
              : (status.error.isEmpty() ? QStringLiteral("render_failed") : status.error));
      recordResolveError(QStringLiteral("render.status"),
                         status.state == edward::resolve::ResolveRenderState::Canceled
                             ? QStringLiteral("render_canceled")
                             : (status.error.isEmpty() ? QStringLiteral("render_failed") : status.error));
      emit operationFailed(status.error.isEmpty() ? QStringLiteral("Resolve 导出失败") : status.error);
    } else if (!error.isEmpty()) {
      resolveRenderPollTimer_.stop();
      timelineExportBusy_ = false;
      resolveRenderJobId_.clear();
      resolveRenderOutputPath_.clear();
      timelineExportProgress_ = 0;
      recordResolveCapabilityEvent(QStringLiteral("resolve.render.status"), false,
                                   QStringLiteral("render_status_failed"));
      recordResolveError(QStringLiteral("render.status"), QStringLiteral("render_status_failed"));
      emit operationFailed(error);
    }
    emit timelineChanged();
  });
}

void WorkbenchRuntime::appendAiConversationError(const QString& message) {
  const auto text = message.trimmed();
  if (text.isEmpty()) return;
  if (!aiConversation_.isEmpty()) aiConversation_ += QLatin1Char('\n');
  aiConversation_ += QStringLiteral("AI：") + text;
  emit timelineChanged();
}

void WorkbenchRuntime::assessResolveRequest(const QString& request) {
  const auto result = resolveApiManuals_.assess(request);
  resolveApiAssessment_ = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
  planResolveAction(request);
  emit timelineChanged();
}

void WorkbenchRuntime::planResolveAction(const QString& request) {
  // 规划前重新读取 Resolve，避免用户移动播放头或更换片段后仍使用旧上下文。
  if (resolveConnected_) refreshResolveTimeline();
  resolveApiAssessment_ = QString::fromUtf8(
      QJsonDocument(resolveApiManuals_.assess(request)).toJson(QJsonDocument::Compact));
  edward::resolve::ResolveProjectContext context;
  context.hasResolveConnection = resolveConnected_;
  context.hasCurrentProject = resolveConnected_ && resolveTimelineSnapshot_.has_value() &&
                              !resolveTimelineSnapshot_->projectName.isEmpty();
  context.hasTimeline = context.hasCurrentProject && !resolveTimelineSnapshot_->timelineName.isEmpty();
  context.hasPlayhead = context.hasTimeline;
  QString currentItemError;
  const auto currentItem = context.hasTimeline ? resolveAdapter_.currentItem(&currentItemError) : std::nullopt;
  context.hasCurrentClip = currentItem.has_value();
  context.currentClipId = currentItem ? currentItem->timelineItemId : QString();
  if (currentItem) {
    context.clipStartFrame = currentItem->startFrame;
    context.clipEndFrame = currentItem->endFrame;
    context.hasFusion = currentItem->fusionCompCount > 0;
  }
  if (resolveTimelineSnapshot_) context.playheadFrame = resolveTimelineSnapshot_->playheadFrame;
  if (resolveTimelineSnapshot_) {
    context.timelineStartFrame = resolveTimelineSnapshot_->timelineStartFrame;
    context.timelineEndFrame = resolveTimelineSnapshot_->timelineEndFrame;
    context.markInFrame = resolveTimelineSnapshot_->markInFrame;
    context.markOutFrame = resolveTimelineSnapshot_->markOutFrame;
  }
  const auto plan = edward::resolve::planResolveRequest(request, context);
  auto planJson = plan.toJson();
  QJsonParseError manualError;
  const auto manualDocument = QJsonDocument::fromJson(resolveApiAssessment_.toUtf8(), &manualError);
  if (manualError.error == QJsonParseError::NoError && manualDocument.isObject()) {
    const auto methods = manualDocument.object().value(QStringLiteral("methods"));
    if (methods.isArray() && !methods.toArray().isEmpty()) {
      planJson.insert(QStringLiteral("manualEvidence"), methods);
      planJson.insert(QStringLiteral("manualEvidencePriority"),
                      QStringLiteral("official_resolve_21.0.4_first_mcp_second"));
    }
  }
  resolveActionPlan_ = QString::fromUtf8(QJsonDocument(planJson).toJson(QJsonDocument::Compact));
  emit timelineChanged();
}

bool WorkbenchRuntime::executeResolveQuickAction(const QString& action) {
  const auto id = action.trimmed().toLower();
  QString request;
  if (id == QStringLiteral("delete")) request = QStringLiteral("删除当前片段");
  else if (id == QStringLiteral("ripple-delete")) request = QStringLiteral("波纹删除当前片段");
  else if (id == QStringLiteral("subtitle")) request = QStringLiteral("自动生成字幕");
  else if (id == QStringLiteral("fade-in")) request = QStringLiteral("将当前片段淡入");
  else if (id == QStringLiteral("fade-out")) request = QStringLiteral("将当前片段淡出");
  else if (id == QStringLiteral("keep-range")) request = QStringLiteral("保留当前范围");
  else if (id == QStringLiteral("add-marker")) request = QStringLiteral("在当前播放头添加重点标记");
  else if (id == QStringLiteral("clip-color")) request = QStringLiteral("将当前片段标为蓝色");
  else if (id == QStringLiteral("set-in")) request = QStringLiteral("设置入点");
  else if (id == QStringLiteral("set-out")) request = QStringLiteral("设置出点");
  else if (id == QStringLiteral("clear-io")) request = QStringLiteral("清除 I/O");
  else {
    emit operationFailed(QStringLiteral("未支持的 Resolve 快捷操作：%1").arg(action));
    return false;
  }
  if (!resolveConnected_) {
    emit operationFailed(QStringLiteral("请先连接 Resolve Studio"));
    return false;
  }
  planResolveAction(request);
  return executeResolveActionPlan();
}

bool WorkbenchRuntime::executeResolveActionPlan() {
  resolveAdapter_.clearLastWriteResult();
  resolveActionLastResult_.clear();
  if (!resolveConnected_) {
    recordResolveError(QStringLiteral("resolve.action_plan"), QStringLiteral("resolve_not_connected"));
    emit operationFailed(QStringLiteral("请先连接 Resolve Studio"));
    return false;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(resolveActionPlan_.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    recordResolveError(QStringLiteral("resolve.action_plan"), QStringLiteral("plan_invalid"));
    emit operationFailed(QStringLiteral("操作计划格式无效"));
    return false;
  }
  const auto plan = document.object();
  if (!plan.value(QStringLiteral("supported")).toBool()) {
    recordResolveError(QStringLiteral("resolve.action_plan"), QStringLiteral("plan_unsupported"));
    auto message = plan.value(QStringLiteral("error")).toString(QStringLiteral("当前操作不支持"));
    const auto fallbackReason = plan.value(QStringLiteral("fallbackReason")).toString().trimmed();
    if (!fallbackReason.isEmpty()) message += QStringLiteral("：%1").arg(fallbackReason);
    emit operationFailed(message);
    return false;
  }
  if (plan.value(QStringLiteral("requiresLogin")).toBool() && !authenticated()) {
    recordResolveError(QStringLiteral("resolve.action_plan"), QStringLiteral("login_required"));
    emit operationFailed(QStringLiteral("此操作需要先登录并配置云端能力"));
    return false;
  }
  if (plan.value(QStringLiteral("requiresCloud")).toBool() && !authenticated()) {
    recordResolveError(QStringLiteral("resolve.action_plan"), QStringLiteral("cloud_auth_required"));
    emit operationFailed(QStringLiteral("此操作需要已登录的云端服务"));
    return false;
  }
  const auto commands = plan.value(QStringLiteral("commands")).toArray();
  if (commands.isEmpty()) {
    recordResolveError(QStringLiteral("resolve.action_plan"), QStringLiteral("plan_empty"));
    emit operationFailed(QStringLiteral("操作计划没有可执行命令"));
    return false;
  }
  QString expectedTimelineItemId;
  QJsonObject readResult;
  for (const auto& raw : commands) {
    const auto method = raw.toObject().value(QStringLiteral("method")).toString();
    if (method == QStringLiteral("timeline.deleteCurrent") ||
        method == QStringLiteral("timeline.unlinkCurrent")) continue;
    const auto id = raw.toObject().value(QStringLiteral("params")).toObject()
                        .value(QStringLiteral("timelineItemId")).toString().trimmed();
    if (!id.isEmpty()) {
      expectedTimelineItemId = id;
      break;
    }
  }
  for (const auto& raw : commands) {
    const auto command = raw.toObject();
    const auto method = command.value(QStringLiteral("method")).toString();
    if (method != QStringLiteral("fusion.setInput") && method != QStringLiteral("fusion.addKeyframe") &&
        method != QStringLiteral("fusion.ensureOpacityGraph") &&
        method != QStringLiteral("fusion.ensureTransformGraph") &&
        method != QStringLiteral("fusion.addTextOverlay") &&
        method != QStringLiteral("fusion.addTextPositionKeyframe") &&
        method != QStringLiteral("timeline.createSubtitlesFromAudio") &&
        method != QStringLiteral("timeline.insertResolveTitle") &&
        method != QStringLiteral("timeline.deleteCurrent") &&
        method != QStringLiteral("timeline.unlinkCurrent") &&
        method != QStringLiteral("timeline.setMarkInOut") &&
        method != QStringLiteral("timeline.deleteMarkedRange") &&
        method != QStringLiteral("timeline.keepMarkedRange") &&
        method != QStringLiteral("timeline.addMarker") &&
        method != QStringLiteral("timeline.deleteMarker") &&
        method != QStringLiteral("timeline.deleteMarkersByColor") &&
        method != QStringLiteral("timeline.setCurrentFlag") &&
        method != QStringLiteral("timeline.setClipColor") &&
        method != QStringLiteral("timeline.snapshot") &&
        method != QStringLiteral("timeline.currentItem") &&
        method != QStringLiteral("resolve.capabilityMatrix") &&
        method != QStringLiteral("ui.openSimpleExport")) {
      emit operationFailed(QStringLiteral("暂不支持该操作计划命令"));
      recordResolveError(method, QStringLiteral("command_unsupported"));
      return false;
    }
    const auto params = command.value(QStringLiteral("params")).toObject();
    if (method == QStringLiteral("ui.openSimpleExport")) {
      exportDialogRequested_ = true;
      continue;
    }
    if (method == QStringLiteral("timeline.snapshot")) {
      QString error;
      const auto snapshot = resolveAdapter_.timelineSnapshot(&error);
      if (!snapshot) { recordResolveError(method, QStringLiteral("resolve_command_failed")); emit operationFailed(QStringLiteral("读取 Resolve 时间线标记失败：%1").arg(error)); return false; }
      QJsonObject markers = snapshot->markers;
      readResult.insert(QStringLiteral("markers"), markers);
      readResult.insert(QStringLiteral("markerCount"), markers.size());
      continue;
    }
    if (method == QStringLiteral("timeline.currentItem")) {
      QString error;
      const auto item = resolveAdapter_.currentItem(&error);
      if (!item) { recordResolveError(method, QStringLiteral("resolve_command_failed")); emit operationFailed(QStringLiteral("读取当前片段状态失败：%1").arg(error)); return false; }
      readResult.insert(QStringLiteral("timelineItemId"), item->timelineItemId); readResult.insert(QStringLiteral("name"), item->name);
      readResult.insert(QStringLiteral("trackType"), item->trackType); readResult.insert(QStringLiteral("trackIndex"), item->trackIndex);
      readResult.insert(QStringLiteral("linkedCount"), item->linkedCount); readResult.insert(QStringLiteral("clipColor"), item->clipColor);
      QJsonArray flags;
      for (const auto& flag : item->flags) flags.append(flag);
      readResult.insert(QStringLiteral("flags"), flags);
      readResult.insert(QStringLiteral("selectedCount"), item->selectedCount);
      readResult.insert(QStringLiteral("fusionCompCount"), item->fusionCompCount);
      readResult.insert(QStringLiteral("selectionSource"), item->selectionSource); readResult.insert(QStringLiteral("startFrame"), item->startFrame); readResult.insert(QStringLiteral("endFrame"), item->endFrame);
      continue;
    }
    if (method == QStringLiteral("resolve.capabilityMatrix")) {
      QJsonArray entries;
      for (const auto& capability : resolve::ResolveComponentCapabilityMatrix::defaults()) {
        const auto status = capability.status == resolve::ResolveComponentStatus::Verified ? QStringLiteral("verified")
                           : capability.status == resolve::ResolveComponentStatus::CandidateVerified ? QStringLiteral("candidate_verified")
                           : capability.status == resolve::ResolveComponentStatus::Unverified ? QStringLiteral("unverified")
                           : capability.status == resolve::ResolveComponentStatus::Disabled ? QStringLiteral("disabled")
                           : QStringLiteral("unsupported");
        entries.append(QJsonObject{{QStringLiteral("id"), capability.id},
                                   {QStringLiteral("status"), status},
                                   {QStringLiteral("editableFields"), QJsonArray::fromStringList(capability.editableFields)},
                                   {QStringLiteral("keyframeFields"), QJsonArray::fromStringList(capability.keyframeFields)},
                                   {QStringLiteral("verifiedResolveVersion"), capability.verifiedResolveVersion},
                                   {QStringLiteral("fallbackReason"), capability.fallbackReason}});
      }
      readResult.insert(QStringLiteral("capabilities"), entries);
      readResult.insert(QStringLiteral("capabilityCount"), entries.size());
      continue;
    }
    if (method == QStringLiteral("timeline.createSubtitlesFromAudio")) {
      QString error;
      const resolve::ResolveSubtitleGenerationOptions options{
          params.value(QStringLiteral("language")).toString(), params.value(QStringLiteral("preset")).toString(),
          params.value(QStringLiteral("charsPerLine")).toInt(), params.value(QStringLiteral("lineBreak")).toString(),
          params.value(QStringLiteral("gap")).toInt()};
      if (!resolveAdapter_.createSubtitlesFromAudio(options, &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 自动字幕生成失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.insertResolveTitle")) {
      QString error;
      const auto insertMethod = params.value(QStringLiteral("insertMethod")).toString().trimmed().toLower();
      const auto name = params.value(QStringLiteral("name")).toString();
      const bool accepted = insertMethod == QStringLiteral("fusion")
                                ? resolveAdapter_.insertFusionTitle(name, &error)
                                : resolveAdapter_.insertResolveTitle(name, &error);
      if (!accepted) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        if (error == QStringLiteral("resolve_title_template_not_discovered") &&
            resolveAdapter_.lastWriteResult().value(QStringLiteral("templateRefreshRequired")).toBool()) {
          emit operationFailed(QStringLiteral("Resolve 已找到该 OGraf 模板文件，但当前进程尚未发现；请重启 Resolve 或刷新模板浏览器后重试"));
          return false;
        }
        emit operationFailed(QStringLiteral("Resolve 标题模板插入失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.deleteCurrent")) {
      QString error;
      if (!resolveAdapter_.deleteCurrentClip(params.value(QStringLiteral("ripple")).toBool(),
                                             params.value(QStringLiteral("timelineItemId")).toString(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 删除片段失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.unlinkCurrent")) {
      QString error;
      if (!resolveAdapter_.unlinkCurrentClip(params.value(QStringLiteral("timelineItemId")).toString(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 解除链接失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.setMarkInOut")) {
      QString error;
      const auto markParams = params;
      const bool ok = markParams.value(QStringLiteral("clear")).toBool()
                          ? resolveAdapter_.clearMarkInOut(&error)
                          : resolveAdapter_.setMarkInOut(markParams.value(QStringLiteral("inFrame")).toInt(),
                                                         markParams.value(QStringLiteral("outFrame")).toInt(), &error);
      if (!ok) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 设置 I/O 失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.deleteMarkedRange")) {
      QString error;
      if (!resolveAdapter_.deleteMarkedRange(params.value(QStringLiteral("startFrame")).toInt(),
                                              params.value(QStringLiteral("endFrame")).toInt(),
                                              params.value(QStringLiteral("ripple")).toBool(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 范围删除失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.keepMarkedRange")) {
      QString error;
      if (!resolveAdapter_.keepMarkedRange(params.value(QStringLiteral("startFrame")).toInt(), params.value(QStringLiteral("endFrame")).toInt(), params.value(QStringLiteral("ripple")).toBool(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 范围保留失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.addMarker")) {
      QString error;
      if (!resolveAdapter_.addMarker(params.value(QStringLiteral("frame")).toInt(), params.value(QStringLiteral("name")).toString(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed")); emit operationFailed(QStringLiteral("Resolve 添加标记失败：%1").arg(error)); return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.deleteMarker")) {
      QString error;
      if (!resolveAdapter_.deleteMarker(params.value(QStringLiteral("frame")).toInt(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed")); emit operationFailed(QStringLiteral("Resolve 删除标记失败：%1").arg(error)); return false;
      }
      continue;
    }
    if (method == QStringLiteral("timeline.deleteMarkersByColor")) {
      QString error;
      if (!resolveAdapter_.deleteMarkersByColor(params.value(QStringLiteral("color")).toString(), &error)) { recordResolveError(method, QStringLiteral("resolve_command_failed")); emit operationFailed(QStringLiteral("Resolve 批量删除标记失败：%1").arg(error)); return false; }
      continue;
    }
    if (method == QStringLiteral("timeline.setCurrentFlag")) {
      QString error;
      if (!resolveAdapter_.setCurrentFlag(params.value(QStringLiteral("color")).toString(), params.value(QStringLiteral("clear")).toBool(), params.value(QStringLiteral("timelineItemId")).toString(), &error)) { recordResolveError(method, QStringLiteral("resolve_command_failed")); emit operationFailed(QStringLiteral("Resolve 片段标记操作失败：%1").arg(error)); return false; }
      continue;
    }
    if (method == QStringLiteral("timeline.setClipColor")) {
      QString error;
      if (!resolveAdapter_.setClipColor(params.value(QStringLiteral("color")).toString(), params.value(QStringLiteral("timelineItemId")).toString(), &error)) { recordResolveError(method, QStringLiteral("resolve_command_failed")); emit operationFailed(QStringLiteral("Resolve 片段颜色修改失败：%1").arg(error)); return false; }
      continue;
    }
    if (method == QStringLiteral("fusion.ensureOpacityGraph")) {
      QString error;
      if (!resolveAdapter_.ensureFusionOpacityGraph(params.value(QStringLiteral("timelineItemId")).toString(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 操作执行失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("fusion.ensureTransformGraph")) {
      QString error;
      if (!resolveAdapter_.ensureFusionTransformGraph(params.value(QStringLiteral("timelineItemId")).toString(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 操作执行失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("fusion.addTextOverlay")) {
      QString error;
      if (!resolveAdapter_.addFusionTextOverlay(params.value(QStringLiteral("text")).toString(),
                                                 params.value(QStringLiteral("font")).toString(),
                                                 params.value(QStringLiteral("timelineItemId")).toString(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 操作执行失败：%1").arg(error));
        return false;
      }
      continue;
    }
    if (method == QStringLiteral("fusion.addTextPositionKeyframe")) {
      QString error;
      if (!resolveAdapter_.addFusionTextPositionKeyframe(
              params.value(QStringLiteral("frame")).toInt(-1),
              params.value(QStringLiteral("x")).toDouble(),
              params.value(QStringLiteral("y")).toDouble(),
              params.value(QStringLiteral("timelineItemId")).toString(), &error)) {
        recordResolveError(method, QStringLiteral("resolve_command_failed"));
        emit operationFailed(QStringLiteral("Resolve 操作执行失败：%1").arg(error));
        return false;
      }
      continue;
    }
    const auto toolName = params.value(QStringLiteral("toolName")).toString();
    const auto inputName = params.value(QStringLiteral("inputName")).toString();
    const auto timelineItemId = params.value(QStringLiteral("timelineItemId")).toString();
    if (toolName.isEmpty() || inputName.isEmpty() || !params.contains(QStringLiteral("value"))) {
      recordResolveError(method, QStringLiteral("params_invalid"));
      emit operationFailed(QStringLiteral("操作计划参数不完整"));
      return false;
    }
    QString error;
    const auto value = params.value(QStringLiteral("value"));
    bool accepted = false;
    if (method == QStringLiteral("fusion.addKeyframe")) {
      const auto frame = params.value(QStringLiteral("frame")).toInt(-1);
      if (frame < 0) {
        recordResolveError(method, QStringLiteral("frame_invalid"));
        emit operationFailed(QStringLiteral("关键帧帧号无效"));
        return false;
      }
      accepted = resolveAdapter_.addFusionKeyframe(toolName, inputName, frame, value, timelineItemId, &error);
    } else {
      accepted = resolve::resolvePropertyBinding(inputName).has_value()
                   ? resolveAdapter_.setFusionProperty(toolName, inputName, value, timelineItemId, &error)
                   : resolveAdapter_.setFusionInput(toolName, inputName, value, timelineItemId, &error);
    }
    if (!accepted) {
      recordResolveError(method, QStringLiteral("resolve_command_failed"));
      emit operationFailed(QStringLiteral("Resolve 操作执行失败：%1").arg(error));
      return false;
    }
  }
  // 执行后同步回读，确保侧栏显示的是 Resolve 实际接受后的上下文。
  refreshResolveTimeline();
  auto lastWriteResult = resolveAdapter_.lastWriteResult();
  if (!readResult.isEmpty()) lastWriteResult = readResult;
  QJsonObject executionAudit{
      {QStringLiteral("status"), QStringLiteral("applied")},
      {QStringLiteral("commandsApplied"), commands.size()},
      {QStringLiteral("lastWriteResult"), lastWriteResult},
      {QStringLiteral("playheadFrame"), resolveTimelineSnapshot_
                                            ? resolveTimelineSnapshot_->playheadFrame
                                            : -1}};
  if (!expectedTimelineItemId.isEmpty()) {
    QString verifyError;
    const auto verified = resolveAdapter_.currentItem(&verifyError);
    if (!verified || verified->timelineItemId != expectedTimelineItemId) {
      recordResolveError(QStringLiteral("resolve.action_plan"), QStringLiteral("timeline_item_verification_failed"));
      emit operationFailed(QStringLiteral("Resolve 操作已发送，但当前片段回读校验失败：%1")
                               .arg(verifyError.isEmpty() ? QStringLiteral("片段 ID 不一致") : verifyError));
      return false;
    }
    executionAudit.insert(QStringLiteral("verifiedTimelineItemId"), verified->timelineItemId);
    executionAudit.insert(QStringLiteral("verification"), QStringLiteral("current_item_id_match"));
  } else {
    executionAudit.insert(QStringLiteral("verification"), QStringLiteral("timeline_context_refreshed"));
  }
  resolveActionLastResult_ = QString::fromUtf8(QJsonDocument(executionAudit).toJson(QJsonDocument::Compact));
  auto auditedPlan = plan;
  auditedPlan.insert(QStringLiteral("execution"), executionAudit);
  resolveActionPlan_ = QString::fromUtf8(QJsonDocument(auditedPlan).toJson(QJsonDocument::Compact));
  if (!activeProjectPath_.isEmpty()) {
    QJsonArray irWrites;
    for (const auto& raw : commands) {
      const auto command = raw.toObject();
      const auto method = command.value(QStringLiteral("method")).toString();
      if (!method.startsWith(QStringLiteral("fusion."))) continue;
      irWrites.append(QJsonObject{{QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                                  {QStringLiteral("method"), method},
                                  {QStringLiteral("params"), command.value(QStringLiteral("params"))},
                                  {QStringLiteral("status"), QStringLiteral("applied")},
                                  {QStringLiteral("execution"), executionAudit}});
    }
    if (!irWrites.isEmpty()) {
      const auto registerPath = activeProjectPath_ + QStringLiteral(".resolve-register.json");
      QJsonObject registerObject;
      QFile registerInput(registerPath);
      if (registerInput.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError registerError;
        const auto registerDocument = QJsonDocument::fromJson(registerInput.readAll(), &registerError);
        if (registerDocument.isObject()) registerObject = registerDocument.object();
      }
      auto history = registerObject.value(QStringLiteral("irOperations")).toArray();
      for (const auto& write : irWrites) history.append(write);
      registerObject.insert(QStringLiteral("irOperations"), history);
      registerObject.insert(QStringLiteral("lastUpdated"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
      QSaveFile registerOutput(registerPath);
      if (registerOutput.open(QIODevice::WriteOnly | QIODevice::Text)) {
        registerOutput.write(QJsonDocument(registerObject).toJson(QJsonDocument::Indented));
        registerOutput.commit();
      }
    }
  }
  const auto firstMethod = commands.first().toObject().value(QStringLiteral("method")).toString();
  recordResolveCapabilityEvent(firstMethod, true);
  if (firstMethod == QStringLiteral("ui.openSimpleExport")) {
    emit operationSucceeded(QStringLiteral("已打开 Edward 简化导出面板，请选择导出位置并确认"));
  } else if (firstMethod == QStringLiteral("timeline.createSubtitlesFromAudio")) {
    const auto createdTrackName = lastWriteResult.value(QStringLiteral("createdTrackName")).toString().trimmed();
    const auto createdTrackIndex = lastWriteResult.value(QStringLiteral("createdTrackIndex")).toInt();
    const auto trackLabel = !createdTrackName.isEmpty()
                              ? createdTrackName
                              : QStringLiteral("字幕轨 %1").arg(createdTrackIndex);
    emit operationSucceeded(QStringLiteral("自动字幕已生成：Resolve 已新增原生%1，请在时间线确认内容")
                                .arg(trackLabel));
  } else {
    emit operationSucceeded(QStringLiteral("操作已发送并完成回读，请在 Resolve 原生预览中确认效果"));
  }
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::beginResolveRebuild() {
  if (!resolveConnected_) { emit operationFailed(QStringLiteral("请先连接 Resolve Studio")); return false; }
  if (!resolveRebuildTransactionId_.isEmpty() &&
      (resolveRebuildTransactionState_ == QStringLiteral("open") ||
       resolveRebuildTransactionState_ == QStringLiteral("validated"))) {
    emit operationFailed(QStringLiteral("已有未完成的 Resolve 重建事务，请先提交或回滚"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.beginRebuild(&error)) { emit operationFailed(QStringLiteral("无法开始 Resolve 重建事务：%1").arg(error)); return false; }
  const auto result = resolveAdapter_.lastWriteResult();
  resolveRebuildTransactionId_ = result.value(QStringLiteral("transactionId")).toString();
  resolveRebuildTransactionState_ = result.value(QStringLiteral("state")).toString(QStringLiteral("open"));
  emit timelineChanged();
  return !resolveRebuildTransactionId_.isEmpty();
}

bool WorkbenchRuntime::importResolveRebuildFile(const QString& path) {
  if (!resolveConnected_) {
    emit operationFailed(QStringLiteral("请先连接 Resolve Studio"));
    return false;
  }
  if (path.trimmed().isEmpty()) {
    emit operationFailed(QStringLiteral("重建文件路径不能为空"));
    return false;
  }
  if (!beginResolveRebuild()) return false;
  QString error;
  if (!resolveAdapter_.importTimelineFile(path, {}, &error)) {
    const auto importError = error.isEmpty() ? QStringLiteral("导入失败") : error;
    // 回滚失败也不覆盖原始导入错误；事务句柄会保留在 open 状态供用户重试。
    if (!resolveAdapter_.rollbackRebuild(resolveRebuildTransactionId_, &error)) {
      emit operationFailed(QStringLiteral("重建文件导入失败：%1；自动回滚失败：%2")
                               .arg(importError, error));
      return false;
    }
    resolveRebuildTransactionState_ = QStringLiteral("rolled_back");
    emit timelineChanged();
    emit operationFailed(QStringLiteral("重建文件导入失败，已回滚原时间线：%1").arg(importError));
    return false;
  }
  refreshResolveTimeline();
  emit operationSucceeded(QStringLiteral("重建文件已导入隔离时间线，请先校验，再确认提交"));
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::validateResolveRebuild(const QString& timelineName, const QString& renderJobId, const QString& outputPath) {
  if (resolveRebuildTransactionId_.isEmpty()) { emit operationFailed(QStringLiteral("没有打开的 Resolve 重建事务")); return false; }
  QString error;
  if (!resolveAdapter_.validateRebuild(resolveRebuildTransactionId_, timelineName, renderJobId, outputPath, &error)) {
    resolveRebuildTransactionState_ = QStringLiteral("open");
    emit operationFailed(QStringLiteral("Resolve 重建事务校验失败：%1").arg(error)); emit timelineChanged(); return false;
  }
  resolveRebuildTransactionState_ = QStringLiteral("validated"); emit timelineChanged(); return true;
}

bool WorkbenchRuntime::commitResolveRebuild() {
  if (resolveRebuildTransactionId_.isEmpty()) { emit operationFailed(QStringLiteral("没有打开的 Resolve 重建事务")); return false; }
  if (resolveRebuildTransactionState_ != QStringLiteral("validated")) {
    emit operationFailed(QStringLiteral("Resolve 重建事务尚未校验，不能提交"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.commitRebuild(resolveRebuildTransactionId_, &error)) { emit operationFailed(QStringLiteral("Resolve 重建事务未提交：%1").arg(error)); return false; }
  resolveRebuildTransactionState_ = QStringLiteral("committed"); emit operationSucceeded(QStringLiteral("Resolve 重建事务已提交")); emit timelineChanged(); return true;
}

bool WorkbenchRuntime::rollbackResolveRebuild() {
  if (resolveRebuildTransactionId_.isEmpty()) { emit operationFailed(QStringLiteral("没有打开的 Resolve 重建事务")); return false; }
  if (resolveRebuildTransactionState_ != QStringLiteral("open") &&
      resolveRebuildTransactionState_ != QStringLiteral("validated")) {
    emit operationFailed(QStringLiteral("Resolve 重建事务已结束，不能再次回滚"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.rollbackRebuild(resolveRebuildTransactionId_, &error)) { emit operationFailed(QStringLiteral("Resolve 重建事务回滚失败：%1").arg(error)); return false; }
  resolveRebuildTransactionState_ = QStringLiteral("rolled_back"); emit operationSucceeded(QStringLiteral("Resolve 重建事务已回滚到原时间线")); emit timelineChanged(); return true;
}

void WorkbenchRuntime::recordResolveCapabilityEvent(const QString& capabilityId, const bool success,
                                                    const QString& errorCode) {
  if (!qualityImprovementEnabled()) return;
  const auto root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const auto path = QDir(root).filePath(QStringLiteral("diagnostics/resolve-capabilities.jsonl"));
  const auto versionMatch = QRegularExpression(QStringLiteral("\\b\\d+\\.\\d+\\.\\d+\\b"))
                                .match(resolveStatus_);
  const auto resolveVersion = versionMatch.hasMatch() ? versionMatch.captured(0) : QStringLiteral("unknown");
  const auto event = edward::resolve::ResolveCapabilityTelemetry::makeEvent(
      capabilityId, success ? edward::resolve::ResolveCapabilityStatus::CandidateVerified
                            : edward::resolve::ResolveCapabilityStatus::Unverified,
      QCoreApplication::applicationVersion().isEmpty() ? QStringLiteral("0.4.0")
                                                        : QCoreApplication::applicationVersion(),
      resolveVersion, success, errorCode, telemetryInstallationId(), projectIdentity_.value());
  QString ignored;
  edward::resolve::ResolveCapabilityTelemetry::append(path, event, &ignored);
}

void WorkbenchRuntime::recordResolveError(const QString& method, const QString& errorCode) {
  const auto root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const auto path = QDir(root).filePath(QStringLiteral("diagnostics/resolve-errors.jsonl"));
  const auto versionMatch = QRegularExpression(QStringLiteral("\\b\\d+\\.\\d+\\.\\d+\\b"))
                                .match(resolveStatus_);
  const auto resolveVersion = versionMatch.hasMatch() ? versionMatch.captured(0) : QStringLiteral("unknown");
  QString ignored;
  DiagnosticsReporter::appendResolveError(
      path, method, errorCode,
      QCoreApplication::applicationVersion().isEmpty() ? QStringLiteral("0.4.0")
                                                        : QCoreApplication::applicationVersion(),
      resolveVersion, &ignored);
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

double WorkbenchRuntime::selectedComponentNodeScale() const {
  if (!demoOverlayIr_) return 1.0;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  return node ? node->value("transform").toObject().value("scale").toDouble(1.0) : 1.0;
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
      if (clip.nativeRuntime) {
        item.insert(QStringLiteral("runtime"), clip.nativeRuntime->runtime);
        item.insert(QStringLiteral("packageRoot"), clip.nativeRuntime->packageRoot);
      }
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

bool WorkbenchRuntime::qualityImprovementEnabled() const {
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  return settings.value(QStringLiteral("diagnostics/telemetryEnabled"), true).toBool();
}

bool WorkbenchRuntime::qualityImprovementNoticeRequired() const {
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  return !settings.value(QStringLiteral("diagnostics/telemetryNoticeAcknowledged"), false).toBool();
}

bool WorkbenchRuntime::setQualityImprovementEnabled(const bool enabled) {
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  settings.setValue(QStringLiteral("diagnostics/telemetryEnabled"), enabled);
  settings.sync();
  if (settings.status() != QSettings::NoError) return false;
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::acknowledgeQualityImprovementNotice() {
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  settings.setValue(QStringLiteral("diagnostics/telemetryNoticeAcknowledged"), true);
  settings.sync();
  if (settings.status() != QSettings::NoError) return false;
  emit timelineChanged();
  return true;
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
  if (resolveConnected_) {
    QString resolveError;
    if (!resolveAdapter_.insertMediaAtPlayhead(path, &resolveError)) {
      controller_.undo();
      emit operationFailed(QStringLiteral("Resolve 素材插入失败：%1").arg(resolveError));
      return false;
    }
  }
  if (const auto clip = timeline_.clip(controller_.selectedClip())) requestClipWaveform(*clip);
  if (const auto clip = timeline_.clip(controller_.selectedClip())) requestClipThumbnail(*clip);
  emit timelineChanged();
  if (resolveConnected_ && resolveAdapter_.lastMediaInsertConflictShifted()) {
    emit operationSucceeded(QStringLiteral("素材已加入时间线，并因 Resolve 时间线冲突顺延到第 %1 帧")
                                .arg(resolveAdapter_.lastMediaInsertActualFrame()));
  } else {
    emit operationSucceeded(QStringLiteral("素材已加入时间线"));
  }
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
      clip && clip->kind == edward::core::TimelineClipKind::Component && clip->nativeRuntime) {
    selectedNativeRuntime_ = *clip->nativeRuntime;
    demoOverlayIr_.reset();
    editingComponentClipId_ = clip->id;
    demoOverlayEnabled_ = false;
    refreshDemoOverlay();
  } else if (clip && clip->kind == edward::core::TimelineClipKind::Component && clip->component) {
    selectedNativeRuntime_.reset();
    demoOverlayIr_ = *clip->component;
    componentClipId_ = 0;
    editingComponentClipId_ = clip->id;
    demoOverlayEnabled_ = true;
    syncDemoOverlayProperties(demoOverlayIr_->toJson());
    refreshDemoOverlay();
  } else if (editingComponentClipId_ != 0) {
    selectedNativeRuntime_.reset();
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
  if (resolveConnected_) {
    if (resolveComponentImportBusy_) {
      emit operationFailed(QStringLiteral("组件正在导入 Resolve"));
      return false;
    }
    if (!demoOverlayIr_) {
      emit operationFailed(QStringLiteral("请先载入或生成组件"));
      return false;
    }
    resolveComponentNodeTimelineIds_.clear();
    QString error;
    const auto capabilities = resolveAdapter_.capabilities(&error);
    if (capabilities && capabilities->fusion) {
      const auto converted = edward::resolve::convertComponentToFusion(*demoOverlayIr_, *capabilities);
      if (!converted.report.complete()) {
        emit operationFailed(fusionConversionFailureMessage(converted.report));
        return false;
      }
      const auto sources = imageSources(*demoOverlayIr_);
      if (!sources.isEmpty()) {
        QSet<QString> imageNodeIds;
        for (const auto& source : sources) imageNodeIds.insert(source.nodeId);
        QHash<QString, QSet<QString>> sourceLayerIds;
        const auto root = demoOverlayIr_->toJson().value(QStringLiteral("root")).toObject();
        for (const auto& source : sources) {
          const auto imageNode = findNode(root, source.nodeId);
          if (imageNode) sourceLayerIds.insert(source.nodeId, imageLayerNodeIds(*imageNode));
        }
        if (sources.size() > 1) {
          for (const auto& binding : converted.bindings) {
            const auto object = binding.toObject();
            if (object.value(QStringLiteral("tool")).toString() == QStringLiteral("MediaImage")) continue;
            bool belongsToImageLayer = false;
            const auto bindingNodeId = object.value(QStringLiteral("nodeId")).toString();
            for (const auto& ids : sourceLayerIds) belongsToImageLayer = belongsToImageLayer || ids.contains(bindingNodeId);
            if (!belongsToImageLayer) {
              emit operationFailed(QStringLiteral("多图组件的文字和形状子节点必须归属于对应图片图层"));
              return false;
            }
          }
        }
        int recordFrame = -1;
        for (const auto& source : sources) {
          if (!resolveAdapter_.insertImageOverlayAtPlayhead(source.path, &error)) {
            emit operationFailed(QStringLiteral("图片组件插入 Resolve 失败：%1").arg(error));
            return false;
          }
          const auto imageReceipt = resolveAdapter_.lastWriteResult();
          const auto imageItems = imageReceipt.value(QStringLiteral("items")).toArray();
          const auto imageTimelineItemId = imageItems.isEmpty()
              ? QString()
              : imageItems.first().toObject().value(QStringLiteral("timelineItemId")).toString();
          if (imageTimelineItemId.isEmpty()) {
            emit operationFailed(QStringLiteral("图片组件插入 Resolve 失败：缺少时间线片段 ID"));
            return false;
          }
          const auto sourceRecordFrame = imageItems.isEmpty() ? -1 :
              imageItems.first().toObject().value(QStringLiteral("startFrame")).toInt(-1);
          if (recordFrame < 0) recordFrame = sourceRecordFrame;
          QJsonArray imageBindings;
          const auto layerIds = sourceLayerIds.value(source.nodeId);
          for (const auto& layerId : layerIds) {
            resolveComponentNodeTimelineIds_.insert(layerId, imageTimelineItemId);
          }
          for (const auto& binding : converted.bindings) {
            const auto object = binding.toObject();
            if (!layerIds.contains(object.value(QStringLiteral("nodeId")).toString()) ||
                object.value(QStringLiteral("tool")).toString() == QStringLiteral("MediaImage")) continue;
            imageBindings.append(binding);
          }
          if (!imageBindings.isEmpty() &&
              !resolveAdapter_.applyFusionBindings(imageBindings,
                                                   resolveAdapter_.lastInsertedComponentId(), &error)) {
            emit operationFailed(QStringLiteral("图片组件 Fusion 变换写入失败：%1").arg(error));
            return false;
          }
        }
        if (recordFrame < 0 || !resolveAdapter_.setPlayhead(recordFrame, &error)) {
          emit operationFailed(QStringLiteral("图片组件插入后无法恢复 Resolve 播放头：%1")
                                   .arg(error.isEmpty() ? QStringLiteral("缺少插入帧") : error));
          return false;
        }
        emit operationSucceeded(QStringLiteral("图片组件已作为独立媒体片段叠加到 Resolve 当前播放头"));
        refreshResolveTimeline();
        return true;
      }
      if (!resolveAdapter_.insertFusionComponentOverlay(converted.bindings, durationFrames, &error)) {
        emit operationFailed(QStringLiteral("Fusion 组件插入 Resolve 失败：%1").arg(error));
        return false;
      }
      // Resolve 21.0.4 may accept the component graph while pruning an
      // unreferenced border subgraph during InsertFusionCompositionIntoTimeline.
      // Re-apply each declared rectangle border through the verified style API
      // after the clip has a stable timelineItemId, then require its readback.
      const auto insertedItemId = resolveAdapter_.lastInsertedComponentId();
      const auto recordFrame = resolveAdapter_.lastWriteResult().value(QStringLiteral("recordFrame")).toInt(-1);
      for (const auto& rawBinding : converted.bindings) {
        const auto binding = rawBinding.toObject();
        if (binding.value(QStringLiteral("tool")).toString() != QStringLiteral("RectangleOverlay")) continue;
        const auto values = binding.value(QStringLiteral("values")).toObject();
        if (!values.value(QStringLiteral("borderColor")).isString() ||
            !values.value(QStringLiteral("borderWidth")).isDouble() ||
            values.value(QStringLiteral("borderWidth")).toDouble() <= 0) continue;
        if (!resolveAdapter_.setFusionRectangleStyle(
                binding.value(QStringLiteral("toolName")).toString(),
                values.value(QStringLiteral("borderColor")).toString(),
                values.value(QStringLiteral("borderWidth")).toDouble(), insertedItemId, &error)) {
          emit operationFailed(QStringLiteral("Fusion 组件边框写入 Resolve 失败：%1").arg(error));
          return false;
        }
      }
      if (recordFrame < 0 || !resolveAdapter_.setPlayhead(recordFrame, &error)) {
        // Resolve can reject a timecode immediately after extending an empty
        // timeline even though the clip is already committed. Do not roll
        // back a verified insertion; refresh the timeline and leave the
        // actual Resolve playhead untouched for the user.
        emit operationSucceeded(QStringLiteral("组件已写入 Resolve；播放头未能回到插入起点"));
      } else {
        emit operationSucceeded(QStringLiteral("组件已作为独立 Fusion 片段叠加到 Resolve 当前播放头"));
      }
      refreshResolveTimeline();
      return true;
    }

    emit operationFailed(QStringLiteral("当前 Resolve 未验证 Fusion 组件路径，组件未写入时间线"));
    return false;
  }
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

bool WorkbenchRuntime::addNativeRuntimePackage(const QString& packageRoot, const QJsonObject& props) {
  const QFileInfo root(packageRoot);
  const QFileInfo manifestFile(QDir(root.filePath()).filePath(QStringLiteral("manifest.json")));
  if (!root.isDir() || !manifestFile.isFile()) {
    emit operationFailed(QStringLiteral("原生运行时组件必须是包含 manifest.json 的目录"));
    return false;
  }
  QFile file(manifestFile.filePath());
  if (!file.open(QIODevice::ReadOnly)) {
    emit operationFailed(QStringLiteral("无法读取原生运行时组件 manifest"));
    return false;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  QString error;
  const auto manifest = document.isObject()
      ? edward::runtime::RuntimeManifest::parse(document.object(), &error)
      : std::nullopt;
  if (!manifest) {
    emit operationFailed(QStringLiteral("原生运行时组件 manifest 无效：%1")
                             .arg(error.isEmpty() ? parseError.errorString() : error));
    return false;
  }
  edward::core::NativeRuntimeComponent component{root.canonicalFilePath(), manifestFile.canonicalFilePath(),
                                                   manifest->runtime, props};
  if (!controller_.dropNativeRuntimeAtPlayhead(component, manifest->durationInFrames)) {
    emit operationFailed(QStringLiteral("原生运行时组件无法插入时间线"));
    return false;
  }
  selectedNativeRuntime_ = component;
  nativeRuntimePackages_.insert(nativeRuntimeResourceId(component), component);
  editingComponentClipId_ = controller_.selectedClip();
  demoOverlayIr_.reset();
  demoOverlayEnabled_ = false;
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("原生运行时组件已加入时间线"));
  return true;
}

bool WorkbenchRuntime::nativeRuntimeSelected() const { return selectedNativeRuntime_.has_value(); }
QJsonObject WorkbenchRuntime::nativeRuntimeProps() const {
  return selectedNativeRuntime_ ? selectedNativeRuntime_->props : QJsonObject{};
}
QString WorkbenchRuntime::nativeRuntimePreviewEntry() const {
  if (!selectedNativeRuntime_) return {};
  QFile file(selectedNativeRuntime_->manifestPath);
  if (!file.open(QIODevice::ReadOnly)) return {};
  const auto document = QJsonDocument::fromJson(file.readAll());
  const auto entry = document.object().value(QStringLiteral("previewEntry")).toString();
  return entry.isEmpty() ? QString{} : QDir(selectedNativeRuntime_->packageRoot).filePath(entry);
}

QJsonObject WorkbenchRuntime::nativeRuntimeHostMessage() const {
  if (!selectedNativeRuntime_ || editingComponentClipId_ == 0) return {};
  const auto clip = timeline_.clip(editingComponentClipId_);
  if (!clip || !clip->nativeRuntime) return {};
  QFile file(selectedNativeRuntime_->manifestPath);
  if (!file.open(QIODevice::ReadOnly)) return {};
  QString error;
  const auto manifest = edward::runtime::RuntimeManifest::parse(
      QJsonDocument::fromJson(file.readAll()).object(), &error);
  if (!manifest) return {};
  edward::runtime::WebRuntimeHost host;
  if (!host.mount(*manifest, selectedNativeRuntime_->packageRoot, clip->nativeRuntime->props).ok) return {};
  const auto localFrame = std::clamp<edward::core::Frame>(
      controller_.playheadFrame() - clip->timelineStart, 0, manifest->durationInFrames - 1);
  if (!host.setFrame(localFrame).ok) return {};
  auto message = host.message(QStringLiteral("setFrame"));
  message.insert(QStringLiteral("protocol"), QStringLiteral("edward.web-runtime.host-message.v1"));
  message.insert(QStringLiteral("clipId"), static_cast<qint64>(clip->id));
  message.insert(QStringLiteral("entryUrl"), QUrl::fromLocalFile(
      QDir(selectedNativeRuntime_->packageRoot).filePath(manifest->previewEntry)).toString());
  return message;
}

bool WorkbenchRuntime::setNativeRuntimeProps(const QJsonObject& props) {
  if (!selectedNativeRuntime_ || !controller_.setSelectedNativeRuntimeProps(props)) {
    emit operationFailed(QStringLiteral("请先选择原生运行时组件"));
    return false;
  }
  selectedNativeRuntime_->props = props;
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::applyPendingAiActionPlan() {
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(pendingAiActionPlan_.toUtf8(), &parseError);
  QString error;
  const auto plan = document.isObject()
      ? edward::ai::ActionPlan::parse(document.object(), &error)
      : std::nullopt;
  if (!plan) {
    emit operationFailed(QStringLiteral("AI 操作计划无效：%1")
                             .arg(error.isEmpty() ? parseError.errorString() : error));
    return false;
  }

  const auto before = timeline_.snapshot();
  QStringList targetIds;
  for (const auto& clip : before.clips) targetIds.append(QString::number(clip.id));
  edward::ai::ProjectSnapshot project{nativeAiProjectRevision(before), targetIds,
                                      nativeRuntimePackages_.keys()};
  if (!plan->validate(project, &error)) {
    emit operationFailed(QStringLiteral("AI 操作计划未执行：%1").arg(error));
    return false;
  }

  const auto selectedBefore = controller_.selectedClip();
  const auto rollback = [&] {
    timeline_.restore(before);
    if (selectedBefore != 0) controller_.selectClip(selectedBefore);
  };
  for (const auto& rawOperation : plan->operations) {
    const auto operation = rawOperation.toObject();
    const auto type = operation.value(QStringLiteral("type")).toString();
    const auto targetId = static_cast<edward::core::ClipId>(
        operation.value(QStringLiteral("targetId")).toString().toLongLong());
    if (type == QStringLiteral("insert_native_component")) {
      const auto component = nativeRuntimePackages_.value(operation.value(QStringLiteral("resourceId")).toString());
      QFile file(component.manifestPath);
      QString manifestError;
      const auto manifest = file.open(QIODevice::ReadOnly)
          ? edward::runtime::RuntimeManifest::parse(QJsonDocument::fromJson(file.readAll()).object(), &manifestError)
          : std::nullopt;
      if (!manifest || !controller_.dropNativeRuntimeAtPlayhead(component, manifest->durationInFrames)) {
        rollback();
        emit operationFailed(QStringLiteral("AI 原生组件插入失败：%1")
                                 .arg(manifestError.isEmpty() ? QStringLiteral("资源或时间线位置无效") : manifestError));
        return false;
      }
      continue;
    }
    const auto clip = timeline_.clip(targetId);
    if (!clip) {
      rollback();
      emit operationFailed(QStringLiteral("AI 操作目标已不存在"));
      return false;
    }
    if (type == QStringLiteral("remove_clip")) {
      if (!timeline_.removeClip(targetId)) {
        rollback();
        emit operationFailed(QStringLiteral("AI 无法删除目标片段"));
        return false;
      }
    } else if (type == QStringLiteral("set_component_props")) {
      if (!clip->nativeRuntime) {
        rollback();
        emit operationFailed(QStringLiteral("AI 属性修改仅适用于原生运行时组件"));
        return false;
      }
      auto updated = *clip;
      updated.nativeRuntime->props = operation.value(QStringLiteral("props")).toObject();
      if (!timeline_.replaceClip(targetId, std::move(updated))) {
        rollback();
        emit operationFailed(QStringLiteral("AI 无法更新组件属性"));
        return false;
      }
    } else if (type == QStringLiteral("move_clip")) {
      auto updated = *clip;
      updated.timelineStart = operation.value(QStringLiteral("timelineStart")).toInteger(-1);
      if (!timeline_.replaceClip(targetId, std::move(updated))) {
        rollback();
        emit operationFailed(QStringLiteral("AI 移动片段会产生冲突或越界"));
        return false;
      }
    } else if (type == QStringLiteral("resize_clip")) {
      const auto duration = operation.value(QStringLiteral("durationFrames")).toInteger(-1);
      if (duration <= 0) {
        rollback();
        emit operationFailed(QStringLiteral("AI 片段时长无效"));
        return false;
      }
      auto updated = *clip;
      updated.sourceOut = updated.sourceIn + duration;
      if (!timeline_.replaceClip(targetId, std::move(updated))) {
        rollback();
        emit operationFailed(QStringLiteral("AI 调整片段时长会产生冲突或越界"));
        return false;
      }
    } else {
      rollback();
      emit operationFailed(QStringLiteral("AI 操作需要明确确认或当前版本尚不可执行：%1").arg(type));
      return false;
    }
  }

  aiTimelineUndo_.push_back(before);
  while (aiTimelineUndo_.size() > 5) aiTimelineUndo_.pop_front();
  pendingAiActionPlan_.clear();
  selectedNativeRuntime_.reset();
  if (const auto selected = timeline_.clip(controller_.selectedClip()); selected && selected->nativeRuntime)
    selectedNativeRuntime_ = *selected->nativeRuntime;
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("AI 操作计划已作为一个事务应用，可撤销"));
  return true;
}

bool WorkbenchRuntime::undoLastAiAction() {
  if (aiTimelineUndo_.empty() || !timeline_.restore(aiTimelineUndo_.back())) {
    emit operationFailed(QStringLiteral("没有可撤销的 AI 项目操作"));
    return false;
  }
  aiTimelineUndo_.pop_back();
  selectedNativeRuntime_.reset();
  if (const auto selected = timeline_.clip(controller_.selectedClip()); selected && selected->nativeRuntime)
    selectedNativeRuntime_ = *selected->nativeRuntime;
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("已撤销最近一次 AI 项目操作"));
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
  if (aiRequestBusy_) {
    emit operationFailed(QStringLiteral("AI 请求未发送：上一个请求仍在处理中，请等待完成或重启请求。"));
    return false;
  }
  if (!prompt.trimmed().isEmpty()) {
    if (!aiConversation_.isEmpty()) aiConversation_ += QLatin1Char('\n');
    aiConversation_ += QStringLiteral("用户：") + prompt.trimmed();
    aiConversation_ += QStringLiteral("\nAI：正在处理...");
    emit timelineChanged();
  }
  // 所有后续校验失败都必须能关联到这次发送，不能等到网络请求前才保存。
  pendingAiPrompt_ = prompt;
  const auto effectiveEndpoint = endpoint.trimmed().isEmpty() ? aiModelEndpoint_ : endpoint.trimmed();
  const auto effectiveApiKey = apiKey.trimmed().isEmpty() ? aiModelApiKey_ : apiKey.trimmed();
  const auto effectiveModel = model.trimmed().isEmpty() ? aiModelId_ : model.trimmed();
  if (effectiveEndpoint.isEmpty() || effectiveApiKey.isEmpty() || effectiveModel.isEmpty()) {
    emit operationFailed(QStringLiteral("AI 请求失败：请先在设置中配置模型"));
    return false;
  }
  QString systemPrompt;
  QString contextualPrompt;
  const auto activeProject = QFileInfo(activeProjectPath_);
  const auto rulesRoot = activeProjectPath_.isEmpty() ? QDir::currentPath() : activeProject.absolutePath();
  const auto rulesTarget = activeProjectPath_.isEmpty() ? rulesRoot : activeProject.absoluteFilePath();
  const auto rules = edward::ai::RuleFileLoader{}.load(rulesRoot.toStdString(), rulesTarget.toStdString());
  const auto agentContext = rules.text.isEmpty() ? QString()
      : QStringLiteral("\n项目规则（按目录由外到内、同级 AGENTS.override.md > AGENTS.md > CLAUDE.md）：\n%1")
            .arg(rules.effectiveRules());
  const auto timeline = timeline_.snapshot();
  QStringList knownTargets;
  for (const auto& clip : timeline.clips) knownTargets.append(QString::number(clip.id));
  const auto revision = nativeAiProjectRevision(timeline);
  const auto resources = nativeRuntimePackages_.keys().join(QStringLiteral(", "));
  systemPrompt = QStringLiteral(
      "You are Edward, a concise Chinese video editing assistant. Reply with plain text for explanation or clarification. "
      "For one unambiguous project modification, return exactly one edward.action-plan.v1 JSON object with schemaVersion, requestId, baseProjectRevision and operations. "
      "Allowed executable operations are insert_native_component(resourceId), set_component_props(targetId, props), move_clip(targetId, timelineStart), resize_clip(targetId, durationFrames), and remove_clip(targetId). "
      "Use only listed verified native runtime resources. Edward supports react, html-css, svg and gsap package runtimes, but never accepts component code, Component IR, conversion instructions, paths, shell commands, Resolve, Fusion, Premiere or OpenShot operations. "
      "If the target, verified resource, position, duration, or requested modification is ambiguous, ask one concise clarification question. Do not use Markdown or code fences for JSON.");
  contextualPrompt = QStringLiteral("当前项目：%1\n项目修订：%2\n可选片段 ID：%3\n已验证原生运行时资源：%4\n历史对话：%5\n用户请求：%6")
                         .arg(projectWindowTitle(), QString::number(revision), knownTargets.join(QStringLiteral(", ")),
                              resources.isEmpty() ? QStringLiteral("无") : resources,
                              aiConversation_, promptWithTextAttachmentContents(prompt)) + agentContext;
  pendingAiConversationOnly_ = false;
  pendingAiAnalysis_ = false;
  pendingAiNativeProtocol_ = true;
  if (contextualPrompt.toUtf8().size() > 256 * 1024) {
    emit operationFailed(QStringLiteral("AI 请求失败：代码或上下文超过 256 KB 输入上限"));
    return false;
  }
  aiRequestBusy_ = true;
  emit timelineChanged();
  if (!modelChatClient_.request({effectiveEndpoint, effectiveApiKey, effectiveModel}, systemPrompt, contextualPrompt)) {
    aiRequestBusy_ = false;
    pendingAiAnalysis_ = false;
    emit timelineChanged();
    return false;
  }
  return true;
}

bool WorkbenchRuntime::deleteLastAiInsertions(int count, int index) {
  if (!resolveConnected_) {
    appendAiConversationError(QStringLiteral("删除失败：请先连接 Resolve Studio。"));
    return false;
  }
  if (aiLastInsertionRecords_.isEmpty()) {
    appendAiConversationError(QStringLiteral("没有可删除的最近一次 AI 插入记录。"));
    return false;
  }
  int deletedCount = 0;
  QStringList failures;
  QSet<int> selectedIndexes;
  if (index >= 0 && index < aiLastInsertionRecords_.size()) {
    selectedIndexes.insert(index);
  } else if (count < 0 || count >= aiLastInsertionRecords_.size()) {
    for (int i = 0; i < aiLastInsertionRecords_.size(); ++i) selectedIndexes.insert(i);
  } else {
    for (int i = aiLastInsertionRecords_.size() - count; i < aiLastInsertionRecords_.size(); ++i)
      selectedIndexes.insert(i);
  }
  for (int i = aiLastInsertionRecords_.size() - 1; i >= 0; --i) {
    if (!selectedIndexes.contains(i)) continue;
    const auto record = aiLastInsertionRecords_.at(i).toObject();
    QString error;
    bool deleted = false;
    const auto id = record.value(QStringLiteral("timelineItemId")).toString().trimmed();
    if (!id.isEmpty()) deleted = resolveAdapter_.deleteCurrentClip(false, id, &error);
    if (!deleted && error != QStringLiteral("bridge_not_connected")) {
      const auto track = record.value(QStringLiteral("trackIndex")).toInt(0);
      const auto start = record.value(QStringLiteral("startFrame")).toInt(-1);
      const auto end = record.value(QStringLiteral("endFrame")).toInt(-1);
      if (track > 0 && start >= 0) deleted = resolveAdapter_.deleteTimelineItemByPosition(track, start, end, &error);
    }
    if (deleted) {
      ++deletedCount;
    } else if (error == QStringLiteral("bridge_not_connected")) {
      // Resolve 可能已执行 DeleteClips，但回执在桥接断开时丢失；不要把这种情况报告成确定失败。
      ++deletedCount;
    } else {
      failures.append(error.isEmpty() ? QStringLiteral("未知错误") : error);
    }
  }
  QJsonArray remainingRecords;
  for (int i = 0; i < aiLastInsertionRecords_.size(); ++i)
    if (!selectedIndexes.contains(i)) remainingRecords.append(aiLastInsertionRecords_.at(i));
  aiLastInsertionRecords_ = remainingRecords;
  {
    QSettings settings(previewSettingsPath(), QSettings::IniFormat);
    if (aiLastInsertionRecords_.isEmpty())
      settings.remove(QStringLiteral("ai/lastInsertionRecords"));
    else
      settings.setValue(QStringLiteral("ai/lastInsertionRecords"), QJsonDocument(aiLastInsertionRecords_).toJson(QJsonDocument::Compact));
    settings.sync();
  }
  if (deletedCount > 0) {
    appendAiConversationError(QStringLiteral("已提交删除最近一次 AI 插入的 %1 个素材。%2")
                                  .arg(deletedCount)
                                  .arg(failures.isEmpty() ? QString() : QStringLiteral("仍有 %1 个未删除。 ").arg(failures.size())));
    refreshResolveTimeline();
    return failures.isEmpty();
  }
  appendAiConversationError(QStringLiteral("删除最近一次 AI 插入失败：%1").arg(failures.join(QStringLiteral("；"))));
  return false;
}

QString WorkbenchRuntime::pasteAiAttachment() {
  const auto *clipboard = QGuiApplication::clipboard();
  if (!clipboard) return {};
  const auto *mime = clipboard->mimeData();
  if (mime->hasUrls()) {
    const auto urls = mime->urls();
    if (!urls.isEmpty()) return urls.first().toString();
  }
  if (mime->hasImage()) {
    const auto image = qvariant_cast<QImage>(mime->imageData());
    if (!image.isNull()) {
      const auto dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../../ai-attachments"));
      QDir().mkpath(dir);
      const auto path = QDir(dir).filePath(QStringLiteral("clipboard-%1.png").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
      if (image.save(path, "PNG")) return QUrl::fromLocalFile(path).toString();
    }
  }
  if (mime->hasText()) return mime->text();
  return {};
}

QStringList WorkbenchRuntime::pasteAiAttachments() {
  const auto *clipboard = QGuiApplication::clipboard();
  if (!clipboard) return {};
  const auto *mime = clipboard->mimeData();
  QStringList result;
  if (mime->hasUrls()) {
    for (const auto& url : mime->urls()) result.append(url.toString());
    return result;
  }
  if (mime->hasText()) {
    const auto text = mime->text();
    constexpr qsizetype longTextThreshold = 16 * 1024;
    if (text.toUtf8().size() > longTextThreshold) {
      const auto dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../../ai-attachments"));
      QDir().mkpath(dir);
      const auto path = QDir(dir).filePath(QStringLiteral("clipboard-code-%1.txt").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
      QFile file(path);
      if (file.open(QIODevice::WriteOnly | QIODevice::Text) && file.write(text.toUtf8()) == text.toUtf8().size()) {
        result.append(QUrl::fromLocalFile(path).toString());
        return result;
      }
    }
  }
  const auto single = pasteAiAttachment();
  if (!single.isEmpty()) result.append(single);
  return result;
}

QStringList WorkbenchRuntime::chooseAiAttachments() {
  const auto urls = QFileDialog::getOpenFileUrls(nullptr, QStringLiteral("添加图片、视频、音频或文本"), QUrl(),
                                                   QStringLiteral("支持的文件 (*.png *.jpg *.jpeg *.webp *.gif *.mp4 *.mov *.mkv *.mp3 *.wav *.m4a *.txt *.md *.json *.csv);;所有文件 (*)"));
  QStringList result;
  for (const auto& url : urls) result.append(url.toString());
  return result;
}

bool WorkbenchRuntime::analyzeCurrentClipWithAi(const QString& prompt) {
  const auto request = prompt.trimmed().isEmpty()
                           ? QStringLiteral("分析当前选中的片段，并给出明确的 Edward 项目修改建议。")
                           : prompt.trimmed();
  return requestAiComponentDraft({}, {}, {}, request);
}

bool WorkbenchRuntime::configureAiModel(const QString& providerName, const QString& endpoint,
                                        const QString& apiKey, const QString& model,
                                        const QString& testModel, const QString& modelList,
                                        const QString& contextWindow) {
  const QUrl url(endpoint.trimmed());
  const auto effectiveApiKey = apiKey.trimmed().isEmpty() ? aiModelApiKey_ : apiKey.trimmed();
  if (!url.isValid() || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0 ||
      url.host().isEmpty() || effectiveApiKey.isEmpty() || model.trimmed().isEmpty()) {
    emit operationFailed(QStringLiteral("AI 模型配置无效：需要 HTTPS 端点、API Key 和模型 ID"));
    return false;
  }
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  settings.setValue(QStringLiteral("ai/provider"), providerName.trimmed());
  settings.setValue(QStringLiteral("ai/endpoint"), url.toString());
  settings.setValue(QStringLiteral("ai/apiKey"), effectiveApiKey);
  settings.setValue(QStringLiteral("ai/model"), model.trimmed());
  settings.setValue(QStringLiteral("ai/testModel"), testModel.trimmed());
  settings.setValue(QStringLiteral("ai/modelList"), modelList.trimmed());
  settings.setValue(QStringLiteral("ai/contextWindow"), contextWindow.trimmed());
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    emit operationFailed(QStringLiteral("AI 模型配置无法保存"));
    return false;
  }
  aiModelEndpoint_ = url.toString();
  aiProviderName_ = providerName.trimmed();
  aiModelApiKey_ = effectiveApiKey;
  aiModelId_ = model.trimmed();
  aiTestModel_ = testModel.trimmed();
  const auto suppliedModels = modelList.trimmed().split(QRegularExpression(QStringLiteral("[\\r\\n,]+")), Qt::SkipEmptyParts);
  const bool preserveFetchedModels = aiAvailableModels_.size() > suppliedModels.size() && aiAvailableModels_.size() > 1;
  if (!preserveFetchedModels) aiAvailableModels_ = suppliedModels;
  aiModelList_ = aiAvailableModels_.join(QStringLiteral("\n"));
  aiAvailableModels_.replaceInStrings(QRegularExpression(QStringLiteral("^\\s+|\\s+$")), QString());
  aiAvailableModels_.removeAll(QString());
  aiAvailableModels_.removeDuplicates();
  if (!aiModelId_.trimmed().isEmpty() && !aiAvailableModels_.contains(aiModelId_.trimmed()))
    aiAvailableModels_.prepend(aiModelId_.trimmed());
  aiModelList_ = aiAvailableModels_.join(QStringLiteral("\n"));
  settings.setValue(QStringLiteral("ai/modelList"), aiModelList_);
  settings.sync();
  aiContextWindow_ = contextWindow.trimmed();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("AI 模型配置已保存"));
  return true;
}

bool WorkbenchRuntime::refreshAiModelList() {
  if (aiModelListBusy_) return false;
  if (!aiModelCredentialsConfigured()) {
    emit operationFailed(QStringLiteral("请先保存供应商端点和 API Key，再获取模型列表"));
    return false;
  }
  aiModelListBusy_ = true;
  emit timelineChanged();
  if (!modelChatClient_.requestModels({aiModelEndpoint_, aiModelApiKey_, aiModelId_})) {
    aiModelListBusy_ = false;
    emit timelineChanged();
    return false;
  }
  return true;
}

bool WorkbenchRuntime::selectAiModel(const QString& model) {
  const auto selected = model.trimmed();
  if (selected.isEmpty()) return false;
  if (!aiAvailableModels_.isEmpty() && !aiAvailableModels_.contains(selected)) return false;
  aiModelId_ = selected;
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  settings.setValue(QStringLiteral("ai/model"), aiModelId_);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    emit operationFailed(QStringLiteral("AI 模型选择无法保存"));
    return false;
  }
  emit timelineChanged();
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
  aiComponentDraftIsAnalysis_ = false;
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("AI 草案已生成，确认后应用"));
  return true;
}

bool WorkbenchRuntime::applyPendingAiComponentCommand() {
  if (!aiComponentDraft_) {
    emit operationFailed(QStringLiteral("没有待应用的 AI 草案"));
    return false;
  }
  const auto draft = *aiComponentDraft_;
  demoOverlayIr_ = draft;
  aiComponentDraftIsAnalysis_ = false;
  aiComponentDraftJson_.clear();
  syncDemoOverlayProperties(demoOverlayIr_->toJson());
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("AI 草案已应用"));
  return true;
}

bool WorkbenchRuntime::applyAiAnalysisDraftToResolve(int durationFrames) {
  if (!aiComponentDraft_ || !aiComponentDraftIsAnalysis_) {
    emit operationFailed(QStringLiteral("没有可添加到 Resolve 的 AI 分析组件草案"));
    return false;
  }
  if (!resolveConnected_) {
    emit operationFailed(QStringLiteral("请先连接 Resolve Studio，再添加 AI 分析组件"));
    return false;
  }
  if (durationFrames <= 0) {
    emit operationFailed(QStringLiteral("AI 分析组件时长无效"));
    return false;
  }
  const auto draft = *aiComponentDraft_;
  demoOverlayIr_ = draft;
  aiComponentDraft_.reset();
  aiComponentDraftIsAnalysis_ = false;
  aiComponentDraftJson_.clear();
  componentClipId_ = 0;
  editingComponentClipId_ = 0;
  demoOverlayEnabled_ = true;
  syncDemoOverlayProperties(demoOverlayIr_->toJson());
  refreshDemoOverlay();
  if (!addCurrentComponentToTimeline(durationFrames)) {
    aiComponentDraft_ = draft;
    aiComponentDraftIsAnalysis_ = true;
    aiComponentDraftJson_ = QString::fromUtf8(QJsonDocument(draft.toJson()).toJson(QJsonDocument::Compact));
    emit timelineChanged();
    return false;
  }
  emit operationSucceeded(QStringLiteral("AI 分析组件已转写为 Fusion 并添加到 Resolve"));
  return true;
}

void WorkbenchRuntime::discardPendingAiComponentCommand() {
  if (!aiComponentDraft_) return;
  aiComponentDraft_.reset();
  aiComponentDraftIsAnalysis_ = false;
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
  pendingAiConversationOnly_ = false;
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
  pendingAiConversationOnly_ = false;
  syncDemoOverlayProperties(demoOverlayIr_->toJson());
  refreshDemoOverlay();
  emit timelineChanged();
  emit operationSucceeded(QStringLiteral("资源库组件已载入为独立实例"));
  return true;
}

bool WorkbenchRuntime::insertLibraryComponentAtPlayhead(const QString& resourceId, int durationFrames) {
  if (resourceId.trimmed().isEmpty() || durationFrames <= 0) {
    emit operationFailed(QStringLiteral("资源库组件参数无效"));
    return false;
  }
  QString packageError;
  const auto package = componentLibrary_.load(resourceId, &packageError);
  if (!package) {
    emit operationFailed(QStringLiteral("资源库组件无法读取：%1").arg(packageError));
    return false;
  }
  // 用户侧/本地组件主路径固定为 Component IR → Fusion 转写 → Resolve 回读。
  // 公共资源库的 fusionComp 下载→缓存→直接导入属于后续独立路径，当前不在此入口接入。
  if (!loadLibraryComponent(resourceId)) return false;
  if (!addCurrentComponentToTimeline(durationFrames)) return false;
  emit operationSucceeded(QStringLiteral("资源库组件已添加到当前播放头"));
  return true;
}

bool WorkbenchRuntime::downloadFusionArtifact(const QString& url, const QString& destination,
                                              const QString& sha256) {
  QString error;
  if (!fusionArtifactCache_.download(url, destination, sha256, &error)) {
    emit operationFailed(QStringLiteral("Fusion 公共组件下载失败：%1").arg(error));
    return false;
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

bool WorkbenchRuntime::signUpWithSupabase(const QString& projectUrl, const QString& anonKey,
                                          const QString& email, const QString& password) {
  if (signInBusy_) return false;
  signInBusy_ = true;
  emit timelineChanged();
  if (!authClient_.signUpWithPassword({projectUrl, anonKey}, email, password)) { signInBusy_ = false; emit timelineChanged(); return false; }
  return true;
}

bool WorkbenchRuntime::sendSupabasePasswordReset(const QString& projectUrl, const QString& anonKey, const QString& email) {
  if (signInBusy_) return false;
  signInBusy_ = true;
  return authClient_.sendPasswordReset({projectUrl, anonKey}, email);
}

bool WorkbenchRuntime::refreshSupabaseEntitlement(const QString& projectUrl, const QString& anonKey) {
  return authenticated() && authClient_.fetchEntitlement({projectUrl, anonKey}, sessions_.session());
}

bool WorkbenchRuntime::refreshSupabaseSession(const QString& projectUrl, const QString& anonKey) {
  return authenticated() && authClient_.refreshSession({projectUrl, anonKey}, &sessions_);
}

bool WorkbenchRuntime::createNativePayment(const QString& projectUrl, const QString& anonKey, double amount, const QString& goodsDesc) {
  if (!authenticated() || paymentBusy_) return false;
  paymentBusy_ = true; paymentQrCode_.clear(); emit timelineChanged();
  return authClient_.createNativePayment({projectUrl, anonKey}, sessions_.session(), amount, goodsDesc, QStringLiteral("wechat"));
}

bool WorkbenchRuntime::createAlipayNativePayment(const QString& projectUrl, const QString& anonKey, const QString& planKey) {
  if (!authenticated() || paymentBusy_) return false;
  paymentBusy_ = true; paymentQrCode_.clear(); emit timelineChanged();
  return authClient_.createAlipayPayment({projectUrl, anonKey}, sessions_.session(), planKey);
}

bool WorkbenchRuntime::queryAlipayOrder(const QString& projectUrl, const QString& anonKey, const QString& orderId) {
  return authenticated() && authClient_.queryAlipayOrder({projectUrl, anonKey}, sessions_.session(), orderId);
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
  return exportTimelineWithOptions(outputPath, 1920, 1080, 30, 0);
}

bool WorkbenchRuntime::flushPreferencesForProjectClose() {
  const auto result = preferenceStore_.flushPendingPreferences();
  if (!result) emit operationFailed(QStringLiteral("偏好数据写入失败，已继续使用当前项目"));
  return result;
}

bool WorkbenchRuntime::flushPreferencesForExport() {
  const auto result = preferenceStore_.flushPendingPreferences();
  if (!result) emit operationFailed(QStringLiteral("偏好数据写入失败，导出仍可继续"));
  return result;
}

bool WorkbenchRuntime::compilePreferencesNow() {
  const auto result = preferenceStore_.compilePreferences();
  if (!result) emit operationFailed(QStringLiteral("偏好方案收敛失败，已保留上一版方案"));
  return result;
}

QVariantMap WorkbenchRuntime::preferenceStoreStatus() const { return preferenceStore_.status(); }

bool WorkbenchRuntime::uploadPreferences(const QString& projectUrl, const QString& anonKey) {
  return preferenceSyncClient_.upload(projectUrl, anonKey, sessions_.session().accessToken,
                                       preferenceStore_.exportFacts());
}

bool WorkbenchRuntime::downloadPreferences(const QString& projectUrl, const QString& anonKey) {
  return preferenceSyncClient_.download(projectUrl, anonKey, sessions_.session().accessToken);
}

bool WorkbenchRuntime::exportPreferencesToFile(const QString& path) {
  if (path.trimmed().isEmpty()) return false;
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_5);
  stream << QByteArrayLiteral("EDWARD_PREFS") << quint32(1) << preferenceStore_.exportFacts();
  if (stream.status() != QDataStream::Ok || !file.commit()) return false;
  return true;
}

bool WorkbenchRuntime::importPreferencesFromFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return false;
  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_5);
  QByteArray magic;
  quint32 version = 0;
  QVariantList facts;
  stream >> magic >> version >> facts;
  if (stream.status() != QDataStream::Ok || magic != QByteArrayLiteral("EDWARD_PREFS") || version != 1) return false;
  return preferenceStore_.importFacts(facts);
}

void WorkbenchRuntime::setPendingFablecutExportPath(const QString& path) {
  pendingFablecutExportPath_ = path.trimmed();
  emit timelineChanged();
}

QString WorkbenchRuntime::savedExportOutputDirectory() const {
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  const auto path = settings.value(QStringLiteral("export/outputDirectory")).toString().trimmed();
  if (path.isEmpty()) return {};
  const QDir directory(path);
  return directory.exists() ? directory.absolutePath() : QString{};
}

void WorkbenchRuntime::setSavedExportOutputDirectory(const QString& path) {
  const auto trimmed = path.trimmed();
  if (trimmed.isEmpty()) return;
  const QDir directory(trimmed);
  if (!directory.exists()) return;
  QSettings settings(previewSettingsPath(), QSettings::IniFormat);
  settings.setValue(QStringLiteral("export/outputDirectory"), directory.absolutePath());
  settings.sync();
  emit timelineChanged();
}

bool WorkbenchRuntime::exportTimelineWithOptions(const QString& outputPath, int width, int height,
                                                 int fps, int quality) {
  flushPreferencesForExport();
  if (timelineExportBusy_) {
    emit operationFailed(QStringLiteral("视频导出正在执行"));
    return false;
  }
  if (outputPath.isEmpty()) {
    emit operationFailed(QStringLiteral("请选择导出路径"));
    return false;
  }
  if (width <= 0 || height <= 0 || fps <= 0 || fps > 60 || quality < 0 || quality > 2) {
    emit operationFailed(QStringLiteral("导出参数无效"));
    return false;
  }
  const auto path = std::filesystem::path(outputPath.toStdString());
  std::error_code pathError;
  if (!path.is_absolute() || path.filename().empty() ||
      !std::filesystem::is_directory(path.parent_path(), pathError)) {
    emit operationFailed(QStringLiteral("导出路径必须是现有目录中的视频文件"));
    return false;
  }
  if (path.extension() != ".mp4") {
    emit operationFailed(QStringLiteral("当前仅支持导出 MP4"));
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
  if (!timelineExportBusy_) return;
  if (!resolveRenderJobId_.isEmpty()) {
    QString error;
    if (!resolveAdapter_.cancelRender(resolveRenderJobId_, &error, resolveRenderOutputPath_) && !error.isEmpty())
      emit operationFailed(error);
    return;
  }
  if (!timelineExportCancel_) return;
  timelineExportCancel_->store(true);
}

void WorkbenchRuntime::clearComponentOverlay() {
  demoOverlayIr_.reset();
  componentClipId_ = 0;
  editingComponentClipId_ = 0;
  aiConversation_.clear();
  pendingAiPrompt_.clear();
  pendingAiConversationOnly_ = false;
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
  const auto text = value.left(120);
  if (resolveConnected_ && demoOverlayIr_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    const auto nodeId = editableTextNodeId(demoOverlayIr_->toJson(), selectedComponentNodeId_);
    if (nodeId.isEmpty()) {
      emit operationFailed(QStringLiteral("当前组件没有可编辑的文字节点"));
      return;
    }
    if (setSelectedComponentPropertyAtPlayhead(nodeId, QStringLiteral("text"), text))
      demoOverlayText_ = text;
    return;
  }
  demoOverlayText_ = text;
  if (demoOverlayIr_)
    setPropertyStatic(*demoOverlayIr_, editableTextNodeId(demoOverlayIr_->toJson(), selectedComponentNodeId_),
                      "text", demoOverlayText_);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayFontSize(int value) {
  if (!componentPlayheadIsEditable()) return;
  demoOverlayFontSize_ = std::max(8, std::min(value, 96));
  if (demoOverlayIr_)
    setPropertyStatic(*demoOverlayIr_, editableTextNodeId(demoOverlayIr_->toJson(), selectedComponentNodeId_),
                      "fontSize", demoOverlayFontSize_);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayBorderWidth(int value) {
  if (!componentPlayheadIsEditable()) return;
  const auto clamped = std::max(0, std::min(value, 32));
  if (resolveConnected_ && demoOverlayIr_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    const auto nodeId = editableShapeNodeId(demoOverlayIr_->toJson(), selectedComponentNodeId_);
    if (setSelectedComponentPropertyAtPlayhead(nodeId, QStringLiteral("borderWidth"), clamped)) {
      demoOverlayBorderWidth_ = clamped;
      emit timelineChanged();
    }
    return;
  }
  demoOverlayBorderWidth_ = clamped;
  if (demoOverlayIr_)
    demoOverlayIr_->setNodeProperty(
        editableShapeNodeId(demoOverlayIr_->toJson(), selectedComponentNodeId_),
        QStringLiteral("borderWidth"), demoOverlayBorderWidth_);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeX(int value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("x"),
                                           std::max(-640, std::min(value, 640)));
    return;
  }
  setTransformAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("x"),
                          std::max(-640, std::min(value, 640)), componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeY(int value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("y"),
                                           std::max(-360, std::min(value, 360)));
    return;
  }
  setTransformAndKeyframe(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("y"),
                          std::max(-360, std::min(value, 360)), componentKeyframeFrame());
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeWidth(int value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("width"),
                                           std::max(1, std::min(value, 640)));
    return;
  }
  setTransformStatic(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("width"),
                     std::max(1, std::min(value, 640)));
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeHeight(int value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("height"),
                                           std::max(1, std::min(value, 360)));
    return;
  }
  setTransformStatic(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("height"),
                     std::max(1, std::min(value, 360)));
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeRotation(double value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  const auto clamped = std::max(-180.0, std::min(value, 180.0));
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("rotation"), clamped);
    return;
  }
  setTransformStatic(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("rotation"), clamped);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeScale(double value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() ||
      !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  const auto clamped = std::max(0.1, std::min(value, 3.0));
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("scale"), clamped);
    return;
  }
  setTransformStatic(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("scale"), clamped);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeOpacity(double value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable() || !findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_)) return;
  const auto clamped = std::max(0.0, std::min(value, 1.0));
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("opacity"), clamped);
    return;
  }
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
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, field, color);
    return;
  }
  setPropertyStatic(*demoOverlayIr_, selectedComponentNodeId_, field, color);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeBorderColor(const QString& value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable()) return;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node || node->value("type").toString() != QStringLiteral("shape")) return;
  const auto color = value.trimmed().left(32);
  if (!color.startsWith(QLatin1Char('#')) || (color.size() != 4 && color.size() != 7 && color.size() != 9)) return;
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("borderColor"), color);
    return;
  }
  setPropertyStatic(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("borderColor"), color);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setSelectedComponentNodeFontFamily(const QString& value) {
  if (!demoOverlayIr_ || !componentPlayheadIsEditable()) return;
  const auto node = findNode(demoOverlayIr_->toJson().value("root").toObject(), selectedComponentNodeId_);
  if (!node || node->value("type").toString() != QStringLiteral("text")) return;
  const auto family = value.trimmed().left(120);
  if (family.isEmpty() || family.contains(QRegularExpression(QStringLiteral("[\\r\\n]")))) return;
  if (resolveConnected_ && !resolveAdapter_.lastInsertedComponentId().isEmpty()) {
    setSelectedComponentPropertyAtPlayhead(selectedComponentNodeId_, QStringLiteral("fontFamily"), family);
    return;
  }
  setPropertyStatic(*demoOverlayIr_, selectedComponentNodeId_, QStringLiteral("fontFamily"), family);
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

bool WorkbenchRuntime::connectResolve(bool notifyFailure) {
  // 每次建立新连接都使旧的重建事务句柄失效，避免跨连接误提交。
  resolveRebuildTransactionId_.clear();
  resolveRebuildTransactionState_.clear();
  emit timelineChanged();
  const auto bridgeUrl = qEnvironmentVariable("EDWARD_RESOLVE_BRIDGE_URL");
  const auto configuredMode = qEnvironmentVariable("EDWARD_RESOLVE_MODE").trimmed().toLower();
  const auto directMode = configuredMode == QStringLiteral("direct") ||
                          (configuredMode.isEmpty() && bridgeUrl.isEmpty());
  QString error;
  bool connected = false;
  if (directMode) {
    const auto python = qEnvironmentVariable("EDWARD_RESOLVE_PYTHON", "python3");
    auto sidecar = qEnvironmentVariable("EDWARD_RESOLVE_SIDECAR");
    if (sidecar.isEmpty()) {
      const auto relativeSidecar = QStringLiteral("src/resolve/resolve-sidecar/resolve_direct_sidecar.py");
      const QStringList candidates{
          QDir::current().filePath(relativeSidecar),
          QDir(QCoreApplication::applicationDirPath()).filePath(
              QStringLiteral("../../../../../%1").arg(relativeSidecar)),
          QDir(QCoreApplication::applicationDirPath()).filePath(
              QStringLiteral("../Resources/resolve-sidecar/resolve_direct_sidecar.py"))};
      for (const auto& candidate : candidates) {
        const auto cleanCandidate = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleanCandidate)) {
          sidecar = cleanCandidate;
          break;
        }
      }
      if (sidecar.isEmpty()) {
        sidecar = QDir::current().filePath(relativeSidecar);
      }
    }
    connected = resolveConnection_.connectToDirectSidecar(python, sidecar, &error);
  } else if (!bridgeUrl.isEmpty()) {
    connected = resolveConnection_.connectToBridge(QUrl(bridgeUrl), &error);
  } else {
    resolveConnected_ = false;
    resolveStatus_ = QStringLiteral("请先启动并连接 Resolve Studio");
    emit resolveStateChanged();
    if (notifyFailure) emit operationFailed(resolveStatus_);
    return false;
  }

  if (!connected || !resolveAdapter_.attach(&error)) {
    resolveConnected_ = false;
    resolveRebuildTransactionId_.clear();
    resolveRebuildTransactionState_.clear();
    emit timelineChanged();
    resolveStatus_ = error.isEmpty() ? QStringLiteral("Resolve Studio 连接失败")
                                    : QStringLiteral("Resolve Studio 连接失败：%1").arg(error);
    emit resolveStateChanged();
    if (notifyFailure) emit operationFailed(resolveStatus_);
    return false;
  }
  resolveConnected_ = true;
  resolveStatus_ = QStringLiteral("已连接 Resolve Studio");
  emit resolveStateChanged();
  const auto timelineReady = refreshResolveTimeline();
  if (timelineReady) {
    resolveContextPollTimer_.start();
    resolveReconnectTimer_.stop();
    refreshResolveRenderCapabilities();
  } else {
    resolveContextPollTimer_.stop();
  }
  return timelineReady;
}

bool WorkbenchRuntime::refreshResolveRenderCapabilities() {
  if (!resolveConnected_) {
    resolveRenderFormats_.clear();
    resolveRenderCodecs_.clear();
    resolveRenderResolutions_.clear();
    emit resolveRenderCapabilitiesChanged();
    return false;
  }
  QString error;
  const auto capabilities = resolveAdapter_.renderCapabilities({}, {}, &error);
  if (!capabilities) {
    emit operationFailed(error.isEmpty() ? QStringLiteral("无法读取 Resolve 导出能力") : error);
    return false;
  }
  resolveRenderFormats_.clear();
  const auto appendFormat = [this, &capabilities](const QString& key) {
    const auto it = capabilities->formats.constFind(key);
    if (it == capabilities->formats.constEnd()) return;
    resolveRenderFormats_.push_back(QVariantMap{{QStringLiteral("name"), it.key()},
                                                {QStringLiteral("extension"), it.value().toString()},
                                                {QStringLiteral("label"), QStringLiteral("%1（.%2）").arg(it.key(), it.value().toString())}});
  };
  appendFormat(QStringLiteral("MP4"));
  appendFormat(QStringLiteral("QuickTime"));
  for (auto it = capabilities->formats.constBegin(); it != capabilities->formats.constEnd(); ++it) {
    if (it.key() == QStringLiteral("MP4") || it.key() == QStringLiteral("QuickTime")) continue;
    appendFormat(it.key());
  }
  resolveRenderCodecs_.clear();
  for (auto it = capabilities->codecs.constBegin(); it != capabilities->codecs.constEnd(); ++it)
    resolveRenderCodecs_.push_back(QVariantMap{{QStringLiteral("name"), it.key()},
                                               {QStringLiteral("value"), it.value().toString()},
                                               {QStringLiteral("label"), it.key()}});
  resolveRenderResolutions_.clear();
  for (const auto& value : capabilities->resolutions) {
    const auto object = value.toObject();
    const auto width = object.value(QStringLiteral("Width")).toInt();
    const auto height = object.value(QStringLiteral("Height")).toInt();
    resolveRenderResolutions_.push_back(QVariantMap{{QStringLiteral("width"), width},
                                                    {QStringLiteral("height"), height},
                                                    {QStringLiteral("label"), QStringLiteral("%1 × %2").arg(width).arg(height)}});
  }
  emit resolveRenderCapabilitiesChanged();
  return true;
}

bool WorkbenchRuntime::exportWithEdwardOptions(const QString& outputPath, int width, int height,
                                               int fps, int quality) {
  return exportTimelineWithOptions(outputPath, width, height, fps, quality);
}

void WorkbenchRuntime::clearExportDialogRequest() {
  if (!exportDialogRequested_) return;
  exportDialogRequested_ = false;
  emit timelineChanged();
}

bool WorkbenchRuntime::openResolveDeliverPage() {
  if (!resolveConnected_) {
    resolveStatus_ = QStringLiteral("请先启动并连接 Resolve Studio");
    emit resolveStateChanged();
    return false;
  }
  QString error;
  if (!resolveAdapter_.openDeliverPage(&error)) {
    resolveStatus_ = error.isEmpty() ? QStringLiteral("无法打开 Resolve 导出页") : error;
    emit resolveStateChanged();
    return false;
  }
  return true;
}

bool WorkbenchRuntime::importResolveLayoutPreset(const QString& path, const QString& name) {
  if (!resolveConnected_) {
    resolveStatus_ = QStringLiteral("请先启动并连接 Resolve Studio");
    emit resolveStateChanged();
    emit operationFailed(QStringLiteral("请先启动并连接 Resolve Studio"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.importLayoutPreset(path, name, &error)) {
    emit operationFailed(QStringLiteral("Resolve 布局预设导入失败：%1").arg(error));
    return false;
  }
  emit operationSucceeded(QStringLiteral("Resolve 布局预设已导入"));
  return true;
}

bool WorkbenchRuntime::loadResolveLayoutPreset(const QString& name) {
  if (!resolveConnected_) {
    resolveStatus_ = QStringLiteral("请先启动并连接 Resolve Studio");
    emit resolveStateChanged();
    emit operationFailed(QStringLiteral("请先启动并连接 Resolve Studio"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.loadLayoutPreset(name, &error)) {
    emit operationFailed(QStringLiteral("Resolve 布局预设加载失败：%1").arg(error));
    return false;
  }
  emit operationSucceeded(QStringLiteral("Resolve 布局预设已加载"));
  return true;
}

bool WorkbenchRuntime::saveResolveLayoutPreset(const QString& name) {
  if (!resolveConnected_) {
    resolveStatus_ = QStringLiteral("请先启动并连接 Resolve Studio");
    emit resolveStateChanged();
    emit operationFailed(QStringLiteral("请先启动并连接 Resolve Studio"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.saveLayoutPreset(name, &error)) {
    emit operationFailed(QStringLiteral("Resolve 布局预设保存失败：%1").arg(error));
    return false;
  }
  emit operationSucceeded(QStringLiteral("Resolve 布局预设已保存"));
  return true;
}

bool WorkbenchRuntime::exportResolveLayoutPreset(const QString& name, const QString& path) {
  if (!resolveConnected_) {
    resolveStatus_ = QStringLiteral("请先启动并连接 Resolve Studio");
    emit resolveStateChanged();
    emit operationFailed(QStringLiteral("请先启动并连接 Resolve Studio"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.exportLayoutPreset(name, path, &error)) {
    emit operationFailed(QStringLiteral("Resolve 布局预设导出失败：%1").arg(error));
    return false;
  }
  emit operationSucceeded(QStringLiteral("Resolve 布局预设已导出"));
  return true;
}

bool WorkbenchRuntime::ensureResolveSubtitleTrack() {
  if (!resolveConnected_) {
    emit operationFailed(QStringLiteral("请先启动并连接 Resolve Studio"));
    return false;
  }
  QString error;
  if (!resolveAdapter_.ensureSubtitleTrack(&error)) {
    emit operationFailed(QStringLiteral("字幕轨道创建失败：%1").arg(error));
    return false;
  }
  emit operationSucceeded(QStringLiteral("已确保 Edward 原生字幕轨道位于最上方"));
  return true;
}

void WorkbenchRuntime::disconnectResolve() {
  resolveConnection_.disconnect();
  resolveContextPollTimer_.stop();
  resolveReconnectTimer_.stop();
  resolveConnected_ = false;
  // 事务状态只在同一 Resolve 会话内有效；断开后必须丢弃本地句柄。
  resolveRebuildTransactionId_.clear();
  resolveRebuildTransactionState_.clear();
  resolveTimelineSnapshot_.reset();
  resolveSelectionId_.clear();
  resolveSelectionKind_ = QStringLiteral("none");
  resolveSelectionName_.clear();
  resolveContextSummary_ = QStringLiteral("尚未读取 Resolve 当前上下文");
  resolveRenderFormats_.clear();
  resolveRenderCodecs_.clear();
  resolveRenderResolutions_.clear();
  resolveStatus_ = QStringLiteral("未连接 Resolve Studio");
  emit timelineChanged();
  emit resolveStateChanged();
  emit resolveRenderCapabilitiesChanged();
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
    const auto transportFailure = error == QStringLiteral("bridge_not_connected") ||
                                  error == QStringLiteral("bridge_response_timeout") ||
                                  error == QStringLiteral("bridge_write_failed") ||
                                  error == QStringLiteral("direct_sidecar_response_timeout") ||
                                  error == QStringLiteral("direct_sidecar_write_failed");
    if (transportFailure) {
      resolveConnection_.disconnect();
      resolveConnected_ = false;
      resolveContextPollTimer_.stop();
      resolveStatus_ = QStringLiteral("Resolve 连接已断开，正在重连");
      emit resolveStateChanged();
      resolveReconnectTimer_.start();
      return false;
    }
    resolveStatus_ = QStringLiteral("Resolve 时间线读取失败：%1").arg(error);
    emit resolveStateChanged();
    emit operationFailed(resolveStatus_);
    return false;
  }
  resolveTimelineSnapshot_ = snapshot;
  // 将 Resolve 当前时间线登记为项目级机器可读清单，供 AI 和后续操作引用。
  if (!activeProjectPath_.isEmpty()) {
    QString itemsError;
    const auto items = resolveAdapter_.timelineItems(&itemsError);
    QJsonArray tracks;
    for (const auto& track : snapshot->tracks) {
      tracks.append(QJsonObject{{QStringLiteral("id"), track.id}, {QStringLiteral("name"), track.name},
                                {QStringLiteral("index"), track.index}, {QStringLiteral("video"), track.video},
                                {QStringLiteral("audio"), track.audio}, {QStringLiteral("subtitle"), track.subtitle},
                                {QStringLiteral("locked"), track.locked}});
    }
    const QJsonObject registerObject{
        {QStringLiteral("version"), 2},
        {QStringLiteral("projectName"), snapshot->projectName},
        {QStringLiteral("timelineName"), snapshot->timelineName},
        {QStringLiteral("fpsNumerator"), snapshot->fpsNumerator},
        {QStringLiteral("fpsDenominator"), snapshot->fpsDenominator},
        {QStringLiteral("timelineStartFrame"), snapshot->timelineStartFrame},
        {QStringLiteral("timelineEndFrame"), snapshot->timelineEndFrame},
        {QStringLiteral("playheadFrame"), snapshot->playheadFrame},
        {QStringLiteral("markInFrame"), snapshot->markInFrame},
        {QStringLiteral("markOutFrame"), snapshot->markOutFrame},
        {QStringLiteral("markers"), snapshot->markers},
        {QStringLiteral("tracks"), tracks},
        {QStringLiteral("items"), items}};
    QSaveFile registerFile(activeProjectPath_ + QStringLiteral(".resolve-register.json"));
    if (registerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      registerFile.write(QJsonDocument(registerObject).toJson(QJsonDocument::Indented));
      registerFile.commit();
    }
  }
  resolveStatus_ = QStringLiteral("已连接 Resolve Studio：%1").arg(snapshot->timelineName);
  QString currentItemError;
  const auto currentItem = resolveAdapter_.currentItem(&currentItemError);
  if (currentItem) {
    resolveSelectionId_ = currentItem->timelineItemId;
    resolveSelectionName_ = currentItem->name;
    resolveSelectionKind_ = currentItem->trackType == QStringLiteral("audio")
                               ? QStringLiteral("audio")
                               : currentItem->trackType == QStringLiteral("subtitle")
                                   ? QStringLiteral("subtitle")
                                   : currentItem->fusionCompCount > 0
                                       ? QStringLiteral("component")
                                       : QStringLiteral("video");
    const auto duration = currentItem->endFrame - currentItem->startFrame;
    const auto source = currentItem->selectionSource == QStringLiteral("selected")
                            ? QStringLiteral("用户选中")
                            : QStringLiteral("播放头所在");
    resolveContextSummary_ = QStringLiteral("当前片段：%1（%2）\n帧范围：%3–%4（%5 帧）\n播放头：%6\n轨道：%7 条")
                                .arg(currentItem->name.isEmpty() ? QStringLiteral("未命名片段") : currentItem->name)
                                .arg(source)
                                .arg(currentItem->startFrame)
                                .arg(currentItem->endFrame)
                                .arg(duration)
                                .arg(snapshot->playheadFrame)
                                .arg(snapshot->tracks.size());
    if (snapshot->markInFrame >= 0 && snapshot->markOutFrame >= snapshot->markInFrame) {
      resolveContextSummary_ += QStringLiteral("\nI/O：%1–%2").arg(snapshot->markInFrame).arg(snapshot->markOutFrame);
    }
    if (!snapshot->markers.isEmpty()) {
      resolveContextSummary_ += QStringLiteral("\n时间线标记：%1 个").arg(snapshot->markers.size());
    }
  } else {
    resolveSelectionId_.clear();
    resolveSelectionKind_ = QStringLiteral("none");
    resolveSelectionName_.clear();
    resolveContextSummary_ = QStringLiteral("未选中 Resolve 片段\n播放头：%1\n轨道：%2 条")
                                .arg(snapshot->playheadFrame)
                                .arg(snapshot->tracks.size());
    if (snapshot->markInFrame >= 0 && snapshot->markOutFrame >= snapshot->markInFrame) {
      resolveContextSummary_ += QStringLiteral("\nI/O：%1–%2").arg(snapshot->markInFrame).arg(snapshot->markOutFrame);
    }
    if (!snapshot->markers.isEmpty()) {
      resolveContextSummary_ += QStringLiteral("\n时间线标记：%1 个").arg(snapshot->markers.size());
    }
    if (!currentItemError.isEmpty()) resolveContextSummary_ += QStringLiteral("\n（%1）").arg(currentItemError);
  }
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
  const auto before = demoOverlayIr_->toJson();
  const auto frame = resolveTimelineSnapshot_->playheadFrame;
  const bool isPosition = (field == QStringLiteral("x") || field == QStringLiteral("y")) && value.isDouble();
  const bool isVerifiedTextProperty = (field == QStringLiteral("text") || field == QStringLiteral("fontFamily")) &&
                                      value.isString();
  const bool isStaticRectangleStyle =
      (field == QStringLiteral("borderColor") && value.isString()) ||
      (field == QStringLiteral("borderWidth") && value.isDouble());
  const bool isTransformProperty = (field == QStringLiteral("rotation") || field == QStringLiteral("scale")) &&
                                   value.isDouble();
  const bool isOpacity = field == QStringLiteral("opacity") && value.isDouble() &&
                         value.toDouble() >= 0.0 && value.toDouble() <= 1.0;
  if (!isPosition && !isVerifiedTextProperty && !isStaticRectangleStyle && !isTransformProperty && !isOpacity) {
    emit operationFailed(QStringLiteral("当前独立 Fusion 组件仅验证位置 x/y 关键帧、文字和字体写入，以及矩形静态边框写入"));
    return false;
  }
  bool changed = false;
  if (isPosition) {
    changed = demoOverlayIr_->setNodeTransformNumber(nodeId, field, value.toDouble()) &&
              demoOverlayIr_->setNodeKeyframeNumber(nodeId, field, frame, value.toDouble());
    const auto updatedNode = findNode(demoOverlayIr_->toJson().value(QStringLiteral("root")).toObject(), nodeId);
    const auto pairedField = field == QStringLiteral("x") ? QStringLiteral("y") : QStringLiteral("x");
    changed = changed && updatedNode && updatedNode->value(QStringLiteral("transform")).toObject()
                                              .value(pairedField).isDouble() &&
              demoOverlayIr_->setNodeKeyframeNumber(
                  nodeId, pairedField, frame,
                  updatedNode->value(QStringLiteral("transform")).toObject()
                      .value(pairedField).toDouble());
  } else if (isTransformProperty) {
    // 旋转/缩放已通过 Fusion Transform 输入验证；本地 IR 也必须保留
    // 当前播放头关键帧，确保属性面板回读与 Resolve 写入保持一致。
    changed = demoOverlayIr_->setNodeTransformNumber(nodeId, field, value.toDouble()) &&
              demoOverlayIr_->setNodeKeyframeNumber(nodeId, field, frame, value.toDouble());
  } else if (isOpacity) {
    changed = demoOverlayIr_->setNodeProperty(nodeId, field, value) &&
              demoOverlayIr_->setNodeKeyframeNumber(nodeId, field, frame, value.toDouble());
  } else {
    changed = demoOverlayIr_->setNodeProperty(nodeId, field, value);
  }
  if (!changed) {
    demoOverlayIr_ = edward::core::ComponentIr::parse(before);
    emit operationFailed(QStringLiteral("组件属性无效"));
    return false;
  }
  QString error;
  const auto capabilities = resolveAdapter_.capabilities(&error);
  const auto converted = capabilities
      ? edward::resolve::convertComponentToFusion(*demoOverlayIr_, *capabilities)
      : edward::resolve::FusionConversionResult{};
  const auto binding = std::find_if(converted.bindings.cbegin(), converted.bindings.cend(),
                                    [&nodeId](const QJsonValue& raw) {
    return raw.toObject().value(QStringLiteral("nodeId")).toString() == nodeId;
  });
  auto bindingObject = binding == converted.bindings.cend() ? QJsonObject{} : binding->toObject();
  // 图片节点会先产生 MediaImage 绑定，文字节点会先产生 TextPlus 绑定，随后才是同 nodeId 的 Transform 绑定。
  // 旋转/缩放必须选择 Transform，而不是把内容绑定误当成可写工具。
  if (bindingObject.value(QStringLiteral("tool")).toString() == QStringLiteral("MediaImage") ||
      (isTransformProperty && bindingObject.value(QStringLiteral("tool")).toString() != QStringLiteral("Transform"))) {
    const auto transformBinding = std::find_if(converted.bindings.cbegin(), converted.bindings.cend(),
                                               [&nodeId](const QJsonValue& raw) {
      const auto object = raw.toObject();
      return object.value(QStringLiteral("nodeId")).toString() == nodeId &&
             object.value(QStringLiteral("tool")).toString() == QStringLiteral("Transform");
    });
    if (transformBinding != converted.bindings.cend()) bindingObject = transformBinding->toObject();
  }
  const auto tool = bindingObject.value(QStringLiteral("tool")).toString();
  const auto toolName = bindingObject.value(QStringLiteral("toolName")).toString().isEmpty() &&
                                tool == QStringLiteral("Transform")
                            ? QStringLiteral("EdwardTransform")
                            : bindingObject.value(QStringLiteral("toolName")).toString();
  const auto values = bindingObject.value(QStringLiteral("values")).toObject();
  const bool isWritableRectangleStyle = isStaticRectangleStyle &&
                                        tool == QStringLiteral("RectangleOverlay") &&
                                        values.value(QStringLiteral("borderColor")).isString() &&
                                        values.value(QStringLiteral("borderWidth")).isDouble();
  const bool isTransformBindingProperty = isTransformProperty && tool == QStringLiteral("Transform");
  const auto timelineItemId = resolveComponentNodeTimelineIds_.value(
      nodeId, resolveAdapter_.lastInsertedComponentId());
  if (timelineItemId.isEmpty()) {
    demoOverlayIr_ = edward::core::ComponentIr::parse(before);
    emit operationFailed(QStringLiteral("Resolve 组件属性写入失败：找不到节点所属时间线片段"));
    return false;
  }
  const bool written = converted.report.complete() &&
      (isPosition
           ? values.value(QStringLiteral("x")).isDouble() && values.value(QStringLiteral("y")).isDouble() &&
                 (tool == QStringLiteral("TextPlus")
                      ? resolveAdapter_.addNamedFusionTextPositionKeyframe(
                            toolName, frame, values.value(QStringLiteral("x")).toDouble(),
                            values.value(QStringLiteral("y")).toDouble(),
                            timelineItemId, &error)
                      : tool == QStringLiteral("RectangleOverlay")
                            ? resolveAdapter_.addNamedFusionRectanglePositionKeyframe(
                                  toolName, frame, values.value(QStringLiteral("x")).toDouble(),
                                  values.value(QStringLiteral("y")).toDouble(),
                                  timelineItemId, &error)
                            : tool == QStringLiteral("Transform")
                                  ? resolveAdapter_.addNamedFusionTransformPositionKeyframe(
                                        toolName, frame, values.value(QStringLiteral("x")).toDouble(),
                                        values.value(QStringLiteral("y")).toDouble(),
                                        timelineItemId, &error)
                                  : false)
           : isOpacity
                 ? resolveAdapter_.ensureFusionOpacityGraph(timelineItemId, &error) &&
                   resolveAdapter_.addFusionKeyframe(QStringLiteral("EdwardFadeMerge"),
                                                     QStringLiteral("Blend"), frame, value,
                                                     timelineItemId, &error)
           : isTransformBindingProperty
                 ? resolveAdapter_.addFusionKeyframe(
                       toolName, field == QStringLiteral("rotation") ? QStringLiteral("Angle")
                                                                        : QStringLiteral("Size"),
                       frame, value, timelineItemId, &error)
           : tool == QStringLiteral("TextPlus") &&
                 resolveAdapter_.setFusionProperty(toolName, field, value,
                                                   timelineItemId, &error) ||
                 (isWritableRectangleStyle &&
                  resolveAdapter_.setFusionRectangleStyle(
                      toolName, values.value(QStringLiteral("borderColor")).toString(),
                      values.value(QStringLiteral("borderWidth")).toDouble(),
                      timelineItemId, &error)));
  if (!written) {
    demoOverlayIr_ = edward::core::ComponentIr::parse(before);
    emit operationFailed(QStringLiteral("Resolve 组件属性写入失败：%1").arg(
        error.isEmpty() ? QStringLiteral("组件节点未映射到已验证 Fusion 工具") : error));
    return false;
  }
  refreshDemoOverlay();
  emit timelineChanged();
  return true;
}

QVariantList WorkbenchRuntime::inspectSelectedResolveTool(const QString& toolName) {
  QVariantList output;
  if (!resolveConnected_) {
    emit operationFailed(QStringLiteral("请先连接 Resolve Studio"));
    return output;
  }
  QString error;
  const auto inputs = resolveAdapter_.inspectFusionTool(toolName, {}, &error);
  if (!error.isEmpty()) {
    emit operationFailed(QStringLiteral("读取 Resolve 属性失败：%1").arg(error));
    return output;
  }
  for (const auto& input : inputs) {
    QVariantMap item;
    item.insert(QStringLiteral("key"), input.key);
    item.insert(QStringLiteral("id"), input.id);
    item.insert(QStringLiteral("displayName"), input.displayName);
    item.insert(QStringLiteral("type"), input.type);
    item.insert(QStringLiteral("control"), input.control);
    item.insert(QStringLiteral("value"), input.value.toVariant());
    output.push_back(item);
  }
  return output;
}

bool WorkbenchRuntime::setSelectedResolveAttribute(const QString& toolName,
                                                   const QString& inputName,
                                                   const QJsonValue& value) {
  if (!resolveConnected_) {
    emit operationFailed(QStringLiteral("请先连接 Resolve Studio"));
    return false;
  }
  QString error;
  const bool propertyField = resolve::resolvePropertyBinding(inputName).has_value();
  const bool accepted = propertyField
      ? resolveAdapter_.setFusionProperty(toolName, inputName, value, {}, &error)
      : resolveAdapter_.setFusionInput(toolName, inputName, value, {}, &error);
  if (!accepted) {
    emit operationFailed(QStringLiteral("Resolve 属性写入失败：%1").arg(error));
    return false;
  }
  emit operationSucceeded(QStringLiteral("Resolve 属性已更新"));
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
    auto clipObject = QJsonObject{{"id", static_cast<qint64>(clip.id)},
                             {"trackId", static_cast<qint64>(clip.trackId)},
                             {"source", QString::fromStdString(clip.source.string())},
                             {"sourceIn", static_cast<qint64>(clip.sourceIn)},
                             {"sourceOut", static_cast<qint64>(clip.sourceOut)},
                             {"timelineStart", static_cast<qint64>(clip.timelineStart)},
                             {"kind", clip.kind == edward::core::TimelineClipKind::Component ? "component" : "media"},
                             {"component", clip.component ? QJsonValue(clip.component->toJson()) : QJsonValue()}};
    if (clip.nativeRuntime) {
      clipObject.insert(QStringLiteral("nativeRuntime"), QJsonObject{
          {QStringLiteral("packageRoot"), clip.nativeRuntime->packageRoot},
          {QStringLiteral("manifestPath"), clip.nativeRuntime->manifestPath},
          {QStringLiteral("runtime"), clip.nativeRuntime->runtime},
          {QStringLiteral("props"), clip.nativeRuntime->props}});
    }
    clips.append(clipObject);
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
  flushPreferencesForProjectClose();
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
  QHash<QString, edward::core::NativeRuntimeComponent> loadedNativeRuntimePackages;
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
    std::optional<edward::core::NativeRuntimeComponent> clipNativeRuntime;
    if (kind == edward::core::TimelineClipKind::Component) {
      if (clip.value("nativeRuntime").isObject()) {
        const auto native = clip.value("nativeRuntime").toObject();
        if (!native.value("packageRoot").isString() || !native.value("manifestPath").isString() ||
            !native.value("runtime").isString() || !native.value("props").isObject()) return false;
        clipNativeRuntime = edward::core::NativeRuntimeComponent{
            native.value("packageRoot").toString(), native.value("manifestPath").toString(),
            native.value("runtime").toString(), native.value("props").toObject()};
        if (!clipNativeRuntime->valid()) return false;
      } else {
        if (!clip.value("component").isObject()) return false;
        clipComponent = edward::core::ComponentIr::parse(clip.value("component").toObject());
        if (!clipComponent) return false;
      }
    }
    if (clipNativeRuntime)
      loadedNativeRuntimePackages.insert(nativeRuntimeResourceId(*clipNativeRuntime), *clipNativeRuntime);
    snapshot.clips.push_back({static_cast<edward::core::ClipId>(clip.value("id").toInteger()),
                              static_cast<edward::core::TrackId>(clip.value("trackId").toInteger()),
                              clip.value("source").toString().toStdString(), clip.value("sourceIn").toInteger(),
                              clip.value("sourceOut").toInteger(), clip.value("timelineStart").toInteger(),
                              kind, std::move(clipComponent), std::move(clipNativeRuntime)});
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
  nativeRuntimePackages_ = std::move(loadedNativeRuntimePackages);
  projectIdentity_ = std::move(projectIdentity);
  previewSession_ = edward::media::PreviewSession(previewStorageRoots_, projectIdentity_);
  previewFrameCache_ = edward::media::PreviewFrameCache(previewStorageRoots_, projectIdentity_);
  refreshClipWaveforms();
  refreshClipThumbnails();
  demoOverlayIr_ = std::move(component);
  componentClipId_ = demoOverlayIr_ ? componentClipId : 0;
  aiConversation_ = std::move(conversation);
  selectedNativeRuntime_.reset();
  editingComponentClipId_ = 0;
  if (const auto selected = timeline_.clip(controller_.selectedClip()); selected && selected->nativeRuntime) {
    selectedNativeRuntime_ = *selected->nativeRuntime;
    editingComponentClipId_ = selected->id;
  }
  pendingAiPrompt_.clear();
  pendingAiConversationOnly_ = false;
  pendingAiNativeProtocol_ = false;
  pendingAiActionPlan_.clear();
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
