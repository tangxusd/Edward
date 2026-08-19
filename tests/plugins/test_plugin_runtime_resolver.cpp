#include <edward/plugins/plugin_runtime_resolver.hpp>

#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main() {
  QString error;
  const auto node = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "node"}, {"version", "1"}, {"entry", "host.mjs"},
                  {"runtime", "node"}}, &error);
  assert(node);

  edward::plugins::PluginRuntimeResolution release;
  assert(!edward::plugins::resolvePluginRuntime(*node, release, &error));
  assert(error == "bundled plugin runtime is required");

  QTemporaryDir runtimeDirectory;
  assert(runtimeDirectory.isValid());
  const auto runtimeRoot = std::filesystem::path(runtimeDirectory.path().toStdString());
  QFile bundled(QString::fromStdString((runtimeRoot / "node").string()));
  assert(bundled.open(QIODevice::WriteOnly));
  bundled.write("runtime");
  bundled.close();
  release.bundledRoot = runtimeRoot;
  const auto resolved = edward::plugins::resolvePluginRuntime(*node, release, &error);
  assert(resolved && std::filesystem::path(resolved->toStdString()) == runtimeRoot / "node");

  release.bundledRoot = runtimeRoot / "missing";
  assert(!edward::plugins::resolvePluginRuntime(*node, release, &error));
  assert(error == "bundled plugin runtime is unavailable");

  edward::plugins::PluginRuntimeResolution development;
  development.allowDevelopmentPath = true;
  assert(edward::plugins::resolvePluginRuntime(*node, development, &error));
  return 0;
}
