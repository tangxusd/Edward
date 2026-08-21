#include <edward/core/project_identity.hpp>
#include <edward/media/render_storage.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main() {
  const auto identity = edward::core::ProjectIdentity::parse(
      QStringLiteral("550e8400-e29b-41d4-a716-446655440000"));
  assert(identity);
  QTemporaryDir temporary;
  assert(temporary.isValid());
  const auto root = std::filesystem::path(temporary.path().toStdString());
  const edward::media::RenderStorageRoots roots{root / "proxies", root / "cache", root / "renders"};
  const auto paths = edward::media::RenderStorage::paths(*identity, roots);
  assert(paths.proxyDirectory == roots.proxyRoot / identity->value().toStdString());
  assert(paths.cacheDirectory == roots.cacheRoot / identity->value().toStdString());
  assert(paths.renderDirectory == roots.renderRoot / identity->value().toStdString());
  std::filesystem::create_directories(paths.proxyDirectory);
  std::filesystem::create_directories(roots.proxyRoot / "not-a-project");
  QFile keep(QString::fromStdString((roots.proxyRoot / "keep.txt").string()));
  assert(keep.open(QIODevice::WriteOnly));
  assert(keep.write("keep") == 4);
  keep.close();
  assert(edward::media::RenderStorage::clearDerived(roots, edward::media::DerivedStorageKind::Proxy));
  assert(!std::filesystem::exists(paths.proxyDirectory));
  assert(std::filesystem::exists(roots.proxyRoot / "not-a-project"));
  assert(std::filesystem::exists(roots.proxyRoot / "keep.txt"));
  assert(edward::media::RenderStorage::clearDerived(roots, edward::media::DerivedStorageKind::Cache));
  assert(edward::media::RenderStorage::clearDerived(roots, edward::media::DerivedStorageKind::Render));
  return 0;
}
