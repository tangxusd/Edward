#include <edward/core/timeline.hpp>
#include <edward/desktop/timeline_controller.hpp>

#include <cassert>

int main(int argc, char** argv) {
  assert(argc == 2);
  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  edward::desktop::TimelineController controller(timeline, track);
  assert(controller.dropMediaAtPlayhead(argv[1]));
  assert(timeline.clips(track).size() == 1);
  assert(controller.selectedClip() == 1);
  assert(controller.selectClip(1));
  assert(!controller.selectClip(999));
  assert(!controller.dropMediaAtPlayhead(argv[1]));
  assert(controller.setPlayhead(10));
  assert(controller.splitSelectedAtPlayhead());
  assert(timeline.clips(track).size() == 2);
  assert(controller.rippleDeleteSelected());
  assert(timeline.clips(track).size() == 1);
  return 0;
}
