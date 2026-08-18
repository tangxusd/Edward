#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: test_dependency_manifest <sources.json>\n";
    return EXIT_FAILURE;
  }
  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "cannot open dependency manifest\n";
    return EXIT_FAILURE;
  }
  const std::string text((std::istreambuf_iterator<char>(input)), {});
  const bool valid =
      contains(text, "https://github.com/mltframework/shotcut.git") &&
      contains(text, "v26.8.1") &&
      contains(text, "0474a712131fe1a82d499a32f3d54be956d2963f") &&
      contains(text, "GPL-3.0-or-later") &&
      contains(text, "timeline clip model") &&
      contains(text, "Shotcut QML user interface") &&
      contains(text, "\"importedFiles\": []");
  if (!valid) {
    std::cerr << "Shotcut dependency manifest is incomplete\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
