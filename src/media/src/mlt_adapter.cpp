#include "edward/media/mlt_adapter.hpp"

#include <framework/mlt.h>

#include <algorithm>
#include <mutex>

namespace edward::media {
namespace {

bool ensureMltRuntime() {
  static std::once_flag initialized;
  static bool ready = false;
  std::call_once(initialized, [] { ready = mlt_factory_init(nullptr) != nullptr; });
  return ready;
}

}  // namespace

std::optional<QImage> MltAdapter::renderFrame(const edward::core::TimelineSnapshot& snapshot,
                                              edward::core::Frame frame) const {
  if (!ensureMltRuntime() || frame < 0 || frame >= snapshot.durationFrames) return std::nullopt;
  const auto clip = std::ranges::find_if(snapshot.clips, [frame](const auto& candidate) {
    const auto duration = candidate.sourceOut - candidate.sourceIn;
    return frame >= candidate.timelineStart && frame < candidate.timelineStart + duration;
  });
  if (clip == snapshot.clips.end()) return std::nullopt;

  const auto profile = mlt_profile_init(nullptr);
  if (!profile) return std::nullopt;
  const auto resource = clip->source.string();
  const auto producer = mlt_factory_producer(profile, nullptr, resource.c_str());
  if (!producer) {
    mlt_profile_close(profile);
    return std::nullopt;
  }

  mlt_profile_from_producer(profile, producer);
  const auto sourceFrame = clip->sourceIn + frame - clip->timelineStart;
  mlt_producer_seek(producer, sourceFrame);
  mlt_frame nativeFrame = nullptr;
  const auto frameError = mlt_service_get_frame(MLT_PRODUCER_SERVICE(producer), &nativeFrame, 0);
  if (frameError != 0 || !nativeFrame) {
    mlt_producer_close(producer);
    mlt_profile_close(profile);
    return std::nullopt;
  }

  mlt_image_format format = mlt_image_rgba;
  uint8_t* pixels = nullptr;
  int width = 0;
  int height = 0;
  const auto imageError = mlt_frame_get_image(nativeFrame, &pixels, &format, &width, &height, 0);
  std::optional<QImage> result;
  if (imageError == 0 && pixels && width > 0 && height > 0) {
    result = QImage(pixels, width, height, QImage::Format_RGBA8888).copy();
  }
  mlt_frame_close(nativeFrame);
  mlt_producer_close(producer);
  mlt_profile_close(profile);
  return result;
}

}  // namespace edward::media
