#pragma once

#include "edward/plugins/plugin_manifest.hpp"

#include <QString>

#include <filesystem>
#include <optional>

namespace edward::plugins {

struct PluginRuntimeResolution final {
  std::filesystem::path bundledRoot;
  QString expectedSha256;
  bool requireIntegrity = false;
  bool allowDevelopmentPath = false;
};

std::optional<QString> resolvePluginRuntime(const PluginManifest& manifest,
                                            const PluginRuntimeResolution& resolution,
                                            QString* error = nullptr);

}  // namespace edward::plugins
