#include "edward/media/preview_frame_cache.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>

namespace edward::media {

PreviewFrameCache::PreviewFrameCache(RenderStorageRoots roots, edward::core::ProjectIdentity project)
    : roots_(std::move(roots)), project_(std::move(project)) {}

std::filesystem::path PreviewFrameCache::pathFor(const QByteArray& key) const {
  const auto digest = QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex();
  return RenderStorage::paths(project_, roots_).cacheDirectory /
         (digest.toStdString() + ".png");
}

std::optional<QImage> PreviewFrameCache::load(const QByteArray& key) const {
  if (key.isEmpty()) return std::nullopt;
  QImage frame(QString::fromStdString(pathFor(key).string()));
  if (frame.isNull()) return std::nullopt;
  return frame;
}

bool PreviewFrameCache::store(const QByteArray& key, const QImage& frame) const {
  if (key.isEmpty() || frame.isNull()) return false;
  const auto path = pathFor(key);
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  QSaveFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::WriteOnly)) return false;
  if (!frame.save(&file, "PNG")) {
    file.cancelWriting();
    return false;
  }
  return file.commit();
}

}  // namespace edward::media
