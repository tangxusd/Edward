#include <edward/desktop/workbench_runtime.hpp>
#include <edward/media/media_probe.hpp>

#include <QGuiApplication>
#include <QFile>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 4);
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication application(argc, argv);
  edward::desktop::WorkbenchRuntime runtime;
  assert(runtime.projectWindowTitle() == QStringLiteral("未命名项目 — 未保存更改"));
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
  const auto exportPath = directory.path() + QStringLiteral("/timeline-720p.mp4");
  bool exported = false;
  QEventLoop exportLoop;
  QObject::connect(&runtime, &edward::desktop::WorkbenchRuntime::operationSucceeded,
                   [&exported, &exportLoop](const QString& message) {
                     if (message.startsWith(QStringLiteral("视频已导出："))) {
                       exported = true;
                       exportLoop.quit();
                     }
                   });
  assert(!runtime.exportTimelineWithOptions(exportPath, 0, 720, 30, 0));
  assert(!runtime.exportTimelineWithOptions(QStringLiteral("relative-output.mp4"), 1280, 720, 30, 0));
  assert(!runtime.exportTimelineWithOptions(directory.path() + QStringLiteral("/timeline.mov"), 1280, 720, 30, 0));
  assert(!runtime.exportTimelineWithOptions(exportPath, 1280, 720, 30, 3));
  assert(runtime.exportTimelineWithOptions(exportPath, 1280, 720, 30, 1));
  QTimer::singleShot(15000, &exportLoop, &QEventLoop::quit);
  exportLoop.exec();
  assert(exported);
  assert(QFile::exists(exportPath));
  assert(runtime.timelineExportProgress() == 100);
  const auto exportedInfo = edward::media::MediaProbe::probe(std::filesystem::path(exportPath.toStdString()));
  assert(exportedInfo.has_value());
  assert(exportedInfo->width == 1280);
  assert(exportedInfo->height == 720);
  assert(exportedInfo->fpsNumerator == 30);
  assert(exportedInfo->fpsDenominator == 1);
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
  bool pluginComponentExported = false;
  QEventLoop pluginComponentExportLoop;
  QObject::connect(&runtime, &edward::desktop::WorkbenchRuntime::operationSucceeded,
                   [&pluginComponentExported, &pluginComponentExportLoop](const QString& message) {
                     if (message.startsWith(QStringLiteral("视频已导出："))) {
                       pluginComponentExported = true;
                       pluginComponentExportLoop.quit();
                     }
                   });
  const auto pluginComponentPath = directory.path() + QStringLiteral("/plugin-component.mp4");
  assert(runtime.exportTimeline(pluginComponentPath));
  QTimer::singleShot(15000, &pluginComponentExportLoop, &QEventLoop::quit);
  pluginComponentExportLoop.exec();
  assert(pluginComponentExported && QFile::exists(pluginComponentPath));
  assert(runtime.proposeAiComponentCommand(
      QStringLiteral("{\"operation\":\"setProperty\",\"nodeId\":\"root\",\"field\":\"opacity\",\"value\":0.5}")));
  assert(runtime.applyPendingAiComponentCommand());
  assert(runtime.componentJson().value("root").toObject().value("properties").toObject().value("opacity").toDouble() == 0.5);
  assert(!runtime.proposeAiComponentCommand(
      QStringLiteral("{\"operation\":\"setProperty\",\"nodeId\":\"root\",\"field\":\"fill\",\"value\":\"#ffffff\"}")));
  runtime.generateComponentDraft();
  const auto componentNodes = runtime.componentNodes();
  bool hasDemoBox = false;
  bool hasDemoText = false;
  for (const auto& nodeValue : componentNodes) {
    const auto node = nodeValue.toMap();
    hasDemoBox = hasDemoBox || node.value("id").toString() == QStringLiteral("demo-box");
    hasDemoText = hasDemoText || node.value("id").toString() == QStringLiteral("demo-text");
  }
  assert(hasDemoBox);
  assert(hasDemoText);
  assert(runtime.selectComponentNode(QStringLiteral("demo-text")));
  assert(runtime.selectedComponentNodeX() == 44);
  runtime.setSelectedComponentNodeX(80);
  runtime.setSelectedComponentNodeY(-30);
  assert(runtime.selectedComponentNodeX() == 80);
  assert(runtime.selectedComponentNodeY() == -30);
  runtime.setSelectedComponentNodeWidth(200);
  runtime.setSelectedComponentNodeHeight(40);
  assert(runtime.selectedComponentNodeWidth() == 200);
  assert(runtime.selectedComponentNodeHeight() == 40);
  runtime.setSelectedComponentNodeRotation(25.0);
  runtime.setSelectedComponentNodeOpacity(0.6);
  assert(runtime.selectedComponentNodeRotation() == 25.0);
  assert(runtime.selectedComponentNodeOpacity() == 0.6);
  runtime.setSelectedComponentNodeColor(QStringLiteral("#ff0000"));
  assert(runtime.selectedComponentNodeColor() == QStringLiteral("#ff0000"));
  assert(!runtime.selectedComponentNodeKeyframes().isEmpty());
  assert(runtime.removeSelectedComponentNodeKeyframe(QStringLiteral("color"), 0));
  for (const auto& keyframe : runtime.selectedComponentNodeKeyframes())
    assert(keyframe.toMap().value("field").toString() != QStringLiteral("color"));
  runtime.setSelectedComponentNodeColor(QStringLiteral("red"));
  assert(runtime.selectedComponentNodeColor() == QStringLiteral("#ff0000"));
  assert(runtime.selectComponentNode(QStringLiteral("demo-box")));
  assert(runtime.selectedComponentNodeType() == QStringLiteral("shape"));
  runtime.setSelectedComponentNodeBorderColor(QStringLiteral("#ffffff"));
  assert(runtime.selectedComponentNodeBorderColor() == QStringLiteral("#ffffff"));
  assert(runtime.selectComponentNode(QStringLiteral("demo-text")));
  runtime.setSelectedComponentNodeFontFamily(QStringLiteral("Arial"));
  assert(runtime.selectedComponentNodeFontFamily() == QStringLiteral("Arial"));
  runtime.setSelectedComponentNodeFontFamily(QStringLiteral("bad\nfont"));
  assert(runtime.selectedComponentNodeFontFamily() == QStringLiteral("Arial"));
  runtime.setDemoOverlayFontSize(31);
  assert(runtime.componentJson().value("root").toObject().value("children").toArray().at(1).toObject()
             .value("properties").toObject().value("fontSize").toInt() == 31);
  assert(runtime.selectComponentNode(QStringLiteral("demo-box")));
  runtime.setDemoOverlayBorderWidth(7);
  assert(runtime.componentJson().value("root").toObject().value("children").toArray().at(0).toObject()
             .value("properties").toObject().value("borderWidth").toInt() == 7);
  assert(!runtime.selectComponentNode(QStringLiteral("missing-node")));
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
  runtime.setDemoOverlayFontSize(28);
  runtime.setDemoOverlayBorderWidth(6);
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
      atPlayhead = atPlayhead || keyframe.toObject().value("frame").toInt() == 2;
    assert(atPlayhead);
  }
  assert(keyframes.value("opacity").toArray().last().toObject().value("value").toDouble() == 0.5);
  const auto transformedBox = runtime.componentJson().value("root").toObject().value("children").toArray().at(0).toObject();
  assert(transformedBox.value("transform").toObject().value("scaleX").toDouble() == 1.5);
  assert(transformedBox.value("transform").toObject().value("rotation").toDouble() == 30.0);
  const auto projectPath = directory.path() + QStringLiteral("/project.edward.json");
  runtime.setDemoOverlayText(QStringLiteral("手动保存基线"));
  assert(runtime.saveProject(projectPath));
  assert(runtime.projectWindowTitle() == QStringLiteral("project — 已保存"));
  runtime.setDemoOverlayText(QStringLiteral("自动保存草稿"));
  QEventLoop autosaveLoop;
  QTimer::singleShot(1500, &autosaveLoop, &QEventLoop::quit);
  autosaveLoop.exec();
  assert(runtime.hasProjectRecovery(projectPath));
  assert(runtime.projectWindowTitle().endsWith(QStringLiteral("— 已保存 · 刚刚自动保存")));
  QFile officialProject(projectPath);
  assert(officialProject.open(QIODevice::ReadOnly));
  const auto officialComponent = QJsonDocument::fromJson(officialProject.readAll()).object()
                                   .value("component").toObject().value("root").toObject()
                                   .value("children").toArray().at(1).toObject()
                                   .value("properties").toObject().value("text").toString();
  assert(officialComponent == QStringLiteral("手动保存基线"));
  edward::desktop::WorkbenchRuntime recoveredRuntime;
  assert(recoveredRuntime.recoverProject(projectPath));
  assert(recoveredRuntime.demoOverlayText() == QStringLiteral("自动保存草稿"));
  assert(recoveredRuntime.discardProjectRecovery(projectPath));
  assert(!recoveredRuntime.hasProjectRecovery(projectPath));
  runtime.setDemoOverlayText(QStringLiteral("手动保存基线"));
  edward::desktop::WorkbenchRuntime restoredRuntime;
  assert(restoredRuntime.loadProject(projectPath));
  assert(restoredRuntime.videoTrackCount() == 2);
  assert(restoredRuntime.clips().size() == 2);
  assert(restoredRuntime.demoOverlayX() == -180);
  assert(restoredRuntime.demoOverlayY() == -40);
  assert(restoredRuntime.demoOverlayWidth() == 300);
  assert(restoredRuntime.demoOverlayHeight() == 100);
  assert(restoredRuntime.demoOverlayOpacity() == 0.5);
  assert(restoredRuntime.demoOverlayFontSize() == 28);
  assert(restoredRuntime.demoOverlayBorderWidth() == 6);
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
  const auto clipsBeforeInvalidLoad = restoredRuntime.clips();
  const auto playheadBeforeInvalidLoad = restoredRuntime.playheadFrame();
  auto invalidProject = roundTripObject;
  auto invalidClips = invalidProject.value("clips").toArray();
  auto invalidClip = invalidClips.at(0).toObject();
  invalidClip.insert("source", QString());
  invalidClips.replace(0, invalidClip);
  invalidProject.insert("clips", invalidClips);
  const auto invalidProjectPath = directory.path() + QStringLiteral("/invalid.edward.json");
  QFile invalidProjectFile(invalidProjectPath);
  assert(invalidProjectFile.open(QIODevice::WriteOnly));
  const auto invalidProjectBytes = QJsonDocument(invalidProject).toJson(QJsonDocument::Compact);
  assert(invalidProjectFile.write(invalidProjectBytes) == invalidProjectBytes.size());
  invalidProjectFile.close();
  assert(!restoredRuntime.loadProject(invalidProjectPath));
  assert(restoredRuntime.clips() == clipsBeforeInvalidLoad);
  assert(restoredRuntime.playheadFrame() == playheadBeforeInvalidLoad);
  const auto savedPath = directory.path() + QStringLiteral("/component.json");
  assert(runtime.saveComponentJson(savedPath));
  const auto packagePath = directory.path() + QStringLiteral("/package");
  assert(runtime.saveComponentPackage(packagePath, QStringLiteral("demo.component"), QStringLiteral("Demo component")));
  assert(QFile::exists(packagePath + QStringLiteral("/manifest.json")));
  assert(QFile::exists(packagePath + QStringLiteral("/component.json")));
  const auto libraryPath = directory.path() + QStringLiteral("/library");
  assert(runtime.configureComponentLibrary(libraryPath));
  assert(runtime.saveCurrentComponentToLibrary(QStringLiteral("demo.library"), QStringLiteral("Library demo")));
  assert(runtime.localComponents().size() == 1);
  assert(QFile::exists(libraryPath + QStringLiteral("/demo.library/manifest.json")));
  assert(QFile::exists(libraryPath + QStringLiteral("/demo.library/component.json")));
  assert(runtime.loadLibraryComponent(QStringLiteral("demo.library")));
  assert(!runtime.componentBoundToClip());
  assert(runtime.addCurrentComponentToTimeline(60));
  assert(!runtime.demoOverlayEnabled());
  assert(runtime.clips().last().toMap().value("kind") == QStringLiteral("component"));
  const auto componentTimelineProject = directory.path() + QStringLiteral("/component-timeline.edward.json");
  assert(runtime.saveProject(componentTimelineProject));
  edward::desktop::WorkbenchRuntime componentTimelineRuntime;
  assert(componentTimelineRuntime.loadProject(componentTimelineProject));
  assert(componentTimelineRuntime.clips().size() == runtime.clips().size());
  assert(componentTimelineRuntime.clips().last().toMap().value("kind") == QStringLiteral("component"));
  assert(!componentTimelineRuntime.demoOverlayEnabled());
  const auto componentClipId = componentTimelineRuntime.clips().last().toMap().value("id").toLongLong();
  assert(componentTimelineRuntime.selectClip(componentClipId));
  assert(componentTimelineRuntime.demoOverlayEnabled());
  assert(componentTimelineRuntime.moveSelected(30));
  assert(componentTimelineRuntime.setPlayhead(35));
  componentTimelineRuntime.setDemoOverlayX(-135);
  const auto localTimeBox = componentTimelineRuntime.componentJson().value("root").toObject()
                                .value("children").toArray().at(0).toObject();
  bool hasLocalFrame = false;
  for (const auto& keyframe : localTimeBox.value("keyframes").toObject().value("x").toArray())
    hasLocalFrame = hasLocalFrame || keyframe.toObject().value("frame").toInt(-1) == 5;
  assert(hasLocalFrame);
  const auto beforeOutOfRangeEdit = componentTimelineRuntime.componentJson();
  assert(componentTimelineRuntime.setPlayhead(29));
  componentTimelineRuntime.setDemoOverlayX(-200);
  assert(componentTimelineRuntime.componentJson() == beforeOutOfRangeEdit);
  assert(componentTimelineRuntime.setPlayhead(35));
  componentTimelineRuntime.setDemoOverlayText(QStringLiteral("编辑后的组件"));
  assert(componentTimelineRuntime.demoOverlayText() == QStringLiteral("编辑后的组件"));
  assert(componentTimelineRuntime.saveProject(componentTimelineProject));
  edward::desktop::WorkbenchRuntime editedComponentRuntime;
  assert(editedComponentRuntime.loadProject(componentTimelineProject));
  assert(editedComponentRuntime.selectClip(componentClipId));
  assert(editedComponentRuntime.demoOverlayText() == QStringLiteral("编辑后的组件"));
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
  assert(runtime.clips().size() == 4);
  bool pluginExported = false;
  QEventLoop pluginExportLoop;
  QObject::connect(&runtime, &edward::desktop::WorkbenchRuntime::operationSucceeded,
                   [&pluginExported, &pluginExportLoop](const QString& message) {
                     if (message.startsWith(QStringLiteral("视频已导出："))) {
                       pluginExported = true;
                       pluginExportLoop.quit();
                     }
                   });
  const auto combinedPath = directory.path() + QStringLiteral("/timeline-with-plugin.mp4");
  assert(runtime.exportTimeline(combinedPath));
  QTimer::singleShot(15000, &pluginExportLoop, &QEventLoop::quit);
  pluginExportLoop.exec();
  assert(pluginExported && QFile::exists(combinedPath));
  runtime.clearInstalledPlugin();
  assert(!runtime.installedPluginAvailable());
  edward::desktop::WorkbenchRuntime waveformRuntime;
  bool waveformReady = false;
  QEventLoop waveformLoop;
  QObject::connect(&waveformRuntime, &edward::desktop::WorkbenchRuntime::timelineChanged,
                   [&waveformRuntime, &waveformReady, &waveformLoop] {
                     const auto clips = waveformRuntime.clips();
                     if (!clips.empty() && clips.front().toMap().value("waveform").toList().size() == 96) {
                       waveformReady = true;
                       waveformLoop.quit();
                     }
                   });
  assert(waveformRuntime.importMedia(QString::fromLocal8Bit(argv[3])));
  QTimer::singleShot(5000, &waveformLoop, &QEventLoop::quit);
  waveformLoop.exec();
  assert(waveformReady);
  assert(waveformRuntime.clips().front().toMap().value("hasAudio").toBool());

  edward::desktop::WorkbenchRuntime thumbnailRuntime;
  bool thumbnailReady = false;
  QEventLoop thumbnailLoop;
  QObject::connect(&thumbnailRuntime, &edward::desktop::WorkbenchRuntime::timelineChanged,
                   [&thumbnailRuntime, &thumbnailReady, &thumbnailLoop] {
                     const auto clips = thumbnailRuntime.clips();
                     if (!clips.empty() && clips.front().toMap().value("thumbnail").toString().startsWith("image://edward/clip-")) {
                       thumbnailReady = true;
                       thumbnailLoop.quit();
                     }
                   });
  assert(thumbnailRuntime.importMedia(QString::fromLocal8Bit(argv[2])));
  QTimer::singleShot(5000, &thumbnailLoop, &QEventLoop::quit);
  thumbnailLoop.exec();
  assert(thumbnailReady);

  QFile transitionSource(projectPath);
  assert(transitionSource.open(QIODevice::ReadOnly));
  QJsonParseError transitionError;
  auto transitionDocument = QJsonDocument::fromJson(transitionSource.readAll(), &transitionError);
  assert(transitionError.error == QJsonParseError::NoError && transitionDocument.isObject());
  auto transitionProject = transitionDocument.object();
  auto transitionClips = transitionProject.value("clips").toArray();
  auto transitionLeft = transitionClips.at(0).toObject();
  auto transitionRight = transitionClips.at(1).toObject();
  transitionLeft.insert("sourceOut", 10);
  transitionLeft.insert("trackId", 1);
  transitionRight.insert("sourceIn", 0);
  transitionRight.insert("sourceOut", 10);
  transitionRight.insert("timelineStart", 10);
  transitionRight.insert("trackId", 1);
  transitionClips.replace(0, transitionLeft);
  transitionClips.replace(1, transitionRight);
  transitionProject.insert("clips", transitionClips);
  const auto transitionPath = directory.path() + QStringLiteral("/transition-project.edward.json");
  QFile transitionInput(transitionPath);
  assert(transitionInput.open(QIODevice::WriteOnly));
  assert(transitionInput.write(QJsonDocument(transitionProject).toJson()) > 0);
  transitionInput.close();
  edward::desktop::WorkbenchRuntime transitionRuntime;
  assert(transitionRuntime.loadProject(transitionPath));
  assert(transitionRuntime.selectClip(transitionLeft.value("id").toInteger()));
  assert(transitionRuntime.addTransitionToSelected(QStringLiteral("flash_white")));
  const auto visibleTransitions = transitionRuntime.transitions();
  assert(visibleTransitions.size() == 1);
  const auto visibleTransition = visibleTransitions.front().toMap();
  assert(visibleTransition.value("type").toString() == QStringLiteral("flash_white"));
  assert(visibleTransition.value("startFrame").toLongLong() == 0);
  assert(visibleTransition.value("durationFrames").toLongLong() == 10);
  assert(!transitionRuntime.addDissolveToSelected());
  const auto transitionOutput = directory.path() + QStringLiteral("/transition-output.edward.json");
  assert(transitionRuntime.saveProject(transitionOutput));
  QFile transitionSaved(transitionOutput);
  assert(transitionSaved.open(QIODevice::ReadOnly));
  const auto savedTransitions = QJsonDocument::fromJson(transitionSaved.readAll()).object().value("transitions").toArray();
  assert(savedTransitions.size() == 1);
  assert(savedTransitions.at(0).toObject().value("type").toString() == QStringLiteral("flash_white"));
  return 0;
}
