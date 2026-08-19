#include <edward/core/component_ir.hpp>
#include <edward/core/timeline.hpp>
#include <edward/desktop/timeline_controller.hpp>
#include <edward/media/export_job.hpp>
#include <edward/media/media_probe.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <QJsonArray>
#include <QJsonObject>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 3);
  const std::filesystem::path output = argv[2];
  std::error_code error;
  std::filesystem::remove(output, error);

  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  edward::desktop::TimelineController controller(timeline, track);
  assert(controller.dropMediaAtPlayhead(argv[1]));
  assert(controller.setPlayhead(10));
  assert(controller.splitSelectedAtPlayhead());
  assert(controller.rippleDeleteSelected());
  assert(timeline.clips(track).size() == 1);
  assert(timeline.clips(track).front().timelineStart == 0);

  const QJsonObject root{{"id", "root"}, {"type", "container"}, {"children", QJsonArray{
      QJsonObject{{"id", "overlay"}, {"type", "shape"},
                  {"transform", QJsonObject{{"width", 8}, {"height", 8}}},
                  {"properties", QJsonObject{{"fill", "#ff0000"}}}},
  }}};
  const auto overlay = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(overlay);

  const edward::media::MltAdapter adapter;
  const edward::media::RenderGraph graph(adapter, *overlay);
  const edward::media::ExportJob job(graph);
  const auto result = job.run(timeline.snapshot(), {output, {1920, 1080}, 25, 1});
  assert(result && result->frameCount == 25);
  const auto info = edward::media::MediaProbe::probe(output);
  assert(info && info->durationFrames == 25);
  assert(std::filesystem::is_regular_file(output));

  edward::core::Timeline exportedTimeline(25);
  const auto exportedTrack = exportedTimeline.addVideoTrack();
  assert(exportedTimeline.insertClip({1, exportedTrack, output, 0, 25, 0}));
  const auto rendered = adapter.renderFrame(exportedTimeline.snapshot(), 0);
  assert(rendered);
  const auto center = rendered->pixelColor(rendered->width() / 2, rendered->height() / 2);
  assert(center.red() > center.green() + 80);
  assert(center.red() > center.blue() + 80);
  std::filesystem::remove(output, error);
  return 0;
}
