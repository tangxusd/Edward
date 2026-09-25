#include "edward/core/timeline_commands.hpp"

#include <cassert>

int main() {
  edward::core::Timeline timeline(120);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, "source.mp4", 0, 60, 0}));
  edward::core::TimelineCommands commands(timeline);
  assert(commands.addMarker(10));
  assert(commands.addMarker(20, edward::core::MarkerScope::Clip, 1, edward::core::MarkerColor::Blue));
  assert(timeline.markers().size() == 2);
  assert(commands.undo());
  assert(timeline.markers().size() == 1);
  assert(commands.redo());
  assert(timeline.markers().size() == 2);
  assert(commands.setMarkerColor(timeline.markers().front().id, edward::core::MarkerColor::Green));
  return 0;
}
