#include <fstream>
#include <cassert>
#include <string>

int main(int argc, char** argv) {
  assert(argc == 2);
  std::ifstream file(argv[1]);
  assert(file);
  const std::string schema((std::istreambuf_iterator<char>(file)), {});
  assert(schema.find("resourceId") != std::string::npos);
  assert(schema.find("displayName") != std::string::npos);
  assert(schema.find("assets") != std::string::npos);
  assert(schema.find("uniqueItems") != std::string::npos);
  assert(schema.find("(?!/)") != std::string::npos);
  return 0;
}
