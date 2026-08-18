#include <edward/core/timeline.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <cassert>

int main(int argc, char** argv) {
  assert(argc == 2);
  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, argv[1], 0, 25, 0}));

  const edward::media::MltAdapter adapter;
  const edward::media::RenderGraph graph(adapter);
  const auto scene = graph.build(timeline.snapshot(), {0});
  assert(scene.has_value());
  assert(scene->frame.width() == 16);
  assert(!graph.build(timeline.snapshot(), {99}).has_value());
  return 0;
}
