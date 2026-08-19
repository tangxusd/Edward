#include <edward/plugins/plugin_manifest.hpp>
#include <edward/plugins/installed_plugin.hpp>
#include <edward/core/component_ir.hpp>

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
  assert(manifest->runtime == "native");
  assert(manifest->allows("read_input_asset"));
  assert(!manifest->allows("network"));

  assert(!edward::plugins::PluginManifest::parse(QJsonObject{{"pluginId", "remotion"}}, &error));
  assert(!edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1"}, {"entry", "/tmp/host.mjs"}}, &error));
  assert(!edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1"}, {"entry", "host.mjs"},
                  {"permissions", QJsonArray{"network"}}}, &error));
  const auto nodeManifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1"}, {"entry", "host.mjs"},
                  {"runtime", "node"}}, &error);
  assert(nodeManifest && nodeManifest->runtime == "node");
  assert(!edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "remotion"}, {"version", "1"}, {"entry", "host.mjs"},
                  {"runtime", "python"}}, &error));
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
  const auto dependent = edward::core::ComponentIr::parse(
      QJsonObject{{"version", "1"}, {"root", QJsonObject{{"id", "root"}, {"type", "container"}}},
                  {"pluginDependency", QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"}}}});
  assert(dependent);
  assert(edward::plugins::dependencyStatus(*dependent, installed) ==
         edward::plugins::PluginDependencyStatus::Available);
  auto mismatch = *installed;
  mismatch.manifest.version = "2.0.0";
  assert(edward::plugins::dependencyStatus(*dependent, mismatch) ==
         edward::plugins::PluginDependencyStatus::VersionMismatch);
  assert(edward::plugins::dependencyStatus(*dependent, std::nullopt) ==
         edward::plugins::PluginDependencyStatus::Missing);
  std::filesystem::remove(root / "host.mjs");
  assert(!edward::plugins::loadInstalledPlugin(root, &error));
  return 0;
}
