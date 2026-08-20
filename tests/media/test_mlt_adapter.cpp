#include <edward/core/timeline.hpp>
#include <edward/media/mlt_adapter.hpp>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 3);
  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, argv[1], 0, 25, 0}));

  const edward::media::MltAdapter adapter;
  const auto first = adapter.renderFrame(timeline.snapshot(), 0);
  const auto later = adapter.renderFrame(timeline.snapshot(), 12);
  assert(first.has_value());
  assert(later.has_value());
  assert(first->width() == 16 && first->height() == 16);
  assert(later->size() == first->size());
  const auto sourceFrame = adapter.renderSourceFrame(argv[1], 12);
  assert(sourceFrame.has_value());
  assert(sourceFrame->size() == first->size());
  assert(!adapter.renderSourceFrame({}, 0).has_value());
  edward::core::Timeline timelineWithGap(25);
  const auto gapTrack = timelineWithGap.addVideoTrack();
  assert(timelineWithGap.insertClip({1, gapTrack, argv[1], 0, 12, 0}));
  const auto gapFrame = adapter.renderFrame(timelineWithGap.snapshot(), 20);
  assert(gapFrame.has_value());
  assert(gapFrame->size() == first->size());
  assert(gapFrame->pixelColor(0, 0) == QColor(Qt::black));
  const auto overlayTrack = timeline.addVideoTrack();
  assert(timeline.insertClip({2, overlayTrack, argv[2], 0, 25, 0}));
  edward::core::Timeline overlayOnlyTimeline(25);
  const auto overlayOnlyTrack = overlayOnlyTimeline.addVideoTrack();
  assert(overlayOnlyTimeline.insertClip({1, overlayOnlyTrack, argv[2], 0, 25, 0}));
  const auto overlayOnly = adapter.renderFrame(overlayOnlyTimeline.snapshot(), 12);
  assert(overlayOnly.has_value());
  assert(overlayOnly->pixelColor(8, 8).red() > overlayOnly->pixelColor(8, 8).green());
  const auto composited = adapter.renderFrame(timeline.snapshot(), 12);
  assert(composited.has_value());
  assert(composited->pixelColor(8, 8).red() > composited->pixelColor(8, 8).green());
  assert(!adapter.renderFrame(timeline.snapshot(), 25).has_value());
  return 0;
}
