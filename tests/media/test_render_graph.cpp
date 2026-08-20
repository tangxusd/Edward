#include <edward/core/timeline.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
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
                   {"properties", QJsonObject{{"fill", "#ff0000"}}},
                   {"keyframes", QJsonObject{{"x", QJsonArray{
                       QJsonObject{{"frame", 0}, {"value", 0}},
                       QJsonObject{{"frame", 10}, {"value", 8}}
                   }}}}},
  }}};
  const auto overlay = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(overlay);
  edward::media::RenderGraph graph(adapter, *overlay);
  const auto scene = graph.build(timeline.snapshot(), {0});
  assert(scene.has_value());
  assert(scene->frame.width() == 16);
  assert(scene->frame.pixelColor(8, 8).red() > scene->frame.pixelColor(8, 8).green());
  const auto movedScene = graph.build(timeline.snapshot(), {10});
  assert(movedScene.has_value());
  assert(movedScene->frame.pixelColor(8, 8).red() < scene->frame.pixelColor(8, 8).red());
  assert(movedScene->frame.pixelColor(0, 8).red() > movedScene->frame.pixelColor(0, 8).green());
  const QJsonObject blueRoot{
      {"id", "blue-root"}, {"type", "container"},
      {"children", QJsonArray{
          QJsonObject{{"id", "blue"}, {"type", "shape"},
                       {"transform", QJsonObject{{"x", 4}, {"y", 0}, {"width", 4}, {"height", 4}}},
                       {"properties", QJsonObject{{"fill", "#0000ff"}}}}}}};
  const auto blue = edward::core::ComponentIr::parse({{"version", "1"}, {"root", blueRoot}});
  assert(blue);
  graph.setComponentLayers({{5, 10, *blue}});
  const auto beforeLayer = graph.build(timeline.snapshot(), {4});
  const auto duringLayer = graph.build(timeline.snapshot(), {5});
  const auto afterLayer = graph.build(timeline.snapshot(), {10});
  assert(beforeLayer && duringLayer && afterLayer);
  assert(beforeLayer->frame.pixelColor(4, 8).blue() < 100);
  assert(duringLayer->frame.pixelColor(4, 8).blue() > 150);
  assert(afterLayer->frame.pixelColor(4, 8).blue() < 100);
  QImage pluginFrame(QSize(16, 16), QImage::Format_RGBA8888);
  pluginFrame.fill(Qt::transparent);
  pluginFrame.setPixelColor(0, 0, QColor(0, 255, 0, 255));
  graph.setPluginFrame(pluginFrame);
  const auto pluginScene = graph.build(timeline.snapshot(), {0});
  assert(pluginScene.has_value());
  assert(pluginScene->frame.pixelColor(0, 0).green() > pluginScene->frame.pixelColor(0, 0).red());
  graph.setPluginFrame(std::nullopt);
  const auto baseline = graph.build(timeline.snapshot(), {0});
  assert(baseline.has_value());
  QImage wrongSize(QSize(8, 8), QImage::Format_RGBA8888);
  wrongSize.fill(Qt::red);
  graph.setPluginFrame(wrongSize);
  const auto unchanged = graph.build(timeline.snapshot(), {0});
  assert(unchanged.has_value());
  assert(unchanged->frame == baseline->frame);
  assert(!graph.build(timeline.snapshot(), {99}).has_value());
  return 0;
}
