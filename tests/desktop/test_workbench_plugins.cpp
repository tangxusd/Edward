#include <edward/desktop/workbench_runtime.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 3);
  edward::desktop::WorkbenchRuntime runtime;
  assert(!runtime.authenticated());
  assert(runtime.importMedia(QString::fromLocal8Bit(argv[2])));
  assert(runtime.importMedia(QString::fromLocal8Bit(argv[2])));
  assert(runtime.videoTrackCount() == 2);
  const auto timelineClips = runtime.clips();
  assert(timelineClips.size() == 2);
  assert(timelineClips.at(0).toMap().value("trackIndex").toInt() == 0);
  assert(timelineClips.at(1).toMap().value("trackIndex").toInt() == 1);
  assert(!runtime.signInWithSupabase("http://project.supabase.co", "anon-key", "demo@example.com", "password"));
  assert(!runtime.authenticated());
  assert(!runtime.uploadCurrentComponent("https://project.supabase.co/functions/v1/component-upload",
                                         "demo.component", "Demo component"));
  assert(!runtime.installedPluginAvailable());
  assert(!runtime.selectInstalledPlugin(QStringLiteral("/missing-plugin")));
  assert(runtime.loadComponentJson(QStringLiteral(
      "{\"version\":\"1\",\"root\":{\"id\":\"root\",\"type\":\"container\"},"
      "\"pluginDependency\":{\"pluginId\":\"remotion\",\"version\":\"1.0.0\"}}")));
  assert(runtime.componentPluginDependencyStatus() == QStringLiteral("缺少插件"));

  QTemporaryDir directory;
  assert(directory.isValid());
  const auto root = std::filesystem::path(directory.path().toStdString());
  QFile entry(QString::fromStdString((root / "host.mjs").string()));
  assert(entry.open(QIODevice::WriteOnly));
  entry.close();
  QFile manifest(QString::fromStdString((root / "edward-plugin.json").string()));
  assert(manifest.open(QIODevice::WriteOnly));
  manifest.write(QJsonDocument(QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"},
                                            {"entry", "host.mjs"}, {"capabilities", QJsonArray{"renderFrame"}}})
                     .toJson(QJsonDocument::Compact));
  manifest.close();

  assert(runtime.selectInstalledPlugin(directory.path()));
  assert(runtime.installedPluginAvailable());
  assert(runtime.installedPluginId() == "remotion");
  assert(runtime.componentPluginDependencyStatus() == QStringLiteral("插件可用"));
  assert(!runtime.describeInstalledPlugin(QStringLiteral("main")));
  std::filesystem::copy_file(argv[1], root / "rpc-plugin");
  QFile rpcManifest(QString::fromStdString((root / "edward-plugin.json").string()));
  assert(rpcManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
  rpcManifest.write(QJsonDocument(QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"},
                                               {"entry", "rpc-plugin"}, {"capabilities", QJsonArray{"describe"}},
                                               {"editableProps", QJsonArray{"opacity"}}})
                        .toJson(QJsonDocument::Compact));
  rpcManifest.close();
  assert(runtime.selectInstalledPlugin(directory.path()));
  assert(runtime.describeInstalledPlugin(QStringLiteral("main")));
  assert(runtime.demoOverlayEnabled());
  assert(runtime.componentJson().value("root").toObject().value("id") == "root");
  assert(runtime.componentJson().value("pluginDependency").toObject().value("pluginId") == "remotion");
  assert(runtime.componentJson().value("pluginDependency").toObject().value("version") == "1.0.0");
  runtime.generateComponentDraft();
  assert(!runtime.uploadCurrentComponent("https://project.supabase.co/functions/v1/component-upload",
                                         "demo.component", "Demo component"));
  assert(runtime.setPlayhead(12));
  runtime.setDemoOverlayX(-180);
  runtime.setDemoOverlayY(-40);
  runtime.setDemoOverlayWidth(300);
  runtime.setDemoOverlayHeight(100);
  runtime.setDemoOverlayOpacity(0.5);
  runtime.setDemoOverlayScale(1.5);
  runtime.setDemoOverlayRotation(30.0);
  const auto overlay = runtime.componentJson();
  const auto nodes = overlay.value("root").toObject().value("children").toArray();
  const auto box = nodes.at(0).toObject();
  assert(runtime.demoOverlayX() == -180);
  assert(box.value("transform").toObject().value("x").toInt() == -180);
  const auto keyframes = box.value("keyframes").toObject();
  for (const auto& field : {"x", "y", "width", "height", "scaleX", "scaleY", "rotation", "opacity"}) {
    bool atPlayhead = false;
    for (const auto& keyframe : keyframes.value(field).toArray())
      atPlayhead = atPlayhead || keyframe.toObject().value("frame").toInt() == 12;
    assert(atPlayhead);
  }
  assert(keyframes.value("opacity").toArray().last().toObject().value("value").toDouble() == 0.5);
  const auto transformedBox = runtime.componentJson().value("root").toObject().value("children").toArray().at(0).toObject();
  assert(transformedBox.value("transform").toObject().value("scaleX").toDouble() == 1.5);
  assert(transformedBox.value("transform").toObject().value("rotation").toDouble() == 30.0);
  const auto savedPath = directory.path() + QStringLiteral("/component.json");
  assert(runtime.saveComponentJson(savedPath));
  const auto packagePath = directory.path() + QStringLiteral("/package");
  assert(runtime.saveComponentPackage(packagePath, QStringLiteral("demo.component"), QStringLiteral("Demo component")));
  assert(QFile::exists(packagePath + QStringLiteral("/manifest.json")));
  assert(QFile::exists(packagePath + QStringLiteral("/component.json")));
  assert(!runtime.configureSilentComponentUploads(
      QStringLiteral("http://project.supabase.co/functions/v1/component-upload"),
      directory.path() + QStringLiteral("/upload-state.json"),
      directory.path() + QStringLiteral("/pending-components")));
  assert(runtime.configureSilentComponentUploads(
      QStringLiteral("https://project.supabase.co/functions/v1/component-upload"),
      directory.path() + QStringLiteral("/upload-state.json"),
      directory.path() + QStringLiteral("/pending-components")));
  QFile saved(savedPath);
  assert(saved.open(QIODevice::ReadOnly));
  assert(QJsonDocument::fromJson(saved.readAll()).object().value("version").toString() == "1");
  assert(runtime.loadComponentFile(savedPath));
  assert(!runtime.loadComponentFile(directory.path() + QStringLiteral("/missing.json")));
  edward::desktop::WorkbenchRuntime emptyRuntime;
  assert(!emptyRuntime.saveComponentJson(directory.path() + QStringLiteral("/empty.json")));
  runtime.clearInstalledPlugin();
  assert(!runtime.installedPluginAvailable());
  return 0;
}
