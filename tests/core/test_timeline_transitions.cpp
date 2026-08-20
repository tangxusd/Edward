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

  assert(!timeline.addTransition(edward::core::TransitionType::FlashBlack, 2, 1, 10));
  return 0;
}
