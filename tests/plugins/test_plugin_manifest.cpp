#include <edward/plugins/plugin_manifest.hpp>

#include <QJsonArray>
#include <QJsonObject>

#include <cassert>

int main() {
  const QJsonObject valid{{"pluginId", "remotion"}, {"version", "1.0.0"}, {"entry", "host.mjs"},
                          {"capabilities", QJsonArray{"describe", "renderFrame"}},
                          {"permissions", QJsonArray{"read_input_asset", "write_draft_output"}},
                          {"editableProps", QJsonArray{"x", "opacity"}}};
  QString error;
  const auto manifest = edward::plugins::PluginManifest::parse(valid, &error);
  assert(manifest);
  assert(manifest->allows("read_input_asset"));
  assert(!manifest->allows("network"));

  assert(!edward::plugins::PluginManifest::parse(QJsonObject{{"pluginId", "remotion"}}, &error));
  assert(!edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1"}, {"entry", "/tmp/host.mjs"}}, &error));
  assert(!edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1"}, {"entry", "host.mjs"},
                  {"permissions", QJsonArray{"network"}}}, &error));
  return 0;
}
