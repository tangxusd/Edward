#include <edward/core/timeline.hpp>
#include <edward/desktop/timeline_controller.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/media_probe.hpp>

#include <QCoreApplication>

#include <cassert>
#include <algorithm>
#include <filesystem>

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
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

    const auto splitFrame = std::min<edward::core::Frame>(10, info->durationFrames - 1);
    assert(splitFrame > 0);
    assert(controller.setPlayhead(splitFrame));
    assert(controller.splitSelectedAtPlayhead());
    assert(controller.rippleDeleteSelected());
    const auto remaining = timeline.clips(controller.targetTrack());
    assert(remaining.size() == 1);
    assert(remaining.front().sourceOut - remaining.front().sourceIn == info->durationFrames - splitFrame);
    assert(remaining.front().timelineStart == 0);

    const edward::media::MltAdapter adapter;
    const auto rendered = adapter.renderFrame(timeline.snapshot(), 0);
    assert(rendered && !rendered->isNull());
  }
  return 0;
}
