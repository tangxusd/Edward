#include <edward/core/timeline.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <cassert>

int main(int argc, char** argv) {
  assert(argc == 2);
  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, argv[1], 0, 25, 0}));

  const edward::media::MltAdapter adapter;
  const QJsonObject root{{"id", "root"}, {"type", "container"}, {"children", QJsonArray{
      QJsonObject{{"id", "overlay"}, {"type", "shape"},
                   {"transform", QJsonObject{{"width", 4}, {"height", 4}}},
                   {"properties", QJsonObject{{"fill", "#ff0000"}}}},
  }}};
  const auto overlay = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(overlay);
  const edward::media::RenderGraph graph(adapter, *overlay);
  const auto scene = graph.build(timeline.snapshot(), {0});
  assert(scene.has_value());
  assert(scene->frame.width() == 16);
  assert(scene->frame.pixelColor(0, 0).red() > scene->frame.pixelColor(0, 0).green());
  assert(!graph.build(timeline.snapshot(), {99}).has_value());
  return 0;
}
