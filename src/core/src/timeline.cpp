#include "edward/core/timeline.hpp"

#include <algorithm>

namespace edward::core {

Timeline::Timeline(Frame durationFrames) : durationFrames_(durationFrames) {}

TrackId Timeline::addVideoTrack() {
  const auto id = nextTrackId_++;
  videoTracks_.push_back(id);
  return id;
}

bool Timeline::removeEmptyVideoTrack(TrackId id) {
  if (videoTracks_.size() <= 1 || !isKnownTrack(id) || !clips(id).empty()) return false;
  std::erase(videoTracks_, id);
  return true;
}

bool Timeline::insertClip(TimelineClip clip) {
  if (!isValid(clip) || overlaps(clip, std::nullopt)) return false;
  if (std::any_of(clips_.begin(), clips_.end(), [clip](const auto& existing) { return existing.id == clip.id; })) return false;
  clips_.push_back(std::move(clip));
  return true;
}

bool Timeline::replaceClip(ClipId id, TimelineClip replacement) {
  const auto it = std::find_if(clips_.begin(), clips_.end(), [id](const auto& clip) { return clip.id == id; });
  if (it == clips_.end() || replacement.id != id || !isValid(replacement) || overlaps(replacement, id)) return false;
  *it = std::move(replacement);
  discardInvalidTransitions();
  return true;
}

bool Timeline::removeClip(ClipId id) {
  const auto previousSize = clips_.size();
  std::erase_if(clips_, [id](const auto& clip) { return clip.id == id; });
  discardInvalidTransitions();
  return clips_.size() != previousSize;
}

bool Timeline::setPlayhead(Frame frame) {
  if (frame < 0 || frame > durationFrames_) return false;
  playheadFrame_ = frame;
  return true;
}

std::optional<Transition> Timeline::addTransition(TransitionType type, ClipId leftClipId,
                                                  ClipId rightClipId, Frame requestedDuration) {
  if (requestedDuration <= 0 || leftClipId == rightClipId) return std::nullopt;
  const auto left = clip(leftClipId);
  const auto right = clip(rightClipId);
  if (!left || !right || left->trackId != right->trackId) return std::nullopt;
  const auto leftDuration = left->sourceOut - left->sourceIn;
  const auto rightDuration = right->sourceOut - right->sourceIn;
  const auto leftEnd = left->timelineStart + leftDuration;
  if (leftEnd != right->timelineStart) return std::nullopt;
  const auto duration = std::min({requestedDuration, leftDuration, rightDuration});
  if (duration <= 0) return std::nullopt;
  const auto existing = std::ranges::find_if(transitions_, [&](const auto& transition) {
    return transition.leftClipId == leftClipId && transition.rightClipId == rightClipId;
  });
  if (existing != transitions_.end()) return std::nullopt;
  Transition transition{type, leftClipId, rightClipId, leftEnd - duration, duration};
  transitions_.push_back(transition);
  return transition;
}

std::optional<Transition> Timeline::setTransitionDuration(ClipId leftClipId, ClipId rightClipId,
                                                           Frame requestedDuration) {
  if (requestedDuration <= 0) return std::nullopt;
  auto transition = std::ranges::find_if(transitions_, [&](const auto& candidate) {
    return candidate.leftClipId == leftClipId && candidate.rightClipId == rightClipId;
  });
  const auto left = clip(leftClipId);
  const auto right = clip(rightClipId);
  if (transition == transitions_.end() || !left || !right || left->trackId != right->trackId) return std::nullopt;
  const auto leftDuration = left->sourceOut - left->sourceIn;
  const auto rightDuration = right->sourceOut - right->sourceIn;
  const auto leftEnd = left->timelineStart + leftDuration;
  if (leftEnd != right->timelineStart) return std::nullopt;
  const auto duration = std::min({requestedDuration, leftDuration, rightDuration});
  if (duration <= 0) return std::nullopt;
  transition->durationFrames = duration;
  transition->startFrame = leftEnd - duration;
  return *transition;
}

bool Timeline::removeTransition(ClipId leftClipId, ClipId rightClipId) {
  const auto previousSize = transitions_.size();
  std::erase_if(transitions_, [&](const auto& transition) {
    return transition.leftClipId == leftClipId && transition.rightClipId == rightClipId;
  });
  return transitions_.size() != previousSize;
}

std::optional<TimelineClip> Timeline::clip(ClipId id) const {
  const auto it = std::find_if(clips_.begin(), clips_.end(), [id](const auto& clip) { return clip.id == id; });
  return it == clips_.end() ? std::nullopt : std::optional<TimelineClip>(*it);
}

std::vector<TimelineClip> Timeline::clips(TrackId trackId) const {
  std::vector<TimelineClip> result;
  for (const auto& clip : clips_) if (clip.trackId == trackId) result.push_back(clip);
  std::ranges::sort(result, {}, &TimelineClip::timelineStart);
  return result;
}

TimelineSnapshot Timeline::snapshot() const {
  return {durationFrames_, playheadFrame_, videoTracks_, clips_, transitions_};
}

bool Timeline::restore(const TimelineSnapshot& snapshot) {
  if (snapshot.durationFrames != durationFrames_ || snapshot.playheadFrame < 0 || snapshot.playheadFrame > durationFrames_) return false;
  if (snapshot.videoTracks.empty()) return false;
  for (const auto track : snapshot.videoTracks)
    if (track == 0 || std::ranges::count(snapshot.videoTracks, track) != 1) return false;
  for (const auto& clip : snapshot.clips) {
    const auto duration = clip.sourceOut - clip.sourceIn;
    if (clip.id == 0 || std::ranges::find(snapshot.videoTracks, clip.trackId) == snapshot.videoTracks.end() ||
        ((clip.kind == TimelineClipKind::Media && (clip.source.empty() || clip.component.has_value() || clip.nativeRuntime.has_value())) ||
         (clip.kind == TimelineClipKind::Component &&
          ((clip.component.has_value() == clip.nativeRuntime.has_value()) ||
           (clip.component && !clip.component->validate()) || (clip.nativeRuntime && !clip.nativeRuntime->valid())))) ||
        clip.sourceIn < 0 || duration <= 0 || clip.timelineStart < 0 ||
        clip.timelineStart + duration > durationFrames_) return false;
    for (const auto& other : snapshot.clips) {
      if (&clip != &other && (clip.id == other.id || (clip.trackId == other.trackId &&
          clip.timelineStart < other.timelineStart + (other.sourceOut - other.sourceIn) &&
          other.timelineStart < clip.timelineStart + (clip.sourceOut - clip.sourceIn)))) return false;
    }
  }
  for (const auto& transition : snapshot.transitions) {
    const auto left = std::ranges::find_if(snapshot.clips, [&](const auto& clip) { return clip.id == transition.leftClipId; });
    const auto right = std::ranges::find_if(snapshot.clips, [&](const auto& clip) { return clip.id == transition.rightClipId; });
    if (left == snapshot.clips.end() || right == snapshot.clips.end() || transition.durationFrames <= 0 ||
        left->trackId != right->trackId || left->timelineStart + (left->sourceOut - left->sourceIn) != right->timelineStart ||
        transition.durationFrames > std::min(left->sourceOut - left->sourceIn, right->sourceOut - right->sourceIn) ||
        transition.startFrame != right->timelineStart - transition.durationFrames)
      return false;
  }
  videoTracks_ = snapshot.videoTracks;
  clips_ = snapshot.clips;
  transitions_ = snapshot.transitions;
  playheadFrame_ = snapshot.playheadFrame;
  nextTrackId_ = videoTracks_.empty() ? 1 : *std::ranges::max_element(videoTracks_) + 1;
  return true;
}

bool Timeline::isKnownTrack(TrackId id) const { return std::ranges::find(videoTracks_, id) != videoTracks_.end(); }
bool Timeline::isValid(const TimelineClip& clip) const {
  const auto duration = clip.sourceOut - clip.sourceIn;
  const bool hasValidContent =
      (clip.kind == TimelineClipKind::Media && !clip.source.empty() && !clip.component && !clip.nativeRuntime) ||
      (clip.kind == TimelineClipKind::Component &&
       ((clip.component && !clip.nativeRuntime && clip.component->validate()) ||
        (clip.nativeRuntime && !clip.component && clip.nativeRuntime->valid())));
  return clip.id > 0 && isKnownTrack(clip.trackId) && hasValidContent && clip.sourceIn >= 0 &&
    duration > 0 && clip.timelineStart >= 0 && clip.timelineStart + duration <= durationFrames_;
}
bool Timeline::overlaps(const TimelineClip& candidate, std::optional<ClipId> ignored) const {
  const auto candidateEnd = candidate.timelineStart + candidate.sourceOut - candidate.sourceIn;
  return std::ranges::any_of(clips_, [&](const auto& existing) {
    if (ignored && existing.id == *ignored) return false;
    const auto existingEnd = existing.timelineStart + existing.sourceOut - existing.sourceIn;
    return existing.trackId == candidate.trackId && candidate.timelineStart < existingEnd && existing.timelineStart < candidateEnd;
  });
}

void Timeline::discardInvalidTransitions() {
  std::erase_if(transitions_, [&](const auto& transition) {
    const auto left = std::ranges::find_if(clips_, [&](const auto& clip) { return clip.id == transition.leftClipId; });
    const auto right = std::ranges::find_if(clips_, [&](const auto& clip) { return clip.id == transition.rightClipId; });
    if (left == clips_.end() || right == clips_.end() || transition.durationFrames <= 0 ||
        left->trackId != right->trackId) return true;
    const auto leftDuration = left->sourceOut - left->sourceIn;
    const auto rightDuration = right->sourceOut - right->sourceIn;
    return left->timelineStart + leftDuration != right->timelineStart ||
           transition.durationFrames > std::min(leftDuration, rightDuration) ||
           transition.startFrame != right->timelineStart - transition.durationFrames;
  });
}

}  // namespace edward::core
