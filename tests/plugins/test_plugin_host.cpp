#include <edward/plugins/plugin_host.hpp>

#include <QJsonArray>
#include <QJsonObject>

#include <cassert>
#include <filesystem>

int main() {
  QString error;
  const auto manifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"}, {"entry", "host.mjs"},
                  {"capabilities", QJsonArray{"renderFrame"}},
                  {"permissions", QJsonArray{"read_input_asset", "write_draft_output"}}});
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
