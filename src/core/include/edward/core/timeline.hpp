#pragma once

#include "edward/core/media_project.hpp"
#include "edward/core/native_runtime_component.hpp"
#include "edward/core/transitions.hpp"

#include <optional>
#include <vector>

namespace edward::core {

enum class TimelineClipKind { Media, Component };

struct TimelineClip {
  ClipId id = 0;
  TrackId trackId = 0;
  std::filesystem::path source;
  Frame sourceIn = 0;
  Frame sourceOut = 0;
  Frame timelineStart = 0;
  TimelineClipKind kind = TimelineClipKind::Media;
  std::optional<NativeRuntimeComponent> nativeRuntime;
};

struct TimelineSnapshot {
  Frame durationFrames = 0;
  Frame playheadFrame = 0;
  std::vector<TrackId> videoTracks;
  std::vector<TimelineClip> clips;
  std::vector<Transition> transitions;
};

class Timeline {
 public:
  explicit Timeline(Frame durationFrames);

  TrackId addVideoTrack();
  bool removeEmptyVideoTrack(TrackId id);
  bool insertClip(TimelineClip clip);
  bool replaceClip(ClipId id, TimelineClip replacement);
  bool removeClip(ClipId id);
  bool setPlayhead(Frame frame);
  std::optional<Transition> addTransition(TransitionType type, ClipId leftClipId,
                                          ClipId rightClipId, Frame requestedDuration);
  std::optional<Transition> setTransitionDuration(ClipId leftClipId, ClipId rightClipId,
                                                   Frame requestedDuration);
  bool removeTransition(ClipId leftClipId, ClipId rightClipId);
  std::optional<TimelineClip> clip(ClipId id) const;
  std::vector<TimelineClip> clips(TrackId trackId) const;
  TimelineSnapshot snapshot() const;
  bool restore(const TimelineSnapshot& snapshot);

 private:
  bool isKnownTrack(TrackId id) const;
  bool isValid(const TimelineClip& clip) const;
  bool overlaps(const TimelineClip& candidate, std::optional<ClipId> ignored) const;
  void discardInvalidTransitions();

  Frame durationFrames_ = 0;
  Frame playheadFrame_ = 0;
  TrackId nextTrackId_ = 1;
  std::vector<TrackId> videoTracks_;
  std::vector<TimelineClip> clips_;
  std::vector<Transition> transitions_;
};

}  // namespace edward::core
