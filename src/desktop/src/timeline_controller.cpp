#include "edward/desktop/timeline_controller.hpp"

#include <algorithm>

namespace edward::desktop {

TimelineController::TimelineController(edward::core::Timeline& timeline,
                                       edward::core::TrackId trackId)
    : timeline_(timeline), trackId_(trackId), commands_(timeline) {}

bool TimelineController::dropMediaAtPlayhead(const QString& path) {
  if (path.isEmpty()) return false;
  const auto info = edward::media::MediaProbe::probe(path.toStdString());
  if (!info || info->durationFrames <= 0) return false;
  const auto snapshot = timeline_.snapshot();
  const auto start = snapshot.playheadFrame;
  if (start + info->durationFrames > snapshot.durationFrames) return false;
  const auto id = nextClipId();
  const edward::core::TimelineClip clip{id, trackId_, path.toStdString(), 0, info->durationFrames, start};
  if (!timeline_.insertClip(clip)) {
    const auto alternateTrack = timeline_.addVideoTrack();
    if (!timeline_.insertClip({id, alternateTrack, path.toStdString(), 0, info->durationFrames, start})) return false;
    trackId_ = alternateTrack;
  }
  selectedClip_ = id;
  return true;
}

bool TimelineController::splitSelectedAtPlayhead() {
  return selectedClip_ != 0 && commands_.splitClipAtPlayhead(selectedClip_);
}

bool TimelineController::deleteSelected() {
  if (selectedClip_ == 0 || !commands_.deleteClip(selectedClip_)) return false;
  selectedClip_ = 0;
  return true;
}

bool TimelineController::rippleDeleteSelected() {
  const auto selected = timeline_.clip(selectedClip_);
  if (!selected || !commands_.rippleDelete(selected->trackId, selectedClip_)) return false;
  selectedClip_ = 0;
  return true;
}

bool TimelineController::moveSelectedTo(edward::core::Frame destination) {
  return selectedClip_ != 0 && commands_.moveClip(selectedClip_, destination);
}

bool TimelineController::selectClip(edward::core::ClipId id) {
  if (!timeline_.clip(id)) return false;
  selectedClip_ = id;
  return true;
}

bool TimelineController::setPlayhead(edward::core::Frame frame) { return commands_.setPlayhead(frame); }
edward::core::Frame TimelineController::playheadFrame() const { return timeline_.snapshot().playheadFrame; }
edward::core::ClipId TimelineController::selectedClip() const { return selectedClip_; }

edward::core::ClipId TimelineController::nextClipId() const {
  edward::core::ClipId maximum = 0;
  for (const auto track : timeline_.snapshot().videoTracks)
    for (const auto& clip : timeline_.clips(track)) maximum = std::max(maximum, clip.id);
  return maximum + 1;
}

}  // namespace edward::desktop
