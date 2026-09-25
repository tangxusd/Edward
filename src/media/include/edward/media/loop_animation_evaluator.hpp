#pragma once

#include "edward/core/timeline.hpp"

namespace edward::media {

enum class LoopAnimationKind { MoveRight, MoveLeft, MoveUp, MoveDown, ScaleUp, ScaleDown };

struct LoopAnimation {
  LoopAnimationKind kind = LoopAnimationKind::MoveRight;
  edward::core::Frame startFrame = 0;
  edward::core::Frame endFrame = 1;
  double distance = 0.0;
  bool pingPong = false;
};

struct AnimationValue {
  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;
};

class LoopAnimationEvaluator final {
 public:
  static AnimationValue evaluate(const LoopAnimation& animation, edward::core::Frame frame, int fps);
};

}  // namespace edward::media
