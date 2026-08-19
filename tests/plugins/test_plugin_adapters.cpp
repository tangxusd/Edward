#include <edward/plugins/plugin_manifest.hpp>
#include <edward/plugins/plugin_host.hpp>

#include <QFile>
#include <QJsonDocument>

#include <cassert>
#include <filesystem>

namespace {

void verifyAdapter(const char* directory, const char* expectedId) {
  const auto root = std::filesystem::path(EDWARD_SOURCE_DIR) / directory;
  QFile manifestFile(QString::fromStdString((root / "edward-plugin.json").string()));
  assert(manifestFile.open(QIODevice::ReadOnly));
  const auto document = QJsonDocument::fromJson(manifestFile.readAll());
  assert(document.isObject());
  QString error;
  const auto manifest = edward::plugins::PluginManifest::parse(document.object(), &error);
  assert(manifest);
  assert(manifest->pluginId == expectedId);
  assert(manifest->runtime == "node");
  assert(manifest->capabilities.contains("describe"));
  assert(manifest->capabilities.contains("renderFrame"));
  assert(manifest->allows("read_input_asset"));
  assert(manifest->allows("write_draft_output"));

  QFile entry(QString::fromStdString((root / manifest->entry.toStdString()).string()));
  assert(entry.open(QIODevice::ReadOnly));
  const auto source = QString::fromUtf8(entry.readAll());
  assert(source.contains("renderFrame"));
  assert(source.contains("describe"));
  assert(source.contains("pngBase64"));
  if (QString::fromLatin1(expectedId) == "edward.hyperframes") {
    QFile composition(QString::fromStdString((root / "composition/index.html").string()));
    assert(composition.open(QIODevice::ReadOnly));
    const auto compositionSource = QString::fromUtf8(composition.readAll());
    assert(compositionSource.contains("data-duration"));
    assert(compositionSource.contains("data-no-timeline"));
  }
}

void verifyDescribe(const char* directory, const char* compositionId) {
  const auto root = std::filesystem::path(EDWARD_SOURCE_DIR) / directory;
  QFile manifestFile(QString::fromStdString((root / "edward-plugin.json").string()));
  assert(manifestFile.open(QIODevice::ReadOnly));
  QString error;
  const auto manifest = edward::plugins::PluginManifest::parse(
      QJsonDocument::fromJson(manifestFile.readAll()).object(), &error);
  assert(manifest);
  const auto component = edward::plugins::describePlugin(
      *manifest, root, "adapter-describe", compositionId, 10000, &error);
  assert(component);
  assert(component->toJson().value("root").toObject().value("id") == "root");
}

}  // namespace

int main() {
  verifyAdapter("plugins/remotion-host", "edward.remotion");
  verifyAdapter("plugins/hyperframes-host", "edward.hyperframes");
  verifyDescribe("plugins/remotion-host", "EdwardAnimation");
  verifyDescribe("plugins/hyperframes-host", "EdwardCard");
  return 0;
}
