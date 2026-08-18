#include <edward/core/timeline.hpp>
#include <edward/media/export_job.hpp>
#include <edward/media/media_probe.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 3);
  const std::filesystem::path output = argv[2];
  std::filesystem::remove(output);
  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, argv[1], 0, 25, 0}));
  const edward::media::MltAdapter adapter;
  const edward::media::RenderGraph graph(adapter);
  const edward::media::ExportJob job(graph);
  const auto result = job.run(timeline.snapshot(), {output, {1920, 1080}, 25, 1});
  assert(result.has_value());
  assert(result->frameCount == 25);
  const auto info = edward::media::MediaProbe::probe(output);
  assert(info.has_value());
  assert(info->width == 1920 && info->height == 1080);
  assert(info->durationFrames == 25);
  std::filesystem::remove(output);
  return 0;
}
