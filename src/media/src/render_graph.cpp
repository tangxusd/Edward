#include "edward/media/render_graph.hpp"

#include "edward/media/component_renderer.hpp"

#include <QPainter>
#include <algorithm>
#include <vector>

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
  for (const auto& transition : snapshot.transitions) {
    if (transition.type != edward::core::TransitionType::Dissolve) continue;
    const auto endFrame = transition.startFrame + transition.durationFrames;
    if (request.frame < transition.startFrame || request.frame >= endFrame) continue;
    const auto left = std::ranges::find_if(snapshot.clips, [&](const auto& clip) {
      return clip.id == transition.leftClipId;
    });
    const auto right = std::ranges::find_if(snapshot.clips, [&](const auto& clip) {
      return clip.id == transition.rightClipId;
    });
    if (left == snapshot.clips.end() || right == snapshot.clips.end() ||
        left->kind != edward::core::TimelineClipKind::Media || right->kind != edward::core::TimelineClipKind::Media)
      continue;
    const auto outgoing = adapter_.renderSourceFrame(left->source,
        left->sourceIn + request.frame - left->timelineStart);
    const auto incoming = adapter_.renderSourceFrame(right->source,
        right->sourceIn + request.frame - transition.startFrame);
    if (!outgoing || !incoming || outgoing->size() != incoming->size() || outgoing->size() != frame->size()) continue;
    const auto progress = std::clamp(static_cast<double>(request.frame - transition.startFrame) /
                                         std::max<edward::core::Frame>(1, transition.durationFrames - 1),
                                     0.0, 1.0);
    QImage blended(frame->size(), QImage::Format_RGBA8888);
    for (int y = 0; y < blended.height(); ++y) {
      for (int x = 0; x < blended.width(); ++x) {
        const auto a = outgoing->pixelColor(x, y);
        const auto b = incoming->pixelColor(x, y);
        blended.setPixelColor(x, y, QColor(
            static_cast<int>(a.red() * (1.0 - progress) + b.red() * progress + 0.5),
            static_cast<int>(a.green() * (1.0 - progress) + b.green() * progress + 0.5),
            static_cast<int>(a.blue() * (1.0 - progress) + b.blue() * progress + 0.5),
            static_cast<int>(a.alpha() * (1.0 - progress) + b.alpha() * progress + 0.5)));
      }
    }
    *frame = std::move(blended);
    break;
  }
  const auto renderOverlay = [&](const edward::core::ComponentIr& component, edward::core::Frame componentFrame) {
    const auto layer = ComponentRenderer{}.render(component, componentFrame, frame->size());
    if (layer.isNull()) return;
    QPainter painter(&*frame);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, layer);
  };
  if (overlay_) renderOverlay(*overlay_, request.frame);
  std::vector<edward::core::ClipId> dissolvedComponentIds;
  for (const auto& transition : snapshot.transitions) {
    if (transition.type != edward::core::TransitionType::Dissolve) continue;
    const auto endFrame = transition.startFrame + transition.durationFrames;
    if (request.frame < transition.startFrame || request.frame >= endFrame) continue;
    const auto left = std::ranges::find_if(snapshot.clips, [&](const auto& clip) {
      return clip.id == transition.leftClipId;
    });
    const auto right = std::ranges::find_if(snapshot.clips, [&](const auto& clip) {
      return clip.id == transition.rightClipId;
    });
    if (left == snapshot.clips.end() || right == snapshot.clips.end() ||
        left->kind != edward::core::TimelineClipKind::Component ||
        right->kind != edward::core::TimelineClipKind::Component ||
        !left->component || !right->component) continue;
    const auto progress = std::clamp(static_cast<double>(request.frame - transition.startFrame) /
                                         std::max<edward::core::Frame>(1, transition.durationFrames - 1),
                                     0.0, 1.0);
    const auto outgoing = ComponentRenderer{}.render(*left->component,
        left->sourceIn + request.frame - left->timelineStart, frame->size());
    const auto incoming = ComponentRenderer{}.render(*right->component,
        right->sourceIn + request.frame - transition.startFrame, frame->size());
    if (outgoing.isNull() || incoming.isNull()) continue;
    QImage blended(frame->size(), QImage::Format_RGBA8888);
    blended.fill(Qt::transparent);
    for (int y = 0; y < blended.height(); ++y) {
      for (int x = 0; x < blended.width(); ++x) {
        const auto a = outgoing.pixelColor(x, y);
        const auto b = incoming.pixelColor(x, y);
        blended.setPixelColor(x, y, QColor(
            static_cast<int>(a.red() * (1.0 - progress) + b.red() * progress + 0.5),
            static_cast<int>(a.green() * (1.0 - progress) + b.green() * progress + 0.5),
            static_cast<int>(a.blue() * (1.0 - progress) + b.blue() * progress + 0.5),
            static_cast<int>(a.alpha() * (1.0 - progress) + b.alpha() * progress + 0.5)));
      }
    }
    QPainter painter(&*frame);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, blended);
    dissolvedComponentIds.push_back(left->id);
    dissolvedComponentIds.push_back(right->id);
  }
  for (const auto& clip : snapshot.clips) {
    const auto duration = clip.sourceOut - clip.sourceIn;
    if (clip.kind == edward::core::TimelineClipKind::Component && clip.component &&
        request.frame >= clip.timelineStart && request.frame < clip.timelineStart + duration &&
        std::ranges::find(dissolvedComponentIds, clip.id) == dissolvedComponentIds.end())
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
