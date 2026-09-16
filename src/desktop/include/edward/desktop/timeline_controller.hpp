#pragma once

#include "edward/core/timeline_commands.hpp"
#include "edward/media/media_probe.hpp"

#include <QString>

namespace edward::desktop {

class TimelineController {
 public:
  TimelineController(edward::core::Timeline& timeline, edward::core::TrackId trackId);

  bool dropMediaAtPlayhead(const QString& path);
  bool dropComponentAtPlayhead(const edward::core::ComponentIr& component, edward::core::Frame duration);
  bool dropNativeRuntimeAtPlayhead(const edward::core::NativeRuntimeComponent& component,
                                   edward::core::Frame duration);
  bool splitSelectedAtPlayhead();
  bool deleteSelected();
  bool rippleDeleteSelected();
  bool moveSelectedTo(edward::core::Frame destination);
  bool trimSelectedLeftToPlayhead();
  bool trimSelectedRightToPlayhead();
  bool setTransitionDuration(edward::core::ClipId leftClipId, edward::core::ClipId rightClipId,
                             edward::core::Frame duration);
  bool removeTransition(edward::core::ClipId leftClipId, edward::core::ClipId rightClipId);
  bool undo();
  bool redo();
  bool selectClip(edward::core::ClipId id);
  bool setPlayhead(edward::core::Frame frame);
  bool setTargetTrack(edward::core::TrackId trackId);
  bool advancePlayhead();
  [[nodiscard]] edward::core::Frame playheadFrame() const;
  [[nodiscard]] edward::core::ClipId selectedClip() const;
  [[nodiscard]] edward::core::TrackId targetTrack() const;

 private:
  edward::core::ClipId nextClipId() const;

  edward::core::Timeline& timeline_;
  edward::core::TrackId trackId_;
  edward::core::TimelineCommands commands_;
  edward::core::ClipId selectedClip_ = 0;
};

}  // namespace edward::desktop
