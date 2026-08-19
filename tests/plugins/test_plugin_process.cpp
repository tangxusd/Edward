#include <edward/plugins/plugin_host.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 2);
  QTemporaryDir directory;
  assert(directory.isValid());
  const auto root = std::filesystem::path(directory.path().toStdString());
  assert(std::filesystem::copy_file(argv[1], root / "fixture"));
  const auto manifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "fixture"}, {"version", "1.0.0"}, {"entry", "fixture"},
                  {"capabilities", QJsonArray{"describe", "renderFrame"}},
                  {"editableProps", QJsonArray{"opacity"}}});
  assert(manifest);
  QString error;
  const auto frame = edward::plugins::renderPluginFrame(
      *manifest, root, "request-1", "main", 7, QSize(4, 3), 2000, &error);
  assert(frame);
  assert(frame->size() == QSize(4, 3));
  assert(frame->hasAlphaChannel());
  assert(frame->pixelColor(0, 0).green() > 200);
  const auto described = edward::plugins::describePlugin(
      *manifest, root, "describe-1", "main", 2000, &error);
  assert(described);
  assert(described->toJson().value("root").toObject().value("id") == "root");
  qputenv("EDWARD_TEST_RPC_MODE", "describe-mismatch");
  assert(!edward::plugins::describePlugin(*manifest, root, "describe-2", "main", 2000, &error));
  qunsetenv("EDWARD_TEST_RPC_MODE");
  qputenv("EDWARD_TEST_RPC_MODE", "timeout");
  assert(!edward::plugins::renderPluginFrame(
      *manifest, root, "timeout-request", "main", 7, QSize(4, 3), 50, &error));
  qputenv("EDWARD_TEST_RPC_MODE", "crash");
  assert(!edward::plugins::renderPluginFrame(
      *manifest, root, "crash-request", "main", 7, QSize(4, 3), 2000, &error));
  qputenv("EDWARD_TEST_RPC_MODE", "opaque");
  assert(!edward::plugins::renderPluginFrame(
      *manifest, root, "opaque-request", "main", 7, QSize(4, 3), 2000, &error));
  qunsetenv("EDWARD_TEST_RPC_MODE");
  std::filesystem::create_symlink(argv[1], root / "outside-entry");
  const auto outsideManifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "outside"}, {"version", "1.0.0"}, {"entry", "outside-entry"},
                  {"capabilities", QJsonArray{"renderFrame"}}}, &error);
  assert(outsideManifest);
  assert(!edward::plugins::renderPluginFrame(
      *outsideManifest, root, "outside-request", "main", 7, QSize(4, 3), 2000, &error));
  std::filesystem::remove(root / "outside-entry");
  QFile nodeEntry(QString::fromStdString((root / "node-fixture.mjs").string()));
  assert(nodeEntry.open(QIODevice::WriteOnly));
  nodeEntry.write("process.stdout.write('node-ok\\n');\n");
  nodeEntry.close();
  const auto nodeManifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "node-fixture"}, {"version", "1.0.0"},
                  {"entry", "node-fixture.mjs"}, {"runtime", "node"}}, &error);
  assert(nodeManifest);
  const auto nodeResult = edward::plugins::launchPluginProcess(
      *nodeManifest, root, {}, 2000);
  assert(nodeResult.started && nodeResult.exitCode == 0);
  assert(nodeResult.standardOutput.contains("node-ok"));
  const auto exported = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "fixture"}, {"version", "1.0.0"}, {"entry", "fixture"},
                  {"capabilities", QJsonArray{"renderFrame", "renderExport"}}});
  assert(exported);
  const edward::plugins::RenderExportRequest exportRequest{
      "request-2", "main", "main.mov", QSize(4, 3), 24, 25, 1, 2000};
  const auto exportResult = edward::plugins::exportPlugin(
      *exported, root, root, exportRequest, &error);
  assert(exportResult);
  assert(exportResult->outputPath == "main.mov");
  assert(exportResult->frameCount == 24);
  assert(std::filesystem::is_regular_file(root / "main.mov"));
  assert(std::filesystem::file_size(root / "main.mov") > 0);
  return 0;
}
