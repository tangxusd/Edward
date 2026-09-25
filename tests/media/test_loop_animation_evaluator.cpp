#include "edward/media/loop_animation_evaluator.hpp"

#include <cassert>
#include <cmath>

int main() {
  using namespace edward::media;
  const LoopAnimation right{LoopAnimationKind::MoveRight, 0, 30, 100, false};
  assert(std::abs(LoopAnimationEvaluator::evaluate(right, 15, 30).x - 50.0) < 0.001);
  const LoopAnimation down{LoopAnimationKind::MoveDown, 0, 30, 20, false};
  assert(LoopAnimationEvaluator::evaluate(down, 30, 30).y == -20.0);
  const LoopAnimation scale{LoopAnimationKind::ScaleUp, 0, 30, 0.5, false};
  assert(std::abs(LoopAnimationEvaluator::evaluate(scale, 30, 30).scale - 1.5) < 0.001);
  return 0;
}
