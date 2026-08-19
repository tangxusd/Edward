#include <edward/plugins/plugin_sandbox_policy.hpp>

#include <cassert>

int main() {
  const edward::plugins::PluginSandboxPaths paths{
      "/plugins/remotion", "/runtime/node", "/tasks/in", "/tasks/out", "/cache/chromium", "/tasks/chrome"};
  const auto profile = edward::plugins::buildMacosSeatbeltProfile(paths);
  assert(profile.startsWith("(version 1)"));
  assert(profile.contains("(deny default)"));
  assert(profile.contains("(allow file-read* (subpath \"/plugins/remotion\"))"));
  assert(profile.contains("(allow file-read* (subpath \"/runtime/node\"))"));
  assert(profile.contains("(allow file-read* (subpath \"/tasks/in\"))"));
  assert(profile.contains("(allow file-write* (subpath \"/tasks/out\"))"));
  assert(profile.contains("(allow file-write* (subpath \"/tasks/chrome\"))"));
  assert(!profile.contains("network-outbound"));
  assert(!profile.contains("/Users"));
  return 0;
}
