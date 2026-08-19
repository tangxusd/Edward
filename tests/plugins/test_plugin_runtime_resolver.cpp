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
  release.requireIntegrity = true;
  assert(!edward::plugins::resolvePluginRuntime(*node, release, &error));
  assert(error == "bundled plugin runtime checksum is required");
  release.expectedSha256 = "3f0a377ba0a4a460ecb378c1012e6557653a7427d7dc761f7c1c6a4e58f0fcb7";
  assert(!edward::plugins::resolvePluginRuntime(*node, release, &error));
  assert(error == "bundled plugin runtime checksum mismatch");
  release.expectedSha256 = "d92c6a81b2ff50096bcda80885427d1f59a25b5f483f7055523504925d16ab23";
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
