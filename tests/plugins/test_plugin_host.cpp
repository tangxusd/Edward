#include <edward/plugins/plugin_host.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QBuffer>

#include <cassert>
#include <filesystem>

int main() {
  QString error;
  const auto manifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"}, {"entry", "host.mjs"},
                  {"capabilities", QJsonArray{"renderFrame"}},
                  {"permissions", QJsonArray{"read_input_asset", "write_draft_output"}},
                  {"editableProps", QJsonArray{"opacity"}}});
  assert(manifest);
  const auto taskRoot = std::filesystem::temp_directory_path();
  edward::plugins::PluginRequest request{"task-1", "remotion", "1.0.0", "read_input_asset", "input.png", "draft.json", 1024};
  assert(edward::plugins::validateRequest(*manifest, request, taskRoot));
  request.inputPath = "../outside.png";
  assert(!edward::plugins::validateRequest(*manifest, request, taskRoot));
  request.inputPath = "input.png";
  request.operation = "engine_write";
  assert(!edward::plugins::validateRequest(*manifest, request, taskRoot));
  const auto rpc = edward::plugins::RpcRequest::parse(
      QJsonObject{{"jsonrpc", "2.0"}, {"id", "7"}, {"method", "renderFrame"}, {"params", QJsonObject{{"frame", 0}}}}, &error);
  assert(rpc);
  assert(rpc->toJson().value("method").toString() == "renderFrame");
  assert(!edward::plugins::RpcRequest::parse(QJsonObject{{"id", "7"}, {"method", "renderFrame"}}, &error));
  assert(edward::plugins::validateRpcMethod(*manifest, "renderFrame"));
  assert(!edward::plugins::validateRpcMethod(*manifest, "deleteProject", &error));
  assert(edward::plugins::validateRpcParams("renderFrame", QJsonObject{{"frame", 0}, {"width", 1920}, {"height", 1080}}));
  assert(!edward::plugins::validateRpcParams("renderFrame", QJsonObject{{"frame", -1}, {"width", 1920}, {"height", 1080}}, &error));
  assert(!edward::plugins::validateRpcParams("renderExport", QJsonObject{{"outputPath", "../out.mov"}, {"width", 1920}, {"height", 1080}}, &error));
  const QJsonObject component{{"version", "1"}, {"root", QJsonObject{{"id", "root"}, {"type", "container"}}}};
  const auto described = edward::plugins::parseDescribeResult(
      *manifest, QJsonObject{{"compositionId", "comp-1"}, {"component", component},
                             {"editableProps", QJsonArray{"opacity"}}}, &error);
  assert(described);
  assert(!edward::plugins::parseDescribeResult(
      *manifest, QJsonObject{{"compositionId", "comp-1"}, {"component", component},
                             {"editableProps", QJsonArray{"forbidden"}}}, &error));
  QImage frameImage(QSize(2, 2), QImage::Format_RGBA8888);
  frameImage.fill(Qt::transparent);
  QByteArray encoded;
  QBuffer buffer(&encoded);
  buffer.open(QIODevice::WriteOnly);
  frameImage.save(&buffer, "PNG");
  const auto frame = edward::plugins::parseRenderFrameResult(
      QJsonObject{{"frame", 3}, {"pngBase64", QString::fromUtf8(encoded.toBase64())}}, 3, QSize(2, 2), &error);
  assert(frame);
  assert(!edward::plugins::parseRenderFrameResult(
      QJsonObject{{"frame", 3}, {"pngBase64", QString::fromUtf8(encoded.toBase64())}}, 4, QSize(2, 2), &error));
  const edward::plugins::RpcResponse frameResponse{
      QStringLiteral("frame-1"),
      QJsonObject{{"frame", 3}, {"pngBase64", QString::fromUtf8(encoded.toBase64())}},
      {}};
  assert(edward::plugins::parseRenderFrameResponse(frameResponse, "frame-1", 3, QSize(2, 2), &error));
  assert(!edward::plugins::parseRenderFrameResponse(frameResponse, "frame-2", 3, QSize(2, 2), &error));
  assert(!edward::plugins::renderPluginFrame(
      *manifest, std::filesystem::path("/missing"), "", "comp-1", 3, QSize(2, 2), 1000, &error));
  assert(!edward::plugins::renderPluginFrame(
      *manifest, std::filesystem::path("/missing"), "frame-1", "comp-1", -1, QSize(2, 2), 1000, &error));
  const auto exportResult = edward::plugins::parseRenderExportResult(
      QJsonObject{{"outputPath", "exports/animation.mov"}, {"width", 1920}, {"height", 1080},
                  {"frameCount", 120}, {"hasAlpha", true}}, &error);
  assert(exportResult);
  assert(exportResult->size == QSize(1920, 1080));
  assert(exportResult->frameCount == 120);
  assert(exportResult->hasAlpha);
  assert(!edward::plugins::parseRenderExportResult(
      QJsonObject{{"outputPath", "../animation.mov"}, {"width", 1920}, {"height", 1080},
                  {"frameCount", 120}, {"hasAlpha", true}}, &error));
  assert(!edward::plugins::parseRenderExportResult(
      QJsonObject{{"outputPath", "exports/animation.mov"}, {"width", 0}, {"height", 1080},
                  {"frameCount", 120}, {"hasAlpha", true}}, &error));
  const auto processManifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "true"}, {"version", "1.0.0"}, {"entry", "true"}});
  assert(processManifest);
  const auto process = edward::plugins::launchPluginProcess(
      *processManifest, std::filesystem::path("/usr/bin"), {}, 1000);
  assert(process.started);
  assert(!process.timedOut);
  assert(process.exitCode == 0);
  const auto response = edward::plugins::RpcResponse::parse(
      QJsonObject{{"id", "7"}, {"result", QJsonObject{{"ok", true}}}}, &error);
  assert(response);
  assert(response->result.value("ok").toBool());
  assert(!edward::plugins::RpcResponse::parse(QJsonObject{{"id", "7"}}, &error));
  return 0;
}
