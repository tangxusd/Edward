#include <edward/core/timeline_commands.hpp>

#include <cassert>

int main() {
  using namespace edward::core;
  Timeline timeline(12'000);
  std::vector<TrackId> tracks;
  for (int index = 0; index < 5; ++index) tracks.push_back(timeline.addVideoTrack());

  ClipId clipId = 1;
  for (int trackIndex = 0; trackIndex < 5; ++trackIndex) {
    for (int clipIndex = 0; clipIndex < 20; ++clipIndex) {
      const auto start = static_cast<Frame>(clipIndex * 100);
      assert(timeline.insertClip({clipId++, tracks[trackIndex], "fixture.mp4", 0, 90, start}));
    }
  }
  assert(timeline.snapshot().clips.size() == 100);

  TimelineCommands commands(timeline);
  assert(commands.setPlayhead(45));
  assert(commands.splitClipAtPlayhead(1));
  const auto firstTrackClips = timeline.clips(tracks.front());
  const auto split = std::ranges::find_if(firstTrackClips, [](const auto& clip) {
    return clip.timelineStart == 45;
  });
  assert(split != firstTrackClips.end());
  assert(commands.trimClip(split->id, 45, 80));
  assert(commands.moveClip(split->id, 60));
  assert(commands.undo());
  assert(commands.redo());

  const auto snapshot = timeline.snapshot();
  Timeline restored(snapshot.durationFrames);
  assert(restored.restore(snapshot));
  assert(restored.snapshot().clips.size() == 101);
  assert(restored.clips(tracks.front()).size() == 21);
  return 0;
}
