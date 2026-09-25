#include "edward/media/loop_animation_evaluator.hpp"

#include <algorithm>
#include <cmath>

namespace edward::media {

AnimationValue LoopAnimationEvaluator::evaluate(const LoopAnimation& animation, edward::core::Frame frame, int fps) {
  AnimationValue value;
  const auto duration = std::max<edward::core::Frame>(1, animation.endFrame - animation.startFrame);
  auto local = std::clamp(frame - animation.startFrame, edward::core::Frame(0), duration);
  auto phase = static_cast<double>(local) / static_cast<double>(duration);
  if (animation.pingPong && ((frame - animation.startFrame) / duration) % 2 != 0) phase = 1.0 - phase;
  if (fps > 0) phase = std::round(phase * static_cast<double>(duration)) / static_cast<double>(duration);
  switch (animation.kind) {
    case LoopAnimationKind::MoveRight: value.x = animation.distance * phase; break;
    case LoopAnimationKind::MoveLeft: value.x = -animation.distance * phase; break;
    case LoopAnimationKind::MoveUp: value.y = animation.distance * phase; break;
    case LoopAnimationKind::MoveDown: value.y = -animation.distance * phase; break;
    case LoopAnimationKind::ScaleUp: value.scale = 1.0 + animation.distance * phase; break;
    case LoopAnimationKind::ScaleDown: value.scale = 1.0 - animation.distance * phase; break;
  }
  return value;
}

}  // namespace edward::media
