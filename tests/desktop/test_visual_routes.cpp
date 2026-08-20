#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

static std::string read(const std::filesystem::path& path) {
  std::ifstream stream(path);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

int main() {
  const std::filesystem::path root = EDWARD_DESKTOP_QML_DIR;
  const auto workbench = read(root / "Workbench.qml");
  const auto timeline = read(root / "EdwardTimeline.qml");
  const auto preview = read(root / "EdwardPreview.qml");
  assert(workbench.find("EdwardPreview") != std::string::npos);
  assert(workbench.find("EdwardTimeline") != std::string::npos);
  assert(workbench.find("generateComponentDraft") != std::string::npos);
  assert(workbench.find("aiConversation") != std::string::npos);
  assert(workbench.find("demoOverlayX") != std::string::npos);
  assert(preview.find("componentDragged") != std::string::npos);
  assert(preview.find("id: componentBounds\n            z: 2") != std::string::npos);
  assert(preview.find("property bool hasFrame") != std::string::npos);
  assert(preview.find("root.hasFrame ? \"image://edward") != std::string::npos);
  assert(workbench.find("hasFrame: workbenchRuntime.clips.length > 0") != std::string::npos);
  assert(workbench.find("demoOverlayOpacity") != std::string::npos);
  assert(workbench.find("demoOverlayScale") != std::string::npos);
  assert(workbench.find("demoOverlayRotation") != std::string::npos);
  assert(workbench.find("demoOverlayText") != std::string::npos);
  assert(workbench.find("bindComponentToSelectedClip") != std::string::npos);
  assert(workbench.find("addCurrentComponentToTimeline") != std::string::npos);
  assert(workbench.find("导入组件草稿") != std::string::npos);
  assert(workbench.find("loadComponentJson") != std::string::npos);
  assert(workbench.find("saveComponentJson") != std::string::npos);
  assert(workbench.find("loadComponentFile") != std::string::npos);
  assert(workbench.find("保存组件 JSON") != std::string::npos);
  assert(workbench.find("保存组件包") != std::string::npos);
  assert(workbench.find("saveComponentPackage") != std::string::npos);
  assert(workbench.find("saveCurrentComponentToLibrary") != std::string::npos);
  assert(workbench.find("保存到我的组件库") != std::string::npos);
  assert(workbench.find("我的组件") != std::string::npos);
  assert(workbench.find("loadLibraryComponent") != std::string::npos);
  assert(workbench.find("saveProject") != std::string::npos);
  assert(workbench.find("loadProject") != std::string::npos);
  assert(workbench.find("hasProjectRecovery") != std::string::npos);
  assert(workbench.find("发现自动保存副本") != std::string::npos);
  assert(workbench.find("恢复自动保存") != std::string::npos);
  assert(workbench.find("导出视频") != std::string::npos);
  assert(workbench.find("exportTimeline") != std::string::npos);
  assert(workbench.find("signInWithSupabase") != std::string::npos);
  assert(workbench.find("已登录:") != std::string::npos);
  assert(workbench.find("上传组件包") != std::string::npos);
  assert(workbench.find("uploadCurrentComponent") != std::string::npos);
  assert(workbench.find("failureTimer") != std::string::npos);
  assert(workbench.find("successTimer") != std::string::npos);
  assert(workbench.find("从文件导入组件") != std::string::npos);
  assert(workbench.find("ApplicationWindow") != std::string::npos);
  assert(workbench.find("videoTrackCount: workbenchRuntime.videoTrackCount") != std::string::npos);
  assert(workbench.find("selectedVideoTrackIndex: workbenchRuntime.selectedVideoTrackIndex") != std::string::npos);
  assert(workbench.find("importedMediaClips") != std::string::npos);
  assert(workbench.find("已导入素材") != std::string::npos);
  assert(timeline.find("modelData.waveform") != std::string::npos);
  assert(timeline.find("modelData.hasAudio") != std::string::npos);
  assert(timeline.find("modelData.thumbnail") != std::string::npos);
  assert(timeline.find("Canvas {") != std::string::npos);
  assert(workbench.find("durationFrames: workbenchRuntime.timelineDurationFrames") != std::string::npos);
  assert(timeline.find("playheadChangedByUser") != std::string::npos);
  assert(timeline.find("selectClip") != std::string::npos);
  assert(timeline.find("onClicked") != std::string::npos);
  assert(timeline.find("splitRequested") != std::string::npos);
  assert(timeline.find("rippleDeleteRequested") != std::string::npos);
  assert(timeline.find("drag.target: parent") != std::string::npos);
  assert(timeline.find("moveSelected") != std::string::npos);
  assert(timeline.find("undoTimeline") != std::string::npos);
  assert(timeline.find("redoTimeline") != std::string::npos);
  assert(timeline.find("addVideoTrack") != std::string::npos);
  assert(timeline.find("removeEmptyVideoTrack") != std::string::npos);
  assert(timeline.find("Keys.onSpacePressed") != std::string::npos);
  assert(timeline.find("Qt.Key_Delete") != std::string::npos);
  assert(timeline.find("Qt.ShiftModifier") != std::string::npos);
  assert(timeline.find("trimSelectedLeft") != std::string::npos);
  assert(timeline.find("trimSelectedRight") != std::string::npos);
  assert(timeline.find("cursorShape: Qt.SizeHorCursor") != std::string::npos);
  assert(timeline.find("onReleased: workbenchRuntime.trimSelectedLeft()") != std::string::npos);
  assert(timeline.find("onReleased: workbenchRuntime.trimSelectedRight()") != std::string::npos);
  assert(timeline.find("anchors.leftMargin: root.rulerWidth") != std::string::npos);
  assert(timeline.find("function frameAtX(x)") != std::string::npos);
  assert(timeline.find("function snapFrame(frame, clipId)") != std::string::npos);
  assert(timeline.find("root.snapFrame(") != std::string::npos);
  assert(timeline.find("onDoubleClicked: root.playheadChangedByUser(modelData.timelineStart)") != std::string::npos);
  assert(timeline.find("function formatTimecode(frame)") != std::string::npos);
  assert(timeline.find("property real zoomFactor") != std::string::npos);
  assert(timeline.find("property int viewStartFrame") != std::string::npos);
  assert(timeline.find("WheelHandler") != std::string::npos);
  assert(timeline.find("root.formatTimecode") != std::string::npos);
  assert(timeline.find("modelData.selected && parent.width > 92") != std::string::npos);
  assert(timeline.find("timelineStart + modelData.sourceOut - modelData.sourceIn") != std::string::npos);
  assert(timeline.find("togglePlayback") != std::string::npos);
  assert(timeline.find("property int videoTrackCount") != std::string::npos);
  assert(timeline.find("root.videoTrackCount + 1") != std::string::npos);
  assert(timeline.find("signal videoTrackSelected") != std::string::npos);
  assert(preview.find("预览窗") != std::string::npos);
  return 0;
}
