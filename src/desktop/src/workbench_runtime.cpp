#include "edward/desktop/workbench_runtime.hpp"

#include "edward/plugins/plugin_host.hpp"

#include <QVariantMap>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
}

void WorkbenchRuntime::refreshDemoOverlay() {
  if (demoOverlayEnabled_) renderGraph_.setOverlay(demoOverlayIr_);
}

int WorkbenchRuntime::playheadFrame() const { return static_cast<int>(controller_.playheadFrame()); }

QString WorkbenchRuntime::installedPluginId() const {
  return installedPlugin_ ? installedPlugin_->manifest.pluginId : QString{};
}

QVariantList WorkbenchRuntime::clips() const {
  QVariantList result;
  for (const auto& clip : timeline_.clips(videoTrack_)) {
    QVariantMap item;
    item.insert(QStringLiteral("id"), static_cast<qlonglong>(clip.id));
    item.insert(QStringLiteral("trackIndex"), 0);
    item.insert(QStringLiteral("timelineStart"), static_cast<qlonglong>(clip.timelineStart));
    item.insert(QStringLiteral("sourceIn"), static_cast<qlonglong>(clip.sourceIn));
    item.insert(QStringLiteral("sourceOut"), static_cast<qlonglong>(clip.sourceOut));
    item.insert(QStringLiteral("name"), QString::fromStdString(clip.source.filename().string()));
    item.insert(QStringLiteral("selected"), clip.id == controller_.selectedClip());
    result.push_back(item);
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

void WorkbenchRuntime::clearComponentOverlay() {
  demoOverlayIr_.reset();
  demoOverlayEnabled_ = false;
  renderGraph_.setOverlay(std::nullopt);
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayX(int value) {
  const int clamped = std::max(0, std::min(value, 640));
  if (demoOverlayX_ == clamped) return;
  demoOverlayX_ = clamped;
  if (demoOverlayIr_) {
    demoOverlayIr_->setNodeTransformNumber("demo-box", "x", demoOverlayX_);
    demoOverlayIr_->setNodeTransformNumber("demo-text", "x", demoOverlayX_ + 20);
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
  }
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayWidth(int value) {
  demoOverlayWidth_ = std::max(40, std::min(value, 640));
  if (demoOverlayIr_) demoOverlayIr_->setNodeTransformNumber("demo-box", "width", demoOverlayWidth_);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayHeight(int value) {
  demoOverlayHeight_ = std::max(24, std::min(value, 360));
  if (demoOverlayIr_) demoOverlayIr_->setNodeTransformNumber("demo-box", "height", demoOverlayHeight_);
  refreshDemoOverlay();
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayOpacity(double value) {
  demoOverlayOpacity_ = std::max(0.0, std::min(value, 1.0));
  if (demoOverlayIr_) demoOverlayIr_->setNodeProperty("demo-box", "opacity", demoOverlayOpacity_);
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
  if (!controller_.rippleDeleteSelected()) {
    emit operationFailed(QStringLiteral("没有可删除的片段"));
    return false;
  }
  emit timelineChanged();
  return true;
}

bool WorkbenchRuntime::rippleDeleteSelected() { return deleteSelected(); }

}  // namespace edward::desktop
