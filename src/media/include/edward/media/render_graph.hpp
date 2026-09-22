#pragma once

#include "edward/core/timeline.hpp"
#include "edward/media/mlt_adapter.hpp"

#include <QByteArray>
#include <QImage>

#include <filesystem>
#include <functional>
#include <optional>

namespace edward::media {
struct RenderRequest { edward::core::Frame frame = 0; };
struct RenderScene { QImage frame; };
class RenderGraph {
 public:
  explicit RenderGraph(const MltAdapter& adapter) : adapter_(adapter) {}
  void setPluginFrame(std::optional<QImage> frame) { pluginFrame_ = std::move(frame); }
  void setPluginFrameProvider(std::function<std::optional<QImage>(edward::core::Frame)> provider) { pluginFrameProvider_ = std::move(provider); }
  void setPluginVideo(const std::filesystem::path& path, edward::core::Frame sourceIn = 0);
  [[nodiscard]] QByteArray cacheSignature() const;
  std::optional<RenderScene> build(const edward::core::TimelineSnapshot& snapshot, const RenderRequest& request) const;
 private:
  const MltAdapter& adapter_;
  std::optional<QImage> pluginFrame_;
  std::function<std::optional<QImage>(edward::core::Frame)> pluginFrameProvider_;
};
}  // namespace edward::media
