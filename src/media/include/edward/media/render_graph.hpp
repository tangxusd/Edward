#pragma once

#include "edward/core/timeline.hpp"
#include "edward/core/component_ir.hpp"
#include "edward/media/mlt_adapter.hpp"

#include <QImage>

#include <optional>

namespace edward::media {

struct RenderRequest {
  edward::core::Frame frame = 0;
};

struct RenderScene {
  QImage frame;
};

class RenderGraph {
 public:
  explicit RenderGraph(const MltAdapter& adapter,
                       std::optional<edward::core::ComponentIr> overlay = std::nullopt);
  void setOverlay(std::optional<edward::core::ComponentIr> overlay);
  void setPluginFrame(std::optional<QImage> frame);
  std::optional<RenderScene> build(const edward::core::TimelineSnapshot& snapshot,
                                   const RenderRequest& request) const;

 private:
  const MltAdapter& adapter_;
  std::optional<edward::core::ComponentIr> overlay_;
  std::optional<QImage> pluginFrame_;
};

}  // namespace edward::media
