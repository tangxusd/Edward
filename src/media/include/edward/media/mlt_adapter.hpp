#pragma once

#include "edward/core/timeline.hpp"

#include <QImage>

#include <optional>

namespace edward::media {

class MltAdapter {
 public:
  std::optional<QImage> renderFrame(const edward::core::TimelineSnapshot& snapshot,
                                    edward::core::Frame frame) const;
};

}  // namespace edward::media
