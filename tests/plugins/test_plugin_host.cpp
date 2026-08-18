#include <edward/plugins/plugin_host.hpp>

#include <QJsonArray>
#include <QJsonObject>

#include <cassert>
#include <filesystem>

int main() {
  const auto manifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"}, {"entry", "host.mjs"},
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
  return 0;
}
