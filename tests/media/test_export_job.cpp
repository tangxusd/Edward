#include <edward/core/timeline.hpp>
#include <edward/core/component_ir.hpp>
#include <edward/media/export_job.hpp>
#include <edward/media/media_probe.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/render_graph.hpp>

#include <cassert>
#include <filesystem>

#include <QJsonArray>
#include <QJsonObject>

int main(int argc, char** argv) {
  assert(argc == 3);
  const std::filesystem::path output = argv[2];
  std::filesystem::remove(output);
  edward::core::Timeline timeline(25);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, argv[1], 0, 25, 0}));
  const edward::media::MltAdapter adapter;
  const QJsonObject root{{"id", "root"}, {"type", "container"}, {"children", QJsonArray{
      QJsonObject{{"id", "overlay"}, {"type", "shape"},
                  {"transform", QJsonObject{{"width", 8}, {"height", 8}}},
                  {"properties", QJsonObject{{"fill", "#ff0000"}}}},
  }}};
  const auto overlay = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(overlay);
  const edward::media::RenderGraph graph(adapter, *overlay);
  const edward::media::ExportJob job(graph);
  assert(!job.run(timeline.snapshot(), {output, {}, 25, 1}));
  assert(!job.run(timeline.snapshot(), {output, {1920, 1080}, 0, 1}));
  const auto result = job.run(timeline.snapshot(), {output, {1920, 1080}, 25, 1,
                                                    edward::media::ExportQuality::High});
  assert(result.has_value());
  assert(result->frameCount == 25);
  const auto info = edward::media::MediaProbe::probe(output);
  assert(info.has_value());
  assert(info->width == 1920 && info->height == 1080);
  assert(info->durationFrames == 25);
  assert(info->hasAudio);
  edward::core::Timeline renderedTimeline(25);
  const auto renderedTrack = renderedTimeline.addVideoTrack();
  assert(renderedTimeline.insertClip({1, renderedTrack, output, 0, 25, 0}));
  const auto exportedFrame = adapter.renderFrame(renderedTimeline.snapshot(), 0);
  assert(exportedFrame);
  const auto center = exportedFrame->pixelColor(exportedFrame->width() / 2, exportedFrame->height() / 2);
  assert(center.red() > center.green() + 80);
  assert(center.red() > center.blue() + 80);
  edward::core::Timeline componentOnlyTimeline(25);
  const auto componentOnlyTrack = componentOnlyTimeline.addVideoTrack();
  const QJsonObject componentRoot{{"id", "component-root"}, {"type", "container"}, {"children", QJsonArray{
      QJsonObject{{"id", "component-shape"}, {"type", "shape"},
                  {"transform", QJsonObject{{"width", 320}, {"height", 180}}},
                  {"properties", QJsonObject{{"fill", "#00a8c8"}}}},
  }}};
  const auto component = edward::core::ComponentIr::parse({{"version", "1"}, {"root", componentRoot}});
  assert(component);
  assert(componentOnlyTimeline.insertClip({2, componentOnlyTrack, {}, 0, 25, 0,
                                           edward::core::TimelineClipKind::Component, *component}));
  const auto componentOnlyOutput = output.parent_path() / "component-only.mp4";
  std::filesystem::remove(componentOnlyOutput);
  const edward::media::RenderGraph componentOnlyGraph(adapter);
  const edward::media::ExportJob componentOnlyJob(componentOnlyGraph);
  const auto componentOnlyResult = componentOnlyJob.run(componentOnlyTimeline.snapshot(),
                                                         {componentOnlyOutput, {1280, 720}, 25, 1});
  assert(componentOnlyResult);
  const auto componentOnlyInfo = edward::media::MediaProbe::probe(componentOnlyOutput);
  assert(componentOnlyInfo && componentOnlyInfo->width == 1280 && componentOnlyInfo->height == 720);
  assert(componentOnlyInfo->durationFrames == 25);
  std::filesystem::remove(componentOnlyOutput);
  std::filesystem::remove(output);
  return 0;
}
