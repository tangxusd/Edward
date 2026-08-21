#include <edward/core/timeline.hpp>
#include <edward/media/export_job.hpp>
#include <edward/media/media_probe.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <QCoreApplication>

#include <cassert>
#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace {
double peakRssMb() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) return 0.0;
  return static_cast<double>(counters.PeakWorkingSetSize) / (1024.0 * 1024.0);
#else
  rusage usage{};
  assert(getrusage(RUSAGE_SELF, &usage) == 0);
#if defined(__APPLE__)
  return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
#endif
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  assert(argc == 2);
  const std::filesystem::path input = argv[1];
  const std::filesystem::path output = std::filesystem::path(argv[1]).parent_path() / "edward-4k-memory-guard.mp4";
  std::error_code error;
  std::filesystem::remove(output, error);
  const auto info = edward::media::MediaProbe::probe(input);
  assert(info && info->width == 3840 && info->height == 2160 && info->durationFrames > 0);
  edward::core::Timeline timeline(info->durationFrames);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, input, 0, info->durationFrames, 0}));
  const edward::media::MltAdapter adapter;
  const edward::media::RenderGraph graph(adapter);
  const edward::media::ExportJob job(graph);
  const auto result = job.run(timeline.snapshot(), {output, {3840, 2160}, info->fpsNumerator,
                                                     info->fpsDenominator});
  assert(result && result->frameCount == info->durationFrames);
  assert(std::filesystem::is_regular_file(output));
  const auto exported = edward::media::MediaProbe::probe(output);
  assert(exported && exported->width == 3840 && exported->height == 2160);
  assert(peakRssMb() < 2048.0);
  std::filesystem::remove(output, error);
  return 0;
}
