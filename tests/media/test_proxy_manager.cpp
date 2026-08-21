#include <edward/core/project_identity.hpp>
#include <edward/media/proxy_manager.hpp>

#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 2);
  QTemporaryDir temporary;
  assert(temporary.isValid());
  const auto root = std::filesystem::path(temporary.path().toStdString());
  const auto project = edward::core::ProjectIdentity::create();
  const edward::media::RenderStorageRoots roots{root / "proxies", root / "cache", root / "renders"};
  edward::media::ProxyManager manager(roots, project);
  const auto source = std::filesystem::path(argv[1]);
  assert(manager.sourceFor(source, edward::media::PreviewQuality::Original) == source);
  assert(manager.sourceFor(source, edward::media::PreviewQuality::Clear) == source);
  const auto proxy = manager.ensureProxy(source, edward::media::PreviewQuality::Clear);
  assert(proxy && std::filesystem::is_regular_file(*proxy));
  assert(manager.sourceFor(source, edward::media::PreviewQuality::Clear) == *proxy);
  assert(!manager.ensureProxy({}, edward::media::PreviewQuality::Clear));
  return 0;
}
