#include "edward/media/render_graph.hpp"

namespace edward::media {

RenderGraph::RenderGraph(const MltAdapter& adapter) : adapter_(adapter) {}

std::optional<RenderScene> RenderGraph::build(const edward::core::TimelineSnapshot& snapshot,
                                              const RenderRequest& request) const {
  const auto frame = adapter_.renderFrame(snapshot, request.frame);
  return frame ? std::optional<RenderScene>(RenderScene{*frame}) : std::nullopt;
}

}  // namespace edward::media
