#include "edward/media/preview_session.hpp"

namespace edward::media {

PreviewSession::PreviewSession(RenderStorageRoots roots, edward::core::ProjectIdentity project)
    : proxyManager_(std::move(roots), std::move(project)) {}

void PreviewSession::setQuality(PreviewQuality quality) {
  quality_ = quality;
}

PreviewQuality PreviewSession::quality() const {
  return quality_;
}

bool PreviewSession::prepare(const std::filesystem::path& source) const {
  return quality_ == PreviewQuality::Original || proxyManager_.ensureProxy(source, quality_).has_value();
}

std::filesystem::path PreviewSession::sourceFor(const std::filesystem::path& source) const {
  return proxyManager_.sourceFor(source, quality_);
}

}  // namespace edward::media
