#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace edward::core {

using Frame = std::int64_t;
using TrackId = std::int64_t;
using ClipId = std::int64_t;

struct MediaClip {
  ClipId id = 0;
  std::filesystem::path source;
  Frame sourceIn = 0;
  Frame sourceOut = 0;
  Frame timelineStart = 0;
};

class MediaProject {
 public:
  explicit MediaProject(Frame durationFrames = 0);

  TrackId addVideoTrack();
  bool insertClip(TrackId trackId, MediaClip clip);
  std::size_t videoTrackCount() const;
  std::size_t clipCount(TrackId trackId) const;
  std::optional<MediaClip> clip(TrackId trackId, ClipId clipId) const;

 private:
  struct VideoTrack {
    TrackId id = 0;
    std::vector<MediaClip> clips;
  };

  Frame durationFrames_ = 0;
  TrackId nextTrackId_ = 1;
  std::vector<VideoTrack> videoTracks_;
};

}  // namespace edward::core
