#include "edward/core/timeline_commands.hpp"

#include <algorithm>

namespace edward::core {

TimelineCommands::TimelineCommands(Timeline& timeline) : timeline_(timeline) {}

bool TimelineCommands::splitClipAtPlayhead(ClipId id) {
  const auto original = timeline_.clip(id);
  if (!original) return false;
  const auto playhead = timeline_.snapshot().playheadFrame;
  const auto end = original->timelineStart + original->sourceOut - original->sourceIn;
  if (playhead <= original->timelineStart || playhead >= end) return false;
  return mutate([&] {
    auto first = *original;
    first.sourceOut = first.sourceIn + playhead - first.timelineStart;
    auto second = *original;
    second.id = nextClipId();
    second.sourceIn = first.sourceOut;
    second.timelineStart = playhead;
    return timeline_.replaceClip(id, first) && timeline_.insertClip(second);
  });
}

bool TimelineCommands::trimClip(ClipId id, Frame in, Frame out) {
  const auto original = timeline_.clip(id);
  if (!original) return false;
  const auto end = original->timelineStart + original->sourceOut - original->sourceIn;
  if (in < original->timelineStart || out > end || in >= out) return false;
  return mutate([&] {
    auto trimmed = *original;
    trimmed.sourceIn += in - original->timelineStart;
    trimmed.sourceOut = trimmed.sourceIn + out - in;
    trimmed.timelineStart = in;
    return timeline_.replaceClip(id, trimmed);
  });
}

bool TimelineCommands::deleteClip(ClipId id) { return mutate([&] { return timeline_.removeClip(id); }); }

bool TimelineCommands::rippleDelete(TrackId trackId, ClipId id) {
  const auto original = timeline_.clip(id);
  if (!original || original->trackId != trackId) return false;
  const auto duration = original->sourceOut - original->sourceIn;
  const auto end = original->timelineStart + duration;
  return mutate([&] {
    if (!timeline_.removeClip(id)) return false;
    const auto following = timeline_.clips(trackId);
    for (const auto& clip : following) {
      if (clip.timelineStart < end) continue;
      auto moved = clip;
      moved.timelineStart -= duration;
      if (!timeline_.replaceClip(clip.id, moved)) return false;
    }
    return true;
  });
}

bool TimelineCommands::moveClip(ClipId id, Frame destination) {
  const auto original = timeline_.clip(id);
  if (!original || destination < 0) return false;
  return mutate([&] { auto moved = *original; moved.timelineStart = destination; return timeline_.replaceClip(id, moved); });
}

bool TimelineCommands::setPlayhead(Frame frame) { return mutate([&] { return timeline_.setPlayhead(frame); }); }

bool TimelineCommands::undo() {
  if (undo_.empty()) return false;
  const auto current = timeline_.snapshot();
  const auto previous = undo_.back();
  undo_.pop_back();
  if (!timeline_.restore(previous)) return false;
  redo_.push_back(current);
  return true;
}

bool TimelineCommands::redo() {
  if (redo_.empty()) return false;
  const auto current = timeline_.snapshot();
  const auto next = redo_.back();
  redo_.pop_back();
  if (!timeline_.restore(next)) return false;
  undo_.push_back(current);
  return true;
}

bool TimelineCommands::mutate(const std::function<bool()>& action) {
  const auto before = timeline_.snapshot();
  if (!action()) {
    timeline_.restore(before);
    return false;
  }
  undo_.push_back(before);
  redo_.clear();
  return true;
}

ClipId TimelineCommands::nextClipId() const {
  ClipId maximum = 0;
  for (const auto trackId : timeline_.snapshot().videoTracks)
    for (const auto& clip : timeline_.clips(trackId)) maximum = std::max(maximum, clip.id);
  return maximum + 1;
}

}  // namespace edward::core
