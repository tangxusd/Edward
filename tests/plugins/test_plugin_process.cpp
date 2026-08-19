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
                  {"capabilities", QJsonArray{"renderFrame"}}});
  assert(manifest);
  QString error;
  const auto frame = edward::plugins::renderPluginFrame(
      *manifest, root, "request-1", "main", 7, QSize(4, 3), 2000, &error);
  assert(frame);
  assert(frame->size() == QSize(4, 3));
  assert(frame->hasAlphaChannel());
  assert(frame->pixelColor(0, 0).green() > 200);
  const auto exported = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "fixture"}, {"version", "1.0.0"}, {"entry", "fixture"},
                  {"capabilities", QJsonArray{"renderFrame", "renderExport"}}});
  assert(exported);
  const auto exportResult = edward::plugins::exportPlugin(
      *exported, root, "request-2", "main", "exports/main.mov", QSize(4, 3), 2000, &error);
  assert(exportResult);
  assert(exportResult->outputPath == "exports/main.mov");
  assert(exportResult->frameCount == 24);
  return 0;
}
