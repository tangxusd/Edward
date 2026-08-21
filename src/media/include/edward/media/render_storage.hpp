#pragma once

#include "edward/core/project_identity.hpp"

#include <filesystem>

namespace edward::media {

struct RenderStorageRoots {
  std::filesystem::path proxyRoot;
  std::filesystem::path cacheRoot;
  std::filesystem::path renderRoot;
};

struct RenderStoragePaths {
  std::filesystem::path proxyDirectory;
  std::filesystem::path cacheDirectory;
  std::filesystem::path renderDirectory;
};

enum class DerivedStorageKind { Proxy, Cache, Render };

class RenderStorage {
 public:
  static RenderStoragePaths paths(const edward::core::ProjectIdentity& project,
                                  const RenderStorageRoots& roots);
  static bool clearDerived(const RenderStorageRoots& roots, DerivedStorageKind kind);
};

}  // namespace edward::media
