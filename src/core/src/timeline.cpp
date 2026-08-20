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
  return true;
}

bool Timeline::removeClip(ClipId id) {
  const auto previousSize = clips_.size();
  std::erase_if(clips_, [id](const auto& clip) { return clip.id == id; });
  return clips_.size() != previousSize;
}

bool Timeline::setPlayhead(Frame frame) {
  if (frame < 0 || frame > durationFrames_) return false;
  playheadFrame_ = frame;
  return true;
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

TimelineSnapshot Timeline::snapshot() const { return {durationFrames_, playheadFrame_, videoTracks_, clips_}; }

bool Timeline::restore(const TimelineSnapshot& snapshot) {
  if (snapshot.durationFrames != durationFrames_ || snapshot.playheadFrame < 0 || snapshot.playheadFrame > durationFrames_) return false;
  if (snapshot.videoTracks.empty()) return false;
  for (const auto track : snapshot.videoTracks)
    if (track == 0 || std::ranges::count(snapshot.videoTracks, track) != 1) return false;
  for (const auto& clip : snapshot.clips) {
    const auto duration = clip.sourceOut - clip.sourceIn;
    if (clip.id == 0 || std::ranges::find(snapshot.videoTracks, clip.trackId) == snapshot.videoTracks.end() ||
        ((clip.kind == TimelineClipKind::Media && (clip.source.empty() || clip.component.has_value())) ||
         (clip.kind == TimelineClipKind::Component && (!clip.component || !clip.component->validate()))) ||
        clip.sourceIn < 0 || duration <= 0 || clip.timelineStart < 0 ||
        clip.timelineStart + duration > durationFrames_) return false;
    for (const auto& other : snapshot.clips) {
      if (&clip != &other && (clip.id == other.id || (clip.trackId == other.trackId &&
          clip.timelineStart < other.timelineStart + (other.sourceOut - other.sourceIn) &&
          other.timelineStart < clip.timelineStart + (clip.sourceOut - clip.sourceIn)))) return false;
    }
  }
  videoTracks_ = snapshot.videoTracks;
  clips_ = snapshot.clips;
  playheadFrame_ = snapshot.playheadFrame;
  nextTrackId_ = videoTracks_.empty() ? 1 : *std::ranges::max_element(videoTracks_) + 1;
  return true;
}

bool Timeline::isKnownTrack(TrackId id) const { return std::ranges::find(videoTracks_, id) != videoTracks_.end(); }
bool Timeline::isValid(const TimelineClip& clip) const {
  const auto duration = clip.sourceOut - clip.sourceIn;
  const bool hasValidContent = (clip.kind == TimelineClipKind::Media && !clip.source.empty() && !clip.component) ||
                               (clip.kind == TimelineClipKind::Component && clip.component && clip.component->validate());
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

}  // namespace edward::core
