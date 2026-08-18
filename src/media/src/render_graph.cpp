#include "edward/media/render_graph.hpp"

#include "edward/media/component_renderer.hpp"

#include <QPainter>

namespace edward::media {

RenderGraph::RenderGraph(const MltAdapter& adapter, std::optional<edward::core::ComponentIr> overlay)
    : adapter_(adapter), overlay_(std::move(overlay)) {}

void RenderGraph::setOverlay(std::optional<edward::core::ComponentIr> overlay) {
  overlay_ = std::move(overlay);
}

std::optional<RenderScene> RenderGraph::build(const edward::core::TimelineSnapshot& snapshot,
                                              const RenderRequest& request) const {
  auto frame = adapter_.renderFrame(snapshot, request.frame);
  if (!frame) return std::nullopt;
  if (overlay_) {
    const auto layer = ComponentRenderer{}.render(*overlay_, request.frame, frame->size());
    if (!layer.isNull()) {
      QPainter painter(&*frame);
      painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
      painter.drawImage(0, 0, layer);
    }
  }
  return std::optional<RenderScene>(RenderScene{*frame});
}

}  // namespace edward::media
