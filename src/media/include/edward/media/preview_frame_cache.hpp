#pragma once

#include "edward/core/project_identity.hpp"
#include "edward/media/render_storage.hpp"

#include <QByteArray>
#include <QImage>

#include <optional>

namespace edward::media {

class PreviewFrameCache final {
 public:
  PreviewFrameCache(RenderStorageRoots roots, edward::core::ProjectIdentity project);

  [[nodiscard]] std::optional<QImage> load(const QByteArray& key) const;
  [[nodiscard]] bool store(const QByteArray& key, const QImage& frame) const;

 private:
  [[nodiscard]] std::filesystem::path pathFor(const QByteArray& key) const;

  RenderStorageRoots roots_;
  edward::core::ProjectIdentity project_;
};

}  // namespace edward::media
