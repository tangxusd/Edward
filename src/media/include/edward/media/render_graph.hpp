#pragma once

#include "edward/core/timeline.hpp"
#include "edward/core/component_ir.hpp"
#include "edward/media/mlt_adapter.hpp"

#include <QImage>

#include <optional>
#include <vector>

namespace edward::media {

struct RenderRequest {
  edward::core::Frame frame = 0;
};

struct RenderScene {
  QImage frame;
};

struct ComponentLayer final {
  edward::core::Frame startFrame = 0;
  edward::core::Frame endFrame = 0;
  edward::core::Frame sourceIn = 0;
  edward::core::ComponentIr component;
};

class RenderGraph {
 public:
  explicit RenderGraph(const MltAdapter& adapter,
                       std::optional<edward::core::ComponentIr> overlay = std::nullopt);
  void setOverlay(std::optional<edward::core::ComponentIr> overlay);
  void setComponentLayers(std::vector<ComponentLayer> layers);
  void setPluginFrame(std::optional<QImage> frame);
  std::optional<RenderScene> build(const edward::core::TimelineSnapshot& snapshot,
                                   const RenderRequest& request) const;

 private:
  const MltAdapter& adapter_;
  std::optional<edward::core::ComponentIr> overlay_;
  std::vector<ComponentLayer> componentLayers_;
  std::optional<QImage> pluginFrame_;
};

}  // namespace edward::media
