#pragma once

#include "edward/plugins/plugin_manifest.hpp"

#include <filesystem>
#include <optional>

namespace edward::plugins {

struct InstalledPlugin final {
  std::filesystem::path root;
  PluginManifest manifest;
};

std::optional<InstalledPlugin> loadInstalledPlugin(const std::filesystem::path& root,
                                                   QString* error = nullptr);

}  // namespace edward::plugins
