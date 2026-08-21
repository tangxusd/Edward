#pragma once

#include "edward/media/proxy_manager.hpp"

namespace edward::media {

class PreviewSession {
 public:
  PreviewSession(RenderStorageRoots roots, edward::core::ProjectIdentity project);

  void setQuality(PreviewQuality quality);
  PreviewQuality quality() const;
  bool prepare(const std::filesystem::path& source) const;
  std::filesystem::path sourceFor(const std::filesystem::path& source) const;

 private:
  ProxyManager proxyManager_;
  PreviewQuality quality_ = PreviewQuality::Original;
};

}  // namespace edward::media
