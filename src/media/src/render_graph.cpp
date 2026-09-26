#include "edward/media/render_graph.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

namespace edward::media {
void RenderGraph::setPluginVideo(const std::filesystem::path& path, edward::core::Frame sourceIn) {
  if (path.empty() || sourceIn < 0) { pluginFrameProvider_ = {}; return; }
  pluginFrameProvider_ = [this, path, sourceIn](edward::core::Frame frame) { return adapter_.renderSourceFrame(path, sourceIn + frame); };
}
QByteArray RenderGraph::cacheSignature() const {
  return QJsonDocument(QJsonObject{{QStringLiteral("pluginFrame"), pluginFrame_.has_value()}, {QStringLiteral("pluginProvider"), static_cast<bool>(pluginFrameProvider_)}}).toJson(QJsonDocument::Compact);
}
std::optional<RenderScene> RenderGraph::build(const edward::core::TimelineSnapshot& snapshot, const RenderRequest& request) const {
  auto frame = adapter_.renderFrame(snapshot, request.frame);
  if (!frame) return std::nullopt;
  const auto composite = [&frame](const std::optional<QImage>& layer) { if (layer && layer->size() == frame->size() && layer->hasAlphaChannel()) { QPainter painter(&*frame); painter.drawImage(0, 0, *layer); } };
  composite(pluginFrame_);
  if (pluginFrameProvider_) composite(pluginFrameProvider_(request.frame));
  return RenderScene{*frame};
}
}  // namespace edward::media
