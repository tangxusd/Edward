#include <edward/core/timeline.hpp>
#include <edward/core/timeline_commands.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <cassert>

int main(int argc, char** argv) {
  assert(argc == 3);
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
  graph.setComponentLayers({{5, 10, 0, *blue}});
  const auto beforeLayer = graph.build(timeline.snapshot(), {4});
  const auto duringLayer = graph.build(timeline.snapshot(), {5});
  const auto afterLayer = graph.build(timeline.snapshot(), {10});
  assert(beforeLayer && duringLayer && afterLayer);
  assert(beforeLayer->frame.pixelColor(4, 8).blue() < 100);
  assert(duringLayer->frame.pixelColor(4, 8).blue() > 150);
  assert(afterLayer->frame.pixelColor(4, 8).blue() < 100);
  const auto componentTrack = timeline.addVideoTrack();
  assert(timeline.insertClip({2, componentTrack, {}, 0, 5, 15, edward::core::TimelineClipKind::Component, *blue}));
  const auto componentClipScene = graph.build(timeline.snapshot(), {15});
  assert(componentClipScene);
  assert(componentClipScene->frame.pixelColor(4, 8).blue() > 150);
  const QJsonObject animatedRoot{{"id", "animated-root"}, {"type", "shape"},
      {"transform", QJsonObject{{"y", 0}, {"width", 4}, {"height", 4}}},
      {"properties", QJsonObject{{"fill", "#00ff00"}}},
      {"keyframes", QJsonObject{{"x", QJsonArray{
          QJsonObject{{"frame", 0}, {"value", 0}},
          QJsonObject{{"frame", 10}, {"value", 8}}
      }}}}};
  const auto animated = edward::core::ComponentIr::parse({{"version", "1"}, {"root", animatedRoot}});
  assert(animated);
  edward::core::Timeline splitTimeline(40);
  const auto splitTrack = splitTimeline.addVideoTrack();
  assert(splitTimeline.insertClip({7, splitTrack, {}, 0, 20, 0,
                                   edward::core::TimelineClipKind::Component, *animated}));
  assert(splitTimeline.setPlayhead(10));
  edward::core::TimelineCommands splitCommands(splitTimeline);
  assert(splitCommands.splitClipAtPlayhead(7));
  const edward::media::RenderGraph splitGraph(adapter);
  const auto splitScene = splitGraph.build(splitTimeline.snapshot(), {10});
  assert(splitScene);
  const auto secondSegment = splitTimeline.clip(8);
  assert(secondSegment && secondSegment->sourceIn == 10);
  const auto secondScene = splitGraph.build(splitTimeline.snapshot(), {15});
  assert(secondScene);
  bool hasGreen = false;
  for (int y = 0; y < secondScene->frame.height() && !hasGreen; ++y)
    for (int x = 0; x < secondScene->frame.width(); ++x)
      hasGreen = hasGreen || secondScene->frame.pixelColor(x, y).green() > 150;
  assert(hasGreen);
  edward::core::Timeline localTimeTimeline(25);
  const auto localTimeTrack = localTimeTimeline.addVideoTrack();
  assert(localTimeTimeline.insertClip({10, localTimeTrack, argv[1], 0, 25, 0}));
  const auto animatedTrack = localTimeTimeline.addVideoTrack();
  assert(localTimeTimeline.insertClip({11, animatedTrack, {}, 0, 10, 15,
                                       edward::core::TimelineClipKind::Component, *animated}));
  const edward::media::RenderGraph localTimeGraph(adapter);
  const auto beforeAnimatedClip = localTimeGraph.build(localTimeTimeline.snapshot(), {14});
  const auto animatedStart = localTimeGraph.build(localTimeTimeline.snapshot(), {15});
  const auto animatedMiddle = localTimeGraph.build(localTimeTimeline.snapshot(), {20});
  const auto animatedLastFrame = localTimeGraph.build(localTimeTimeline.snapshot(), {24});
  assert(beforeAnimatedClip && animatedStart && animatedMiddle && animatedLastFrame);
  assert(beforeAnimatedClip->frame.pixelColor(8, 8).green() < 150);
  assert(animatedStart->frame.pixelColor(8, 8).green() > 150);
  assert(animatedMiddle->frame.pixelColor(4, 8).green() > 150);
  assert(animatedLastFrame->frame.pixelColor(1, 8).green() > 150);
  edward::core::Timeline componentOnlyTimeline(25);
  const auto componentOnlyTrack = componentOnlyTimeline.addVideoTrack();
  assert(componentOnlyTimeline.insertClip({3, componentOnlyTrack, {}, 0, 25, 0,
                                           edward::core::TimelineClipKind::Component, *blue}));
  const edward::media::RenderGraph componentOnlyGraph(adapter);
  const auto componentOnlyScene = componentOnlyGraph.build(componentOnlyTimeline.snapshot(), {0});
  assert(componentOnlyScene);
  assert(componentOnlyScene->frame.width() == 1920);
  assert(componentOnlyScene->frame.height() == 1080);
  assert(componentOnlyScene->frame.pixelColor(956, 538).blue() > 150);
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

  edward::core::Timeline transitionTimeline(20);
  const auto transitionTrack = transitionTimeline.addVideoTrack();
  assert(transitionTimeline.insertClip({21, transitionTrack, {}, 0, 10, 0,
                                        edward::core::TimelineClipKind::Component, *blue}));
  assert(transitionTimeline.insertClip({22, transitionTrack, {}, 0, 10, 10,
                                        edward::core::TimelineClipKind::Component, *animated}));
  assert(transitionTimeline.addTransition(edward::core::TransitionType::FlashBlack, 21, 22, 6));
  const edward::media::RenderGraph transitionGraph(adapter);
  const auto transitionBefore = transitionGraph.build(transitionTimeline.snapshot(), {11});
  const auto transitionDuring = transitionGraph.build(transitionTimeline.snapshot(), {15});
  assert(transitionBefore && transitionDuring);
  assert(transitionBefore->frame.pixelColor(960, 540).green() > 150);
  assert(transitionDuring->frame.pixelColor(960, 540).red() < 10 &&
         transitionDuring->frame.pixelColor(960, 540).green() < 10 &&
         transitionDuring->frame.pixelColor(960, 540).blue() < 10);
  edward::core::Timeline dissolveTimeline(20);
  const auto dissolveTrack = dissolveTimeline.addVideoTrack();
  const QJsonObject redRoot{{"id", "red"}, {"type", "shape"},
      {"transform", QJsonObject{{"width", 1920}, {"height", 1080}}},
      {"properties", QJsonObject{{"fill", "#ff0000"}}}};
  const QJsonObject whiteRoot{{"id", "white"}, {"type", "shape"},
      {"transform", QJsonObject{{"width", 1920}, {"height", 1080}}},
      {"properties", QJsonObject{{"fill", "#ffffff"}}}};
  const auto red = edward::core::ComponentIr::parse({{"version", "1"}, {"root", redRoot}});
  const auto white = edward::core::ComponentIr::parse({{"version", "1"}, {"root", whiteRoot}});
  assert(red && white);
  assert(dissolveTimeline.insertClip({31, dissolveTrack, {}, 0, 10, 0,
                                      edward::core::TimelineClipKind::Component, *red}));
  assert(dissolveTimeline.insertClip({32, dissolveTrack, {}, 0, 10, 10,
                                      edward::core::TimelineClipKind::Component, *white}));
  assert(dissolveTimeline.addTransition(edward::core::TransitionType::Dissolve, 31, 32, 6));
  const auto dissolveScene = transitionGraph.build(dissolveTimeline.snapshot(), {12});
  assert(dissolveScene);
  const auto dissolvePixel = dissolveScene->frame.pixelColor(960, 540);
  assert(dissolvePixel.red() > 100 && dissolvePixel.green() > 100 && dissolvePixel.blue() > 100);
  const auto dissolveMiddleScene = transitionGraph.build(dissolveTimeline.snapshot(), {7});
  assert(dissolveMiddleScene);
  const auto dissolveMiddlePixel = dissolveMiddleScene->frame.pixelColor(960, 540);
  assert(dissolveMiddlePixel.red() > 120 && dissolveMiddlePixel.green() > 70 &&
         dissolveMiddlePixel.blue() > 70);
  edward::core::Timeline mediaDissolveTimeline(20);
  const auto mediaDissolveTrack = mediaDissolveTimeline.addVideoTrack();
  assert(mediaDissolveTimeline.insertClip({41, mediaDissolveTrack, argv[1], 0, 10, 0}));
  assert(mediaDissolveTimeline.insertClip({42, mediaDissolveTrack, argv[2], 0, 10, 10}));
  assert(mediaDissolveTimeline.addTransition(edward::core::TransitionType::Dissolve, 41, 42, 6));
  const auto mediaDissolveScene = transitionGraph.build(mediaDissolveTimeline.snapshot(), {7});
  assert(mediaDissolveScene);
  assert(mediaDissolveScene->frame.pixelColor(8, 8).red() > 50);
  assert(!graph.build(timeline.snapshot(), {99}).has_value());
  return 0;
}
