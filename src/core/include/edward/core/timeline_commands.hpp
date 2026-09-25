#pragma once

#include "edward/core/timeline.hpp"

#include <functional>
#include <vector>

namespace edward::core {

class TimelineCommands {
 public:
  explicit TimelineCommands(Timeline& timeline);

  bool splitClipAtPlayhead(ClipId id);
  bool trimClip(ClipId id, Frame in, Frame out);
  bool deleteClip(ClipId id);
  bool rippleDelete(TrackId trackId, ClipId id);
  bool moveClip(ClipId id, Frame destination);
  bool setNativeRuntimeProps(ClipId id, QJsonObject props);
  bool setTransitionDuration(ClipId leftClipId, ClipId rightClipId, Frame duration);
  bool removeTransition(ClipId leftClipId, ClipId rightClipId);
  bool setPlayhead(Frame frame);
  bool addMarker(Frame frame, MarkerScope scope = MarkerScope::Timeline, ClipId clipId = 0,
                 MarkerColor color = MarkerColor::Orange);
  bool removeMarker(MarkerId id);
  bool setMarkerColor(MarkerId id, MarkerColor color);
  bool undo();
  bool redo();

 private:
  bool mutate(const std::function<bool()>& action);
  ClipId nextClipId() const;

  Timeline& timeline_;
  std::vector<TimelineSnapshot> undo_;
  std::vector<TimelineSnapshot> redo_;
};

}  // namespace edward::core
