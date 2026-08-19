#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace edward::media {

struct MediaInfo {
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::int32_t fpsNumerator = 0;
  std::int32_t fpsDenominator = 1;
  std::int64_t durationFrames = 0;
  bool hasAudio = false;
};

class MediaProbe {
 public:
  static std::optional<MediaInfo> probe(const std::filesystem::path &path);
};

}  // namespace edward::media
