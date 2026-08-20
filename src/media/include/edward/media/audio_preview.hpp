#pragma once

#include "edward/core/timeline.hpp"

#include <QStringList>
#include <QProcess>

#include <optional>

namespace edward::media {

class AudioPreview final {
 public:
  AudioPreview();
  ~AudioPreview();
  AudioPreview(const AudioPreview&) = delete;
  AudioPreview& operator=(const AudioPreview&) = delete;
  static std::optional<QStringList> argumentsFor(const edward::core::TimelineSnapshot& snapshot,
                                                 edward::core::Frame startFrame, int fpsNumerator,
                                                 int fpsDenominator);
  bool start(const edward::core::TimelineSnapshot& snapshot, edward::core::Frame startFrame,
             int fpsNumerator, int fpsDenominator);
  void stop();
  void pump();
  [[nodiscard]] bool active() const;

 private:
  QProcess process_;
  void* stream_ = nullptr;
  bool audioInitialized_ = false;
};

}  // namespace edward::media
