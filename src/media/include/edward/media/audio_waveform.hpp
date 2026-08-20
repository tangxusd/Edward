#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace edward::media {

struct AudioWaveform {
  std::vector<float> peaks;
  int sampleRate = 0;
};

class AudioWaveformExtractor {
 public:
  static std::optional<AudioWaveform> extract(const std::filesystem::path& path, std::size_t bucketCount,
                                              double startSeconds = 0.0,
                                              std::optional<double> endSeconds = std::nullopt);
};

}  // namespace edward::media
