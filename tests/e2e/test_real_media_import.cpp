#include <edward/core/timeline.hpp>
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
    const edward::core::TimelineClip clip{1, track, path, 0, info->durationFrames, 0};
    assert(timeline.insertClip(clip));

    const auto clips = timeline.clips(track);
    assert(clips.size() == 1);
    assert(clips.front().source == path);
    assert(clips.front().sourceIn == 0);
    assert(clips.front().sourceOut == info->durationFrames);
    assert(clips.front().timelineStart == 0);

    const auto splitFrame = std::min<edward::core::Frame>(10, info->durationFrames - 1);
    assert(splitFrame > 0);
    assert(timeline.setPlayhead(splitFrame));
    assert(timeline.replaceClip(1, {1, track, path, 0, splitFrame, 0}));
    assert(timeline.insertClip({2, track, path, splitFrame, info->durationFrames, splitFrame}));
    assert(timeline.removeClip(1));
    assert(timeline.replaceClip(2, {2, track, path, splitFrame, info->durationFrames, 0}));
    const auto remaining = timeline.clips(track);
    assert(remaining.size() == 1);
    assert(remaining.front().sourceOut - remaining.front().sourceIn == info->durationFrames - splitFrame);
    assert(remaining.front().timelineStart == 0);

    const edward::media::MltAdapter adapter;
    const auto rendered = adapter.renderFrame(timeline.snapshot(), 0);
    assert(rendered && !rendered->isNull());
  }
  return 0;
}
