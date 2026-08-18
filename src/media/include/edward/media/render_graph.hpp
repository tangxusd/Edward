#pragma once

#include "edward/core/timeline.hpp"
#include "edward/media/mlt_adapter.hpp"

#include <QImage>

#include <optional>

namespace edward::media {

struct RenderRequest {
  edward::core::Frame frame = 0;
};

struct RenderScene {
  QImage frame;
};

class RenderGraph {
 public:
  explicit RenderGraph(const MltAdapter& adapter);
  std::optional<RenderScene> build(const edward::core::TimelineSnapshot& snapshot,
                                   const RenderRequest& request) const;

 private:
  const MltAdapter& adapter_;
};

}  // namespace edward::media
