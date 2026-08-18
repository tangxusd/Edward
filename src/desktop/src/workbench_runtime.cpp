#include "edward/desktop/workbench_runtime.hpp"

#include <QVariantMap>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace edward::desktop {

namespace {
std::optional<edward::core::ComponentIr> demoOverlay(int x) {
  const QJsonObject box{{"id", "demo-box"}, {"type", "shape"},
                        {"transform", QJsonObject{{"x", x}, {"y", 24}, {"width", 220}, {"height", 72}}},
                        {"properties", QJsonObject{{"fill", "#00b8c8"}, {"opacity", 0.82}}},
                        {"keyframes", QJsonObject{{"x", QJsonArray{
                            QJsonObject{{"frame", 0}, {"value", x}},
                            QJsonObject{{"frame", 90}, {"value", x + 156}}
                        }}}}};
  const QJsonObject text{{"id", "demo-text"}, {"type", "text"},
                         {"transform", QJsonObject{{"x", 44}, {"y", 44}, {"width", 180}, {"height", 32}}},
                         {"properties", QJsonObject{{"text", "Edward Component"}, {"fontSize", 18}, {"color", "#ffffff"}}}};
  const QJsonObject root{{"id", "demo-root"}, {"type", "container"}, {"children", QJsonArray{box, text}}};
  return edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
}
}  // namespace

WorkbenchRuntime::WorkbenchRuntime(QObject* parent)
    : QObject(parent), timeline_(900), videoTrack_(timeline_.addVideoTrack()), controller_(timeline_, videoTrack_),
      renderGraph_(mltAdapter_) {}

int WorkbenchRuntime::playheadFrame() const { return static_cast<int>(controller_.playheadFrame()); }

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
  renderGraph_.setOverlay(demoOverlayEnabled_ ? demoOverlay(demoOverlayX_) : std::nullopt);
  emit timelineChanged();
}

void WorkbenchRuntime::setDemoOverlayX(int value) {
  const int clamped = std::max(0, std::min(value, 640));
  if (demoOverlayX_ == clamped) return;
  demoOverlayX_ = clamped;
  if (demoOverlayEnabled_) renderGraph_.setOverlay(demoOverlay(demoOverlayX_));
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
