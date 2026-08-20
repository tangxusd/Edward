#include <edward/core/timeline.hpp>

#include <cassert>

int main() {
  edward::core::Timeline timeline(120);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, "/tmp/a.mp4", 0, 30, 0}));
  assert(timeline.insertClip({2, track, "/tmp/b.mp4", 0, 40, 30}));

  const auto dissolve = timeline.addTransition(edward::core::TransitionType::Dissolve, 1, 2, 90);
  assert(dissolve);
  assert(dissolve->durationFrames == 30);
  assert(dissolve->leftClipId == 1 && dissolve->rightClipId == 2);

  const auto snapshot = timeline.snapshot();
  assert(snapshot.transitions.size() == 1);
  assert(timeline.restore(snapshot));
  assert(timeline.snapshot().transitions.front().type == edward::core::TransitionType::Dissolve);

  const auto resized = timeline.setTransitionDuration(1, 2, 90);
  assert(resized);
  assert(resized->durationFrames == 30);
  assert(resized->startFrame == 0);

  const auto shortened = timeline.setTransitionDuration(1, 2, 12);
  assert(shortened);
  assert(shortened->durationFrames == 12);
  assert(shortened->startFrame == 18);
  assert(timeline.removeTransition(1, 2));
  assert(timeline.snapshot().transitions.empty());
  assert(!timeline.removeTransition(1, 2));

  assert(timeline.addTransition(edward::core::TransitionType::Dissolve, 1, 2, 12));

  assert(timeline.removeClip(1));
  assert(timeline.snapshot().transitions.empty());

  edward::core::Timeline editedTimeline(120);
  const auto editedTrack = editedTimeline.addVideoTrack();
  assert(editedTimeline.insertClip({11, editedTrack, "/tmp/c.mp4", 0, 30, 0}));
  assert(editedTimeline.insertClip({12, editedTrack, "/tmp/d.mp4", 0, 30, 30}));
  assert(editedTimeline.addTransition(edward::core::TransitionType::Dissolve, 11, 12, 10));
  auto moved = *editedTimeline.clip(12);
  moved.timelineStart = 60;
  assert(editedTimeline.replaceClip(12, moved));
  assert(editedTimeline.snapshot().transitions.empty());

  assert(!timeline.addTransition(edward::core::TransitionType::FlashBlack, 2, 1, 10));
  return 0;
}
