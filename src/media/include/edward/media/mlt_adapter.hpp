#pragma once

#include "edward/core/timeline.hpp"

#include <QImage>

#include <filesystem>
#include <optional>

namespace edward::media {

class MltAdapter {
 public:
  std::optional<QImage> renderFrame(const edward::core::TimelineSnapshot& snapshot,
                                    edward::core::Frame frame) const;
  std::optional<QImage> renderSourceFrame(const std::filesystem::path& source,
                                          edward::core::Frame frame) const;
};

}  // namespace edward::media
