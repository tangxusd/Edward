#include <edward/core/project_identity.hpp>
#include <edward/media/media_probe.hpp>
#include <edward/media/proxy_manager.hpp>

#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 3);
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
  const auto largeSource = std::filesystem::path(argv[2]);
  const auto clearProxy = manager.ensureProxy(largeSource, edward::media::PreviewQuality::Clear);
  assert(clearProxy);
  const auto clearInfo = edward::media::MediaProbe::probe(*clearProxy);
  assert(clearInfo && clearInfo->width <= 1920 && clearInfo->height <= 1080);
  const auto fluentProxy = manager.ensureProxy(largeSource, edward::media::PreviewQuality::Fluent);
  assert(fluentProxy);
  const auto fluentInfo = edward::media::MediaProbe::probe(*fluentProxy);
  assert(fluentInfo && fluentInfo->width <= 854 && fluentInfo->height <= 480);
  assert(!manager.ensureProxy({}, edward::media::PreviewQuality::Clear));
  return 0;
}
