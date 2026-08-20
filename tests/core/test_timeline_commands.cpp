#include <edward/core/timeline_commands.hpp>

#include <cassert>

using namespace edward::core;

int main() {
  Timeline timeline(300);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, "source.mp4", 0, 100, 0}));
  assert(timeline.insertClip({2, track, "source.mp4", 100, 180, 150}));
  TimelineCommands commands(timeline);

  assert(!commands.setPlayhead(301));
  assert(commands.setPlayhead(40));
  assert(commands.splitClipAtPlayhead(1));
  assert(timeline.clips(track).size() == 3);
  assert(timeline.clip(1)->sourceOut == 40);
  assert(timeline.clip(3)->timelineStart == 40);
  assert(!commands.moveClip(3, 160));
  assert(commands.trimClip(3, 40, 80));
  assert(timeline.clip(3)->sourceOut == 80);
  assert(commands.deleteClip(3));
  assert(timeline.clips(track).size() == 2);
  assert(commands.undo());
  assert(timeline.clip(3));
  assert(commands.rippleDelete(track, 3));
  assert(timeline.clip(2)->timelineStart == 110);
  assert(commands.undo());
  assert(timeline.clip(2)->timelineStart == 150);
  assert(commands.redo());
  assert(timeline.clip(2)->timelineStart == 110);
  Timeline restored(300);
  restored.addVideoTrack();
  assert(restored.restore({300, 0, {1, 2}, {{9, 2, "overlay.mp4", 0, 30, 0}}}));
  assert(restored.clips(2).size() == 1);
  const auto component = ComponentIr::parse({{"version", "1"}, {"root", QJsonObject{{"id", "component"}, {"type", "container"}}}});
  assert(component);
  assert(timeline.insertClip({4, track, {}, 0, 20, 200, TimelineClipKind::Component, *component}));
  assert(timeline.clip(4)->kind == TimelineClipKind::Component);
  assert(commands.moveClip(4, 220));
  assert(timeline.clip(4)->timelineStart == 220);
  return 0;
}
