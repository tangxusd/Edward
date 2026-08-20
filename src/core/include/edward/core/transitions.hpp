#pragma once

#include "edward/core/media_project.hpp"

namespace edward::core {

enum class TransitionType { FlashBlack, FlashWhite, Dissolve };

struct Transition final {
  TransitionType type = TransitionType::Dissolve;
  ClipId leftClipId = 0;
  ClipId rightClipId = 0;
  Frame startFrame = 0;
  Frame durationFrames = 0;
};

}  // namespace edward::core
