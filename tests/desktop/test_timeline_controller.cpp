#include <edward/core/timeline.hpp>
#include <edward/desktop/timeline_controller.hpp>

#include <cassert>

int main(int argc, char** argv) {
  assert(argc == 2);
  edward::core::Timeline timeline(50);
  const auto track = timeline.addVideoTrack();
  edward::desktop::TimelineController controller(timeline, track);
  assert(controller.dropMediaAtPlayhead(argv[1]));
  assert(timeline.clips(track).size() == 1);
  assert(controller.selectedClip() == 1);
  assert(controller.selectClip(1));
  assert(!controller.selectClip(999));
  assert(controller.dropMediaAtPlayhead(argv[1]));
  const auto tracks = timeline.snapshot().videoTracks;
  assert(tracks.size() == 2);
  assert(timeline.clips(tracks[0]).size() == 1);
  assert(timeline.clips(tracks[1]).size() == 1);
  assert(controller.selectedClip() == 2);
  assert(controller.moveSelectedTo(10));
  assert(timeline.clips(tracks[1]).front().timelineStart == 10);
  assert(controller.undo());
  assert(timeline.clips(tracks[1]).front().timelineStart == 0);
  assert(controller.redo());
  assert(timeline.clips(tracks[1]).front().timelineStart == 10);
  assert(controller.setPlayhead(20));
  assert(controller.splitSelectedAtPlayhead());
  assert(timeline.clips(tracks[1]).size() == 2);
  assert(controller.deleteSelected());
  assert(timeline.clips(tracks[1]).size() == 1);
  assert(timeline.clips(tracks[1]).front().timelineStart == 20);
  assert(controller.selectClip(3));
  assert(controller.rippleDeleteSelected());
  assert(timeline.clips(tracks[1]).empty());
  return 0;
}
