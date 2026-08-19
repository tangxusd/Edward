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
  assert(workbench.find("demoOverlayX") != std::string::npos);
  assert(preview.find("componentDragged") != std::string::npos);
  assert(preview.find("id: componentBounds\n            z: 2") != std::string::npos);
  assert(workbench.find("demoOverlayOpacity") != std::string::npos);
  assert(workbench.find("demoOverlayScale") != std::string::npos);
  assert(workbench.find("demoOverlayRotation") != std::string::npos);
  assert(workbench.find("demoOverlayText") != std::string::npos);
  assert(workbench.find("导入组件草稿") != std::string::npos);
  assert(workbench.find("loadComponentJson") != std::string::npos);
  assert(workbench.find("saveComponentJson") != std::string::npos);
  assert(workbench.find("loadComponentFile") != std::string::npos);
  assert(workbench.find("保存组件 JSON") != std::string::npos);
  assert(workbench.find("保存组件包") != std::string::npos);
  assert(workbench.find("saveComponentPackage") != std::string::npos);
  assert(workbench.find("signInWithSupabase") != std::string::npos);
  assert(workbench.find("已登录:") != std::string::npos);
  assert(workbench.find("上传组件包") != std::string::npos);
  assert(workbench.find("uploadCurrentComponent") != std::string::npos);
  assert(workbench.find("failureTimer") != std::string::npos);
  assert(workbench.find("successTimer") != std::string::npos);
  assert(workbench.find("从文件导入组件") != std::string::npos);
  assert(workbench.find("ApplicationWindow") != std::string::npos);
  assert(timeline.find("playheadChangedByUser") != std::string::npos);
  assert(timeline.find("selectClip") != std::string::npos);
  assert(timeline.find("onClicked") != std::string::npos);
  assert(timeline.find("splitRequested") != std::string::npos);
  assert(timeline.find("rippleDeleteRequested") != std::string::npos);
  assert(preview.find("预览窗") != std::string::npos);
  return 0;
}
