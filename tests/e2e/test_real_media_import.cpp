#include <edward/core/timeline.hpp>
#include <edward/desktop/timeline_controller.hpp>
#include <edward/media/media_probe.hpp>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc >= 2);
  for (int index = 1; index < argc; ++index) {
    const std::filesystem::path path = argv[index];
    const auto info = edward::media::MediaProbe::probe(path);
    assert(info && info->durationFrames > 0);

    edward::core::Timeline timeline(info->durationFrames);
    const auto track = timeline.addVideoTrack();
    edward::desktop::TimelineController controller(timeline, track);
    assert(controller.dropMediaAtPlayhead(QString::fromStdString(path.string())));

    const auto clips = timeline.clips(controller.targetTrack());
    assert(clips.size() == 1);
    assert(clips.front().source == path);
    assert(clips.front().sourceIn == 0);
    assert(clips.front().sourceOut == info->durationFrames);
    assert(clips.front().timelineStart == 0);
  }
  return 0;
}
