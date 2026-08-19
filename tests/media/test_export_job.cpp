#include <edward/core/timeline.hpp>
#include <edward/core/component_ir.hpp>
#include <edward/media/export_job.hpp>
#include <edward/media/media_probe.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <cassert>
#include <filesystem>

#include <QJsonArray>
#include <QJsonObject>

int main(int argc, char** argv) {
  assert(argc == 3);
  const std::filesystem::path output = argv[2];
  std::filesystem::remove(output);
  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, argv[1], 0, 25, 0}));
  const edward::media::MltAdapter adapter;
  const QJsonObject root{{"id", "root"}, {"type", "container"}, {"children", QJsonArray{
      QJsonObject{{"id", "overlay"}, {"type", "shape"},
                  {"transform", QJsonObject{{"width", 8}, {"height", 8}}},
                  {"properties", QJsonObject{{"fill", "#ff0000"}}}},
  }}};
  const auto overlay = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(overlay);
  const edward::media::RenderGraph graph(adapter, *overlay);
  const edward::media::ExportJob job(graph);
  const auto result = job.run(timeline.snapshot(), {output, {1920, 1080}, 25, 1});
  assert(result.has_value());
  assert(result->frameCount == 25);
  const auto info = edward::media::MediaProbe::probe(output);
  assert(info.has_value());
  assert(info->width == 1920 && info->height == 1080);
  assert(info->durationFrames == 25);
  assert(info->hasAudio);
  edward::core::Timeline renderedTimeline(25);
  const auto renderedTrack = renderedTimeline.addVideoTrack();
  assert(renderedTimeline.insertClip({1, renderedTrack, output, 0, 25, 0}));
  const auto exportedFrame = adapter.renderFrame(renderedTimeline.snapshot(), 0);
  assert(exportedFrame);
  const auto center = exportedFrame->pixelColor(exportedFrame->width() / 2, exportedFrame->height() / 2);
  assert(center.red() > center.green() + 80);
  assert(center.red() > center.blue() + 80);
  std::filesystem::remove(output);
  return 0;
}
