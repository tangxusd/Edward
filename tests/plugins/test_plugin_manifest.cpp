#include <edward/plugins/plugin_manifest.hpp>
#include <edward/plugins/installed_plugin.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

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
  QTemporaryDir installedDirectory;
  assert(installedDirectory.isValid());
  const auto root = std::filesystem::path(installedDirectory.path().toStdString());
  QFile entry(QString::fromStdString((root / "host.mjs").string()));
  assert(entry.open(QIODevice::WriteOnly));
  entry.close();
  QFile installedManifest(QString::fromStdString((root / "edward-plugin.json").string()));
  assert(installedManifest.open(QIODevice::WriteOnly));
  installedManifest.write(QJsonDocument(valid).toJson(QJsonDocument::Compact));
  installedManifest.close();
  const auto installed = edward::plugins::loadInstalledPlugin(root, &error);
  assert(installed);
  assert(installed->manifest.pluginId == "remotion");
  std::filesystem::remove(root / "host.mjs");
  assert(!edward::plugins::loadInstalledPlugin(root, &error));
  return 0;
}
