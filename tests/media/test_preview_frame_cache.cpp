#include <edward/core/project_identity.hpp>
#include <edward/media/preview_frame_cache.hpp>

#include <QColor>
#include <QImage>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main() {
  QTemporaryDir temporary;
  assert(temporary.isValid());
  const auto root = std::filesystem::path(temporary.path().toStdString());
  const edward::media::RenderStorageRoots roots{root / "proxies", root / "cache", root / "renders"};
  const auto firstProject = edward::core::ProjectIdentity::parse(
      QStringLiteral("550e8400-e29b-41d4-a716-446655440000"));
  const auto secondProject = edward::core::ProjectIdentity::parse(
      QStringLiteral("550e8400-e29b-41d4-a716-446655440001"));
  assert(firstProject && secondProject);

  edward::media::PreviewFrameCache cache(roots, *firstProject);
  edward::media::PreviewFrameCache otherProjectCache(roots, *secondProject);
  const QByteArray frameKey("timeline-v1-frame-42");
  assert(!cache.load(frameKey));

  QImage expected(QSize(8, 6), QImage::Format_RGBA8888);
  expected.fill(QColor("#00b8c8"));
  assert(cache.store(frameKey, expected));
  const auto hit = cache.load(frameKey);
  assert(hit);
  assert(hit->size() == expected.size());
  assert(hit->pixelColor(0, 0) == expected.pixelColor(0, 0));
  assert(!cache.load(QByteArray("timeline-v2-frame-42")));
  assert(!otherProjectCache.load(frameKey));

  const auto cacheDirectory = edward::media::RenderStorage::paths(*firstProject, roots).cacheDirectory;
  assert(std::filesystem::exists(cacheDirectory));
  assert(edward::media::RenderStorage::clearDerived(roots, edward::media::DerivedStorageKind::Cache));
  assert(!cache.load(frameKey));
  return 0;
}
