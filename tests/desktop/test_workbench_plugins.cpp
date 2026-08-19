#include <edward/desktop/workbench_runtime.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main() {
  edward::desktop::WorkbenchRuntime runtime;
  assert(!runtime.authenticated());
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
  runtime.generateComponentDraft();
  assert(!runtime.uploadCurrentComponent("https://project.supabase.co/functions/v1/component-upload",
                                         "demo.component", "Demo component"));
  assert(runtime.setPlayhead(12));
  runtime.setDemoOverlayX(80);
  runtime.setDemoOverlayOpacity(0.5);
  const auto overlay = runtime.componentJson();
  const auto nodes = overlay.value("root").toObject().value("children").toArray();
  const auto box = nodes.at(0).toObject();
  bool xAtPlayhead = false;
  for (const auto& keyframe : box.value("keyframes").toObject().value("x").toArray())
    xAtPlayhead = xAtPlayhead || keyframe.toObject().value("frame").toInt() == 12;
  assert(xAtPlayhead);
  assert(box.value("keyframes").toObject().value("opacity").toArray().last().toObject().value("value").toDouble() == 0.5);
  runtime.setDemoOverlayScale(1.5);
  runtime.setDemoOverlayRotation(30.0);
  const auto transformedBox = runtime.componentJson().value("root").toObject().value("children").toArray().at(0).toObject();
  assert(transformedBox.value("transform").toObject().value("scaleX").toDouble() == 1.5);
  assert(transformedBox.value("transform").toObject().value("rotation").toDouble() == 30.0);
  const auto savedPath = directory.path() + QStringLiteral("/component.json");
  assert(runtime.saveComponentJson(savedPath));
  const auto packagePath = directory.path() + QStringLiteral("/package");
  assert(runtime.saveComponentPackage(packagePath, QStringLiteral("demo.component"), QStringLiteral("Demo component")));
  assert(QFile::exists(packagePath + QStringLiteral("/manifest.json")));
  assert(QFile::exists(packagePath + QStringLiteral("/component.json")));
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
