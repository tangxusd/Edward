#include <edward/desktop/workbench_runtime.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 3);
  QCoreApplication application(argc, argv);
  edward::desktop::WorkbenchRuntime runtime;
  assert(!runtime.authenticated());
  assert(runtime.importMedia(QString::fromLocal8Bit(argv[2])));
  assert(runtime.importMedia(QString::fromLocal8Bit(argv[2])));
  assert(runtime.videoTrackCount() == 2);
  const auto timelineClips = runtime.clips();
  assert(timelineClips.size() == 2);
  assert(timelineClips.at(0).toMap().value("trackIndex").toInt() == 0);
  assert(timelineClips.at(1).toMap().value("trackIndex").toInt() == 1);
  assert(runtime.selectClip(timelineClips.at(1).toMap().value("id").toLongLong()));
  assert(runtime.moveSelected(10));
  assert(runtime.clips().at(1).toMap().value("timelineStart").toLongLong() == 10);
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
  runtime.clearComponentOverlay();
  const auto exportPath = directory.path() + QStringLiteral("/timeline.mp4");
  bool exported = false;
  QEventLoop exportLoop;
  QObject::connect(&runtime, &edward::desktop::WorkbenchRuntime::operationSucceeded,
                   [&exported, &exportLoop](const QString& message) {
                     if (message.startsWith(QStringLiteral("视频已导出："))) {
                       exported = true;
                       exportLoop.quit();
                     }
                   });
  assert(runtime.exportTimeline(exportPath));
  QTimer::singleShot(15000, &exportLoop, &QEventLoop::quit);
  exportLoop.exec();
  assert(exported);
  assert(QFile::exists(exportPath));
  QFile entry(QString::fromStdString((root / "host.mjs").string()));
  assert(entry.open(QIODevice::WriteOnly));
  entry.close();
  QFile manifest(QString::fromStdString((root / "edward-plugin.json").string()));
  assert(manifest.open(QIODevice::WriteOnly));
  manifest.write(QJsonDocument(QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"},
                                            {"entry", "host.mjs"}, {"capabilities", QJsonArray{"renderFrame"}}})
                     .toJson(QJsonDocument::Compact));
  manifest.close();

  assert(runtime.loadComponentJson(QStringLiteral(
      "{\"version\":\"1\",\"root\":{\"id\":\"root\",\"type\":\"container\"},"
      "\"pluginDependency\":{\"pluginId\":\"remotion\",\"version\":\"1.0.0\"}}")));
  assert(runtime.selectInstalledPlugin(directory.path()));
  assert(runtime.installedPluginAvailable());
  assert(runtime.installedPluginId() == "remotion");
  assert(runtime.componentPluginDependencyStatus() == QStringLiteral("插件可用"));
  assert(!runtime.describeInstalledPlugin(QStringLiteral("main")));
  std::filesystem::copy_file(argv[1], root / "rpc-plugin");
  QFile rpcManifest(QString::fromStdString((root / "edward-plugin.json").string()));
  assert(rpcManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
  rpcManifest.write(QJsonDocument(QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"},
                                               {"entry", "rpc-plugin"}, {"capabilities", QJsonArray{"describe", "renderExport"}},
                                               {"editableProps", QJsonArray{"opacity"}}})
                        .toJson(QJsonDocument::Compact));
  rpcManifest.close();
  assert(runtime.selectInstalledPlugin(directory.path()));
  assert(runtime.describeInstalledPlugin(QStringLiteral("main")));
  assert(runtime.demoOverlayEnabled());
  assert(runtime.componentJson().value("root").toObject().value("id") == "root");
  assert(runtime.componentJson().value("pluginDependency").toObject().value("pluginId") == "remotion");
  assert(runtime.componentJson().value("pluginDependency").toObject().value("version") == "1.0.0");
  assert(!runtime.exportTimeline(directory.path() + QStringLiteral("/plugin-component.mp4")));
  assert(runtime.proposeAiComponentCommand(
      QStringLiteral("{\"operation\":\"setProperty\",\"nodeId\":\"root\",\"field\":\"opacity\",\"value\":0.5}")));
  assert(runtime.applyPendingAiComponentCommand());
  assert(runtime.componentJson().value("root").toObject().value("properties").toObject().value("opacity").toDouble() == 0.5);
  assert(!runtime.proposeAiComponentCommand(
      QStringLiteral("{\"operation\":\"setProperty\",\"nodeId\":\"root\",\"field\":\"fill\",\"value\":\"#ffffff\"}")));
  runtime.generateComponentDraft();
  assert(!runtime.requestAiComponentDraft(QStringLiteral("http://model.example.com/v1/chat/completions"),
                                          QStringLiteral("runtime-key"), QStringLiteral("model"),
                                          QStringLiteral("move left")));
  assert(!runtime.requestAiComponentDraft(QStringLiteral("https://model.example.com/v1/chat/completions"),
                                          QStringLiteral("runtime-key"), QStringLiteral("model"),
                                          QString(70000, QLatin1Char('x'))));
  assert(runtime.proposeAiComponentCommand(
      QStringLiteral("{\"operation\":\"setTransformNumber\",\"nodeId\":\"demo-box\",\"field\":\"x\",\"value\":-72}")));
  assert(runtime.aiComponentDraftAvailable());
  assert(runtime.demoOverlayX() != -72);
  assert(runtime.bindComponentToSelectedClip());
  assert(runtime.componentBoundToClip());
  assert(runtime.applyPendingAiComponentCommand());
  assert(runtime.demoOverlayX() == -72);
  assert(runtime.applyAiComponentCommand(
      QStringLiteral("{\"operation\":\"setTransformNumber\",\"nodeId\":\"demo-box\",\"field\":\"x\",\"value\":-60}")));
  assert(runtime.demoOverlayX() == -60);
  assert(runtime.componentJson().value("root").toObject().value("children").toArray().at(0).toObject()
             .value("transform").toObject().value("x").toInt() == -60);
  assert(runtime.applyAiComponentCommand(
      QStringLiteral("{\"operation\":\"setProperty\",\"nodeId\":\"demo-text\",\"field\":\"text\",\"value\":\"AI 编辑\"}")));
  assert(runtime.demoOverlayText() == QStringLiteral("AI 编辑"));
  assert(runtime.applyAiComponentCommand(
      QStringLiteral("{\"operation\":\"setKeyframeValue\",\"nodeId\":\"demo-box\",\"field\":\"x\",\"frame\":24,\"value\":-24}")));
  assert(!runtime.applyAiComponentCommand(QStringLiteral("[\"not-a-command\"]")));
  assert(!runtime.applyAiComponentCommand(
      QStringLiteral("{\"operation\":\"eval\",\"nodeId\":\"demo-box\",\"field\":\"x\",\"value\":\"alert(1)\"}")));
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
  const auto projectPath = directory.path() + QStringLiteral("/project.edward.json");
  assert(runtime.saveProject(projectPath));
  edward::desktop::WorkbenchRuntime restoredRuntime;
  assert(restoredRuntime.loadProject(projectPath));
  assert(restoredRuntime.videoTrackCount() == 2);
  assert(restoredRuntime.clips().size() == 2);
  assert(restoredRuntime.demoOverlayX() == -180);
  assert(restoredRuntime.demoOverlayY() == -40);
  assert(restoredRuntime.demoOverlayWidth() == 300);
  assert(restoredRuntime.demoOverlayHeight() == 100);
  assert(restoredRuntime.demoOverlayOpacity() == 0.5);
  assert(restoredRuntime.demoOverlayScale() == 1.5);
  assert(restoredRuntime.demoOverlayRotation() == 30.0);
  assert(!restoredRuntime.componentJson().isEmpty());
  assert(restoredRuntime.componentBoundToClip());
  assert(restoredRuntime.aiConversation().isEmpty());
  QFile conversationProject(projectPath);
  assert(conversationProject.open(QIODevice::ReadOnly));
  QJsonParseError conversationError;
  auto conversationDocument = QJsonDocument::fromJson(conversationProject.readAll(), &conversationError);
  assert(conversationError.error == QJsonParseError::NoError && conversationDocument.isObject());
  auto conversationObject = conversationDocument.object();
  conversationObject.insert("aiConversation", QStringLiteral("用户：向左移动\nAI：setTransformNumber"));
  conversationProject.close();
  assert(conversationProject.open(QIODevice::WriteOnly | QIODevice::Truncate));
  const auto conversationBytes = QJsonDocument(conversationObject).toJson(QJsonDocument::Compact);
  assert(conversationProject.write(conversationBytes) == conversationBytes.size());
  conversationProject.close();
  assert(restoredRuntime.loadProject(projectPath));
  assert(restoredRuntime.aiConversation().contains(QStringLiteral("向左移动")));
  const auto conversationRoundTrip = directory.path() + QStringLiteral("/conversation-roundtrip.edward.json");
  assert(restoredRuntime.saveProject(conversationRoundTrip));
  QFile roundTrip(conversationRoundTrip);
  assert(roundTrip.open(QIODevice::ReadOnly));
  const auto roundTripObject = QJsonDocument::fromJson(roundTrip.readAll()).object();
  assert(roundTripObject.value("aiConversation").toString().contains(QStringLiteral("setTransformNumber")));
  assert(!restoredRuntime.loadProject(directory.path() + QStringLiteral("/missing.edward.json")));
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
  bool applied = false;
  QEventLoop applyLoop;
  QObject::connect(&runtime, &edward::desktop::WorkbenchRuntime::operationSucceeded,
                   [&applied, &applyLoop](const QString& message) {
                     if (message == QStringLiteral("插件动画已应用到时间线")) {
                       applied = true;
                       applyLoop.quit();
                     }
                   });
  const auto appliedPath = directory.path() + QStringLiteral("/plugin-component.mov");
  assert(runtime.applyInstalledPluginToTimeline(QStringLiteral("apply-12"), QStringLiteral("main"), appliedPath));
  QTimer::singleShot(15000, &applyLoop, &QEventLoop::quit);
  applyLoop.exec();
  assert(applied);
  assert(QFile::exists(appliedPath));
  assert(!runtime.demoOverlayEnabled());
  assert(runtime.clips().size() == 3);
  bool pluginExported = false;
  QEventLoop pluginExportLoop;
  QObject::connect(&runtime, &edward::desktop::WorkbenchRuntime::operationSucceeded,
                   [&pluginExported, &pluginExportLoop](const QString& message) {
                     if (message.startsWith(QStringLiteral("视频已导出："))) {
                       pluginExported = true;
                       pluginExportLoop.quit();
                     }
                   });
  const auto combinedPath = directory.path() + QStringLiteral("/plugin-component.mp4");
  assert(runtime.exportTimeline(combinedPath));
  QTimer::singleShot(15000, &pluginExportLoop, &QEventLoop::quit);
  pluginExportLoop.exec();
  assert(pluginExported && QFile::exists(combinedPath));
  runtime.clearInstalledPlugin();
  assert(!runtime.installedPluginAvailable());
  return 0;
}
