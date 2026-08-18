#include "edward/desktop/workbench_runtime.hpp"

#include <QVariantMap>

namespace edward::desktop {

WorkbenchRuntime::WorkbenchRuntime(QObject* parent)
    : QObject(parent), timeline_(900), videoTrack_(timeline_.addVideoTrack()), controller_(timeline_, videoTrack_) {}

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

bool WorkbenchRuntime::importMedia(const QString& path) {
  if (!controller_.dropMediaAtPlayhead(path)) {
    emit operationFailed(QStringLiteral("素材无法添加：播放头位置存在冲突，或媒体不可读"));
    return false;
  }
  emit timelineChanged();
  return true;
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
