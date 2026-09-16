#include "edward/runtime/web_runtime_host.hpp"

#include <QDir>
#include <QFileInfo>

namespace edward::runtime {
namespace {
HostResult checkEntry(const RuntimeManifest& manifest, const QString& root) {
  const QFileInfo package(root);
  if (!package.exists() || !package.isDir()) return HostResult::failure(QStringLiteral("runtime package root does not exist"));
  const auto base = QDir(package.canonicalFilePath());
  for (const auto& entry : {manifest.previewEntry, manifest.renderEntry}) {
    const QFileInfo file(base.filePath(entry));
    if (!file.exists() || !file.isFile() || file.canonicalFilePath().isEmpty() ||
        !file.canonicalFilePath().startsWith(base.canonicalPath() + QDir::separator()))
      return HostResult::failure(QStringLiteral("runtime entry is outside package root or missing"));
  }
  return HostResult::success();
}
}

HostResult WebRuntimeHost::mount(const RuntimeManifest& manifest, const QString& packageRoot,
                                 const QJsonObject& props) {
  QString error;
  if (!manifest.validate(&error)) return HostResult::failure(error);
  const auto checked = checkEntry(manifest, packageRoot);
  if (!checked.ok) return checked;
  manifest_ = &manifest;
  packageRoot_ = QFileInfo(packageRoot).canonicalFilePath();
  props_ = props;
  frame_ = 0;
  mounted_ = true;
  return HostResult::success();
}

HostResult WebRuntimeHost::setFrame(std::int64_t frame) {
  if (!mounted_ || !manifest_) return HostResult::failure(QStringLiteral("runtime host is not mounted"));
  if (frame < 0 || frame >= manifest_->durationInFrames) return HostResult::failure(QStringLiteral("frame is outside runtime duration"));
  frame_ = frame;
  return HostResult::success();
}

HostResult WebRuntimeHost::setProps(const QJsonObject& props) {
  if (!mounted_) return HostResult::failure(QStringLiteral("runtime host is not mounted"));
  props_ = props;
  return HostResult::success();
}

HostResult WebRuntimeHost::renderFrame(std::int64_t frame, const QString& outputPath) {
  if (const auto result = setFrame(frame); !result.ok) return result;
  if (outputPath.isEmpty() || QFileInfo(outputPath).isAbsolute() == false)
    return HostResult::failure(QStringLiteral("render output path must be absolute"));
  return HostResult::success();
}

void WebRuntimeHost::unmount() {
  manifest_ = nullptr;
  packageRoot_.clear();
  props_ = {};
  frame_ = 0;
  mounted_ = false;
}

QJsonObject WebRuntimeHost::message(const QString& type, const QJsonObject& extra) const {
  QJsonObject result = extra;
  result.insert(QStringLiteral("type"), type);
  result.insert(QStringLiteral("frame"), frame_);
  result.insert(QStringLiteral("props"), props_);
  if (manifest_) {
    result.insert(QStringLiteral("runtime"), manifest_->runtime);
    result.insert(QStringLiteral("entry"), type == QStringLiteral("renderFrame") ? manifest_->renderEntry : manifest_->previewEntry);
  }
  return result;
}
}  // namespace edward::runtime
