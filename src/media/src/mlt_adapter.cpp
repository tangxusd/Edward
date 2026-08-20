#include "edward/media/mlt_adapter.hpp"

#include <framework/mlt.h>

#include <algorithm>
#include <mutex>

#include <QPainter>

namespace edward::media {
namespace {

bool ensureMltRuntime() {
  static std::once_flag initialized;
  static bool ready = false;
  std::call_once(initialized, [] { ready = mlt_factory_init(nullptr) != nullptr; });
  return ready;
}

std::mutex& renderMutex() {
  static std::mutex mutex;
  return mutex;
}

std::optional<QImage> readClipFrame(mlt_profile profile, const edward::core::TimelineClip& clip,
                                    edward::core::Frame frame) {
  const auto resource = clip.source.string();
  const auto producer = mlt_factory_producer(profile, nullptr, resource.c_str());
  if (!producer) return std::nullopt;
  const auto sourceFrame = clip.sourceIn + frame - clip.timelineStart;
  mlt_producer_seek(producer, sourceFrame);
  mlt_frame nativeFrame = nullptr;
  const auto frameError = mlt_service_get_frame(MLT_PRODUCER_SERVICE(producer), &nativeFrame, 0);
  if (frameError != 0 || !nativeFrame) {
    mlt_producer_close(producer);
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
  return result;
}

}  // namespace

std::optional<QImage> MltAdapter::renderFrame(const edward::core::TimelineSnapshot& snapshot,
                                              edward::core::Frame frame) const {
  const std::scoped_lock lock(renderMutex());
  if (frame < 0 || frame >= snapshot.durationFrames) return std::nullopt;
  std::vector<const edward::core::TimelineClip*> active;
  for (const auto track : snapshot.videoTracks) {
    const auto clip = std::ranges::find_if(snapshot.clips, [frame, track](const auto& candidate) {
      const auto duration = candidate.sourceOut - candidate.sourceIn;
      return candidate.kind == edward::core::TimelineClipKind::Media && candidate.trackId == track &&
             frame >= candidate.timelineStart && frame < candidate.timelineStart + duration;
    });
    if (clip != snapshot.clips.end()) active.push_back(&*clip);
  }
  const auto mediaReference = std::ranges::find_if(snapshot.clips, [](const auto& clip) {
    return clip.kind == edward::core::TimelineClipKind::Media;
  });
  const auto reference = active.empty() ? (mediaReference == snapshot.clips.end() ? nullptr : &*mediaReference) : active.front();
  if (!reference) {
    QImage componentCanvas(QSize(1920, 1080), QImage::Format_RGBA8888);
    componentCanvas.fill(Qt::black);
    return componentCanvas;
  }
  if (!ensureMltRuntime()) return std::nullopt;

  const auto profile = mlt_profile_init(nullptr);
  if (!profile) return std::nullopt;
  const auto resource = reference->source.string();
  const auto producer = mlt_factory_producer(profile, nullptr, resource.c_str());
  if (!producer) {
    mlt_profile_close(profile);
    return std::nullopt;
  }

  mlt_profile_from_producer(profile, producer);
  QImage result(profile->width, profile->height, QImage::Format_RGBA8888);
  result.fill(Qt::black);
  mlt_producer_close(producer);
  for (const auto* clip : active) {
    const auto image = readClipFrame(profile, *clip, frame);
    if (!image) continue;
    QPainter painter(&result);
    painter.drawImage(result.rect(), *image);
  }
  mlt_profile_close(profile);
  return result.isNull() ? std::nullopt : std::optional<QImage>(std::move(result));
}

std::optional<QImage> MltAdapter::renderSourceFrame(const std::filesystem::path& source,
                                                     edward::core::Frame frame) const {
  const std::scoped_lock lock(renderMutex());
  if (source.empty() || frame < 0 || !ensureMltRuntime()) return std::nullopt;
  const auto profile = mlt_profile_init(nullptr);
  if (!profile) return std::nullopt;
  const auto resource = source.string();
  const auto producer = mlt_factory_producer(profile, nullptr, resource.c_str());
  if (!producer) {
    mlt_profile_close(profile);
    return std::nullopt;
  }
  mlt_profile_from_producer(profile, producer);
  edward::core::TimelineClip clip{1, 1, source, frame, frame + 1, 0};
  const auto image = readClipFrame(profile, clip, 0);
  mlt_producer_close(producer);
  mlt_profile_close(profile);
  return image;
}

}  // namespace edward::media
