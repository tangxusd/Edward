#pragma once

#include "edward/media/render_storage.hpp"

#include <filesystem>
#include <optional>

namespace edward::media {

enum class PreviewQuality { Original, Clear, Fluent };

class ProxyManager {
 public:
  ProxyManager(RenderStorageRoots roots, edward::core::ProjectIdentity project);

  std::optional<std::filesystem::path> ensureProxy(const std::filesystem::path& source,
                                                   PreviewQuality quality) const;
  std::filesystem::path sourceFor(const std::filesystem::path& source,
                                  PreviewQuality quality) const;

 private:
  std::optional<std::filesystem::path> proxyPath(const std::filesystem::path& source,
                                                 PreviewQuality quality) const;

  RenderStorageRoots roots_;
  edward::core::ProjectIdentity project_;
};

}  // namespace edward::media
