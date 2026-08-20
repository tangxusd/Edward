#include "edward/media/render_graph.hpp"

#include "edward/media/component_renderer.hpp"

#include <QPainter>

namespace edward::media {

RenderGraph::RenderGraph(const MltAdapter& adapter, std::optional<edward::core::ComponentIr> overlay)
    : adapter_(adapter), overlay_(std::move(overlay)) {}

void RenderGraph::setOverlay(std::optional<edward::core::ComponentIr> overlay) {
  overlay_ = std::move(overlay);
}

void RenderGraph::setComponentLayers(std::vector<ComponentLayer> layers) {
  componentLayers_.clear();
  for (auto& layer : layers) {
    if (layer.startFrame < 0 || layer.endFrame <= layer.startFrame || !layer.component.validate()) continue;
    componentLayers_.push_back(std::move(layer));
  }
}

void RenderGraph::setPluginFrame(std::optional<QImage> frame) {
  pluginFrame_ = std::move(frame);
}

std::optional<RenderScene> RenderGraph::build(const edward::core::TimelineSnapshot& snapshot,
                                              const RenderRequest& request) const {
  auto frame = adapter_.renderFrame(snapshot, request.frame);
  if (!frame) return std::nullopt;
  const auto renderOverlay = [&](const edward::core::ComponentIr& component, edward::core::Frame componentFrame) {
    const auto layer = ComponentRenderer{}.render(component, componentFrame, frame->size());
    if (layer.isNull()) return;
    QPainter painter(&*frame);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, layer);
  };
  if (overlay_) renderOverlay(*overlay_, request.frame);
  for (const auto& clip : snapshot.clips) {
    const auto duration = clip.sourceOut - clip.sourceIn;
    if (clip.kind == edward::core::TimelineClipKind::Component && clip.component &&
        request.frame >= clip.timelineStart && request.frame < clip.timelineStart + duration)
      renderOverlay(*clip.component, clip.sourceIn + request.frame - clip.timelineStart);
  }
  for (const auto& componentLayer : componentLayers_) {
    if (request.frame >= componentLayer.startFrame && request.frame < componentLayer.endFrame)
      renderOverlay(componentLayer.component, componentLayer.sourceIn + request.frame - componentLayer.startFrame);
  }
  for (const auto& transition : snapshot.transitions) {
    const auto endFrame = transition.startFrame + transition.durationFrames;
    if (request.frame < transition.startFrame || request.frame >= endFrame) continue;
    if (transition.type == edward::core::TransitionType::FlashBlack ||
        transition.type == edward::core::TransitionType::FlashWhite) {
      QPainter painter(&*frame);
      painter.fillRect(frame->rect(), transition.type == edward::core::TransitionType::FlashBlack
                                      ? QColor(Qt::black) : QColor(Qt::white));
    }
  }
  if (pluginFrame_ && pluginFrame_->size() == frame->size() && pluginFrame_->hasAlphaChannel()) {
    QPainter painter(&*frame);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, *pluginFrame_);
  }
  return std::optional<RenderScene>(RenderScene{*frame});
}

}  // namespace edward::media
