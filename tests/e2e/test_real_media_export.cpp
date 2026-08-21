#include <edward/core/component_ir.hpp>
#include <edward/core/timeline.hpp>
#include <edward/media/export_job.hpp>
#include <edward/media/media_probe.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  assert(argc == 3);
  const std::filesystem::path input = argv[1];
  const std::filesystem::path output = argv[2];
  std::error_code error;
  std::filesystem::remove(output, error);

  const auto info = edward::media::MediaProbe::probe(input);
  assert(info && info->durationFrames > 0);
  edward::core::Timeline timeline(info->durationFrames);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, input, 0, info->durationFrames, 0}));

  const QJsonObject root{{"id", "root"}, {"type", "container"}, {"children", QJsonArray{}}};
  const auto component = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(component);
  const edward::media::MltAdapter adapter;
  const edward::media::RenderGraph graph(adapter, *component);
  const edward::media::ExportJob job(graph);
  const auto result = job.run(timeline.snapshot(), {output, {640, 360}, info->fpsNumerator, info->fpsDenominator});
  assert(result && result->frameCount == info->durationFrames);
  const auto exported = edward::media::MediaProbe::probe(output);
  assert(exported && exported->width == 640 && exported->height == 360);
  assert(exported->durationFrames > 0);
  assert(std::filesystem::is_regular_file(output));
  return 0;
}
