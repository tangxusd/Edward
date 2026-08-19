#pragma once

#include "edward/plugins/plugin_manifest.hpp"
#include "edward/core/component_ir.hpp"

#include <filesystem>
#include <optional>

namespace edward::plugins {

struct InstalledPlugin final {
  std::filesystem::path root;
  PluginManifest manifest;
};

enum class PluginDependencyStatus { NotRequired, Available, Missing, VersionMismatch };

std::optional<InstalledPlugin> loadInstalledPlugin(const std::filesystem::path& root,
                                                   QString* error = nullptr);
PluginDependencyStatus dependencyStatus(const edward::core::ComponentIr& component,
                                        const std::optional<InstalledPlugin>& installedPlugin);

}  // namespace edward::plugins
