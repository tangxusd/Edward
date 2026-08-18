#include <edward/core/media_project.hpp>

#include <algorithm>

namespace edward::core {

MediaProject::MediaProject(Frame durationFrames) : durationFrames_(durationFrames) {}

TrackId MediaProject::addVideoTrack() {
  const TrackId id = nextTrackId_++;
  videoTracks_.push_back(VideoTrack{id, {}});
  return id;
}

bool MediaProject::insertClip(TrackId trackId, MediaClip clip) {
  if (clip.id <= 0 || clip.source.empty() || clip.sourceIn < 0 ||
      clip.sourceOut <= clip.sourceIn || clip.timelineStart < 0 ||
      (durationFrames_ > 0 && clip.timelineStart + (clip.sourceOut - clip.sourceIn) > durationFrames_)) {
    return false;
  }
  const auto track = std::find_if(videoTracks_.begin(), videoTracks_.end(),
                                  [trackId](const VideoTrack &value) { return value.id == trackId; });
  if (track == videoTracks_.end()) return false;
  if (std::any_of(track->clips.begin(), track->clips.end(),
                  [id = clip.id](const MediaClip &value) { return value.id == id; })) {
    return false;
  }
  track->clips.push_back(std::move(clip));
  return true;
}

std::size_t MediaProject::videoTrackCount() const { return videoTracks_.size(); }

std::size_t MediaProject::clipCount(TrackId trackId) const {
  const auto track = std::find_if(videoTracks_.begin(), videoTracks_.end(),
                                  [trackId](const VideoTrack &value) { return value.id == trackId; });
  return track == videoTracks_.end() ? 0 : track->clips.size();
}

std::optional<MediaClip> MediaProject::clip(TrackId trackId, ClipId clipId) const {
  const auto track = std::find_if(videoTracks_.begin(), videoTracks_.end(),
                                  [trackId](const VideoTrack &value) { return value.id == trackId; });
  if (track == videoTracks_.end()) return std::nullopt;
  const auto item = std::find_if(track->clips.begin(), track->clips.end(),
                                 [clipId](const MediaClip &value) { return value.id == clipId; });
  return item == track->clips.end() ? std::nullopt : std::optional<MediaClip>(*item);
}

}  // namespace edward::core
