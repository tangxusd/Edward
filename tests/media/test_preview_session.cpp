#include <edward/core/project_identity.hpp>
#include <edward/media/preview_session.hpp>

#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 2);
  QTemporaryDir temporary;
  assert(temporary.isValid());
  const auto root = std::filesystem::path(temporary.path().toStdString());
  const edward::media::RenderStorageRoots roots{root / "proxies", root / "cache", root / "renders"};
  edward::media::PreviewSession session(roots, edward::core::ProjectIdentity::create());
  const auto source = std::filesystem::path(argv[1]);
  assert(session.quality() == edward::media::PreviewQuality::Original);
  assert(session.sourceFor(source) == source);
  session.setQuality(edward::media::PreviewQuality::Clear);
  assert(session.sourceFor(source) == source);
  assert(session.prepare(source));
  assert(session.sourceFor(source) != source);
  session.setQuality(edward::media::PreviewQuality::Fluent);
  assert(session.sourceFor(source) == source);
  assert(session.prepare(source));
  assert(session.sourceFor(source) != source);
  session.setQuality(edward::media::PreviewQuality::Original);
  assert(session.sourceFor(source) == source);
  return 0;
}
