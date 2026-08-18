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
  assert(workbench.find("toggleDemoOverlay") != std::string::npos);
  assert(workbench.find("demoOverlayX") != std::string::npos);
  assert(preview.find("componentDragged") != std::string::npos);
  assert(workbench.find("demoOverlayOpacity") != std::string::npos);
  assert(workbench.find("demoOverlayText") != std::string::npos);
  assert(workbench.find("ApplicationWindow") != std::string::npos);
  assert(timeline.find("playheadChangedByUser") != std::string::npos);
  assert(timeline.find("selectClip") != std::string::npos);
  assert(timeline.find("onClicked") != std::string::npos);
  assert(timeline.find("splitRequested") != std::string::npos);
  assert(timeline.find("rippleDeleteRequested") != std::string::npos);
  assert(preview.find("预览窗") != std::string::npos);
  return 0;
}
