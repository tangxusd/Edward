#include "edward/media/render_storage.hpp"

namespace edward::media {
namespace {

const std::filesystem::path& rootFor(const RenderStorageRoots& roots, DerivedStorageKind kind) {
  switch (kind) {
    case DerivedStorageKind::Proxy: return roots.proxyRoot;
    case DerivedStorageKind::Cache: return roots.cacheRoot;
    case DerivedStorageKind::Render: return roots.renderRoot;
  }
  return roots.proxyRoot;
}

bool isProjectDirectory(const std::filesystem::directory_entry& entry) {
  if (!entry.is_directory()) return false;
  return edward::core::ProjectIdentity::parse(QString::fromStdString(entry.path().filename().string())).has_value();
}

}  // namespace

RenderStoragePaths RenderStorage::paths(const edward::core::ProjectIdentity& project,
                                        const RenderStorageRoots& roots) {
  const auto name = project.value().toStdString();
  return {roots.proxyRoot / name, roots.cacheRoot / name, roots.renderRoot / name};
}

bool RenderStorage::clearDerived(const RenderStorageRoots& roots, DerivedStorageKind kind) {
  const auto& root = rootFor(roots, kind);
  if (root.empty()) return false;
  std::error_code error;
  if (!std::filesystem::exists(root, error)) return !error;
  for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
    if (error) return false;
    if (isProjectDirectory(entry)) std::filesystem::remove_all(entry.path(), error);
    if (error) return false;
  }
  return true;
}

}  // namespace edward::media
