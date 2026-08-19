#pragma once

#include "edward/core/timeline_commands.hpp"
#include "edward/media/media_probe.hpp"

#include <QString>

namespace edward::desktop {

class TimelineController {
 public:
  TimelineController(edward::core::Timeline& timeline, edward::core::TrackId trackId);

  bool dropMediaAtPlayhead(const QString& path);
  bool splitSelectedAtPlayhead();
  bool deleteSelected();
  bool rippleDeleteSelected();
  bool moveSelectedTo(edward::core::Frame destination);
  bool trimSelectedLeftToPlayhead();
  bool trimSelectedRightToPlayhead();
  bool undo();
  bool redo();
  bool selectClip(edward::core::ClipId id);
  bool setPlayhead(edward::core::Frame frame);
  [[nodiscard]] edward::core::Frame playheadFrame() const;
  [[nodiscard]] edward::core::ClipId selectedClip() const;

 private:
  edward::core::ClipId nextClipId() const;

  edward::core::Timeline& timeline_;
  edward::core::TrackId trackId_;
  edward::core::TimelineCommands commands_;
  edward::core::ClipId selectedClip_ = 0;
};

}  // namespace edward::desktop
