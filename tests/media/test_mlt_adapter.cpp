#include <edward/core/timeline.hpp>
#include <edward/media/mlt_adapter.hpp>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 2);
  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, argv[1], 0, 25, 0}));

  const edward::media::MltAdapter adapter;
  const auto first = adapter.renderFrame(timeline.snapshot(), 0);
  const auto later = adapter.renderFrame(timeline.snapshot(), 12);
  assert(first.has_value());
  assert(later.has_value());
  assert(first->width() == 16 && first->height() == 16);
  assert(later->size() == first->size());
  assert(!adapter.renderFrame(timeline.snapshot(), 25).has_value());
  return 0;
}
